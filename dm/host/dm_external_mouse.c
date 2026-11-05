// External pointer bridge shared by iPadOS and Android. It deliberately does
// not synthesize UITouch/MotionEvent finger input or change controller mode.
// macOS keeps its already tested live mouse path in touch_mac.c.
#include "dm_external_mouse.h"
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IOS)
#include "dm_world.h"
#include "dm_cursor.h"
#include "dm_uiscreen.h"
#include "dm_input.h"
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

uint64_t now_ns(void);
int dm_game_paused(void);
uint64_t dm_padmode_legend_click(float, float);
int dm_padmode_hud_click(float, float, uint64_t *);

typedef struct { int op, arg; float ux, uy, wx, wy; } mouse_event;
static pthread_mutex_t mouse_mu = PTHREAD_MUTEX_INITIALIZER;
static mouse_event mouse_queue[128];
static unsigned mouse_head, mouse_tail;
static int mouse_overflow;
static _Atomic int mouse_active, mouse_right, mouse_right_latch, mouse_scroll;
static _Atomic uint64_t mouse_scroll_until, mouse_hud_button, mouse_hud_until;
static int mouse_down, mouse_world, mouse_ui, mouse_eat, mouse_repeat;
static unsigned mouse_seq;
static float mouse_x, mouse_y, mouse_tap_x, mouse_tap_y;
static uint64_t mouse_last_walk;

int dm_external_mouse_active(void) { return atomic_load(&mouse_active); }
void dm_external_mouse_event(int op, float ux, float uy, float wx, float wy, int arg) {
    if (op < DM_MOUSE_DOWN || op > DM_MOUSE_WHEEL) return;
    if (!isfinite(ux) || !isfinite(uy) || !isfinite(wx) || !isfinite(wy)) op = DM_MOUSE_LEAVE;
    mouse_event e = {op, arg, ux, uy, wx, wy};
    pthread_mutex_lock(&mouse_mu);
    // Hover/drag storms never crowd out a button release. Preserve every
    // down/up edge even when the display thread is slower than the mouse.
    if ((op == DM_MOUSE_HOVER || op == DM_MOUSE_DRAG) && mouse_tail != mouse_head &&
        mouse_queue[(mouse_tail - 1) % 128].op == op)
        mouse_queue[(mouse_tail - 1) % 128] = e;
    else if (mouse_tail - mouse_head == 128) {
        mouse_overflow = 1; mouse_head = mouse_tail = 0;
    } else mouse_queue[mouse_tail++ % 128] = e;
    pthread_mutex_unlock(&mouse_mu);
}
static void mouse_clear(void) {
    if (mouse_ui) dm_ui_touch(3, 0, 0);
    mouse_down = mouse_world = mouse_ui = mouse_eat = mouse_repeat = 0;
    mouse_seq = 0;
    dm_world_mouse_stop();
    atomic_store(&mouse_active, 0);
    atomic_store(&mouse_right, 0);
    atomic_store(&mouse_right_latch, 0);
    atomic_store(&mouse_scroll, 0);
    atomic_store(&mouse_hud_button, 0);
    dm_world_hover_end();
}
static int mouse_world_ok(void) {
    return dm_uiscreen_ingame() && !dm_uiscreen_panel() && !dm_game_paused();
}
static int mouse_over_world(float ux, float uy, float wx, float wy) {
    if (!mouse_world_ok() || wx < 0 || wx > 1 || wy < 0 || wy > 1) return 0;
    // In true fullscreen the upper 3D area lies outside the UI rectangle.
    return ux < 0 || ux > 1 || uy < 0 || uy > 1 || dm_ui_item_at_now(ux, uy) == 3;
}
static void mouse_hover_at(const mouse_event *e) {
    atomic_store(&mouse_active, 1);
    if (mouse_down) return;
    if (mouse_over_world(e->ux, e->uy, e->wx, e->wy)) {
        dm_world_hover_move(e->wx, e->wy);
    } else {
        dm_world_hover_end();
        if (e->ux >= 0 && e->ux <= 1 && e->uy >= 0 && e->uy <= 1) dm_ui_mouse_hover_now(e->ux, e->uy);
    }
}
void dm_external_mouse_step(void) {
    mouse_event batch[128]; unsigned n = 0;
    pthread_mutex_lock(&mouse_mu);
    int overflow = mouse_overflow; mouse_overflow = 0;
    while (mouse_head != mouse_tail && n < 128) batch[n++] = mouse_queue[mouse_head++ % 128];
    pthread_mutex_unlock(&mouse_mu);
    if (overflow) mouse_clear();
    for (unsigned i = 0; i < n; i++) {
        mouse_event *e = &batch[i];
        if (e->op == DM_MOUSE_LEAVE) { mouse_clear(); continue; }
        if (e->op == DM_MOUSE_RIGHT_DOWN) {
            atomic_store(&mouse_right, 1); atomic_store(&mouse_right_latch, 1); continue;
        }
        if (e->op == DM_MOUSE_RIGHT_UP) { atomic_store(&mouse_right, 0); continue; }
        if (e->op == DM_MOUSE_WHEEL) {
            if (e->arg) {
                atomic_store(&mouse_scroll, e->arg > 0 ? 1 : -1);
                atomic_store(&mouse_scroll_until, now_ns() + 120000000ull);
            }
            continue;
        }
        if (e->op == DM_MOUSE_HOVER) { mouse_hover_at(e); continue; }
        atomic_store(&mouse_active, 1);
        if (e->op == DM_MOUSE_DOWN) {
            // Start a new gesture; never let a previous held-click leak into it.
            if (mouse_ui) dm_ui_touch(3, 0, 0);
            mouse_down = 1; mouse_world = mouse_ui = mouse_eat = mouse_repeat = 0; mouse_seq = 0;
            mouse_x = mouse_tap_x = e->wx; mouse_y = mouse_tap_y = e->wy;
            dm_world_hover_end();
            uint64_t legend = dm_padmode_legend_click(e->ux, e->uy), hud = 0;
            int hk = !legend && mouse_world_ok() ? dm_padmode_hud_click(e->ux, e->uy, &hud) : 0;
            if (legend || hk) {
                mouse_eat = 1;
                if (legend || (hk == 1 && hud)) {
                    atomic_store(&mouse_hud_button, legend ? legend : hud);
                    atomic_store(&mouse_hud_until, legend ? now_ns() + 120000000ull : UINT64_MAX);
                }
            } else if (mouse_over_world(e->ux, e->uy, e->wx, e->wy)) {
                mouse_world = 1;
                mouse_seq = dm_world_click(e->wx, e->wy);
            } else if (e->ux >= 0 && e->ux <= 1 && e->uy >= 0 && e->uy <= 1) {
                mouse_ui = 1; dm_ui_touch(0, e->ux, e->uy);
            }
            continue;
        }
        if (e->op == DM_MOUSE_DRAG) {
            mouse_x = e->wx; mouse_y = e->wy;
            if (mouse_down && mouse_ui) dm_ui_touch(1, e->ux, e->uy);
            else if (!mouse_down) mouse_hover_at(e);
            continue;
        }
        if (e->op == DM_MOUSE_UP) {
            mouse_x = e->wx; mouse_y = e->wy;
            if (mouse_ui) dm_ui_touch(2, e->ux, e->uy);
            if (mouse_eat && atomic_load(&mouse_hud_until) == UINT64_MAX)
                atomic_store(&mouse_hud_until, now_ns() + 100000000ull);
            mouse_down = mouse_world = mouse_ui = mouse_eat = 0;
            mouse_hover_at(e);
        }
    }
    if (!mouse_world_ok()) {
        mouse_down = mouse_world = mouse_repeat = 0; mouse_seq = 0;
        dm_world_mouse_stop();
    }
    // The game's world-tap answer is published at the end of dm_world_pump.
    // A completed click on empty ground is enough to initiate a single walk;
    // holding the button keeps the target updated, as on macOS.
    if (mouse_seq) {
        int result = dm_world_tap_result(mouse_seq);
        if (result >= 0) {
            mouse_repeat = result == 0 || result == 6;
            mouse_seq = 0; mouse_last_walk = now_ns();
            if (result == 0 && mouse_world_ok()) dm_world_walk(mouse_tap_x, mouse_tap_y);
        }
    }
    uint64_t now = now_ns();
    if (mouse_world && mouse_down && mouse_repeat && mouse_world_ok() && now - mouse_last_walk >= 33333333ull) {
        mouse_last_walk = now; dm_world_walk(mouse_x, mouse_y);
    }
}
void dm_external_mouse_apply(uint64_t *buttons, int32_t *rx, int32_t *ry) {
    (void)rx;
    int latched = atomic_exchange(&mouse_right_latch, 0);
    if (atomic_load(&mouse_right) || latched) *buttons |= DM_BTN_B;
    uint64_t now = now_ns();
    uint64_t held = atomic_load(&mouse_hud_button), until = atomic_load(&mouse_hud_until);
    if (held) {
        if (now < until) *buttons |= held;
        else atomic_store(&mouse_hud_button, 0);
    }
    int wheel = atomic_load(&mouse_scroll);
    if (wheel && now < atomic_load(&mouse_scroll_until)) { if (!*ry) *ry = wheel * 32767; }
    else if (wheel) atomic_store(&mouse_scroll, 0);
}

#else
// Other ports (including macOS and HarmonyOS) retain their current input path.
// Keeping the stub symbols here avoids cross-platform linker regressions.
int dm_external_mouse_active(void) { return 0; }
void dm_external_mouse_event(int op, float ux, float uy, float wx, float wy, int arg) {
    (void)op; (void)ux; (void)uy; (void)wx; (void)wy; (void)arg;
}
void dm_external_mouse_step(void) {}
void dm_external_mouse_apply(uint64_t *buttons, int32_t *rx, int32_t *ry) {
    (void)buttons; (void)rx; (void)ry;
}
#endif
