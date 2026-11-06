// External pointer bridge shared by iPadOS and Android. It deliberately does
// not synthesize UITouch/MotionEvent finger input or change controller mode.
// macOS keeps its already tested live mouse path in touch_mac.c.
#include "dm_external_mouse.h"
#include "dm_kbmap.h"
#include "dm_pc_input.h"
#include "dm_pc_profile.h"
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
static mouse_event mouse_last_hover;
static int mouse_hover_valid;
static int mouse_right_item;
static int g_mouse_physical_role[3]={0,1,2};
static int g_mouse_physical_down[3];

int dm_external_mouse_active(void) { return atomic_load(&mouse_active); }
void dm_external_mouse_event(int op, float ux, float uy, float wx, float wy, int arg) {
    if (op < DM_MOUSE_DOWN || op > DM_MOUSE_MIDDLE_UP) return;
    if (!isfinite(ux) || !isfinite(uy) || !isfinite(wx) || !isfinite(wy)) op = DM_MOUSE_LEAVE;
    pthread_mutex_lock(&mouse_mu);
    // Translate physical button roles once at press time; release uses the
    // latched action even if the user rebinds while holding the button.
    int button=-1,press=0;
    if(op==DM_MOUSE_DOWN){button=0;press=1;}
    else if(op==DM_MOUSE_RIGHT_DOWN){button=1;press=1;}
    else if(op==DM_MOUSE_MIDDLE_DOWN){button=2;press=1;}
    else if(op==DM_MOUSE_UP){button=0;}
    else if(op==DM_MOUSE_RIGHT_UP){button=1;}
    else if(op==DM_MOUSE_MIDDLE_UP){button=2;}
    if(op==DM_MOUSE_LEAVE) memset(g_mouse_physical_down,0,sizeof g_mouse_physical_down);
    if(button>=0) {
        if(press) {
            g_mouse_physical_role[button]=dm_kbmap_mouse_action((unsigned)button);
            g_mouse_physical_down[button]=1;
        } else if(!g_mouse_physical_down[button]) {
            pthread_mutex_unlock(&mouse_mu);return;
        } else g_mouse_physical_down[button]=0;
        int role=g_mouse_physical_role[button];
        if(role<0 || role>2){pthread_mutex_unlock(&mouse_mu);return;}
        if(press)op=role==0?DM_MOUSE_DOWN:role==1?DM_MOUSE_RIGHT_DOWN:DM_MOUSE_MIDDLE_DOWN;
        else {
            if(role==2){pthread_mutex_unlock(&mouse_mu);return;}
            op=role==0?DM_MOUSE_UP:DM_MOUSE_RIGHT_UP;
        }
    }
    mouse_event e = {op, arg, ux, uy, wx, wy};
    // Hover/drag storms never crowd out a button release. Preserve every
    // down/up edge even when the display thread is slower than the mouse.
    if ((op == DM_MOUSE_HOVER || op == DM_MOUSE_DRAG) && mouse_tail != mouse_head &&
        mouse_queue[(mouse_tail - 1) % 128].op == op)
        mouse_queue[(mouse_tail - 1) % 128] = e;
    else if (mouse_tail - mouse_head == 128) {
        // A burst of hover/drag events may overflow the queue. Drop the
        // gesture atomically including its physical-button latches so the
        // next genuine press is not mistaken for a still-held old press.
        mouse_overflow = 1; mouse_head = mouse_tail = 0;
        memset(g_mouse_physical_down,0,sizeof g_mouse_physical_down);
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
    mouse_hover_valid=0; mouse_right_item=0;
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
        // UIKit can end hover recognition on a mouse press. Do not discard
        // the pending world click or terminate an active mouse drag.
        if (e->op == DM_MOUSE_HOVER_END) {
            // Indirect-pointer presses in iOS/Android panels are handled by
            // the native item state machine, but still own the mouse session.
            atomic_store(&mouse_active,1);
            mouse_hover_valid=0; dm_world_hover_end(); continue;
        }
        if (e->op == DM_MOUSE_MIDDLE_DOWN) {
            int source=(dm_kbmap_pc_mode() && dm_uiscreen_ingame())?dm_pc_profile_file_status():0;
            if(source==1)(void)dm_pc_input_mouse_special(0x100);
            else if(source==0)dm_world_mouse_map_toggle();
            continue;
        }
        if (e->op == DM_MOUSE_RIGHT_DOWN) {
            atomic_store(&mouse_active,1); // Right-click alone must run the native use-item pump.
            mouse_right_item=0;
            if (dm_uiscreen_panel() && e->ux>=0 && e->ux<=1 &&
                e->uy>=0 && e->uy<=1 && dm_ui_item_at_now(e->ux,e->uy)==1) {
                mouse_right_item=1; atomic_store(&mouse_right,0);
                atomic_store(&mouse_right_latch,0);
                dm_ui_mouse_right_use(e->ux,e->uy); continue;
            }
            atomic_store(&mouse_right,1); atomic_store(&mouse_right_latch,1); continue;
        }
        if (e->op == DM_MOUSE_RIGHT_UP) { mouse_right_item=0; atomic_store(&mouse_right,0); continue; }
        if (e->op == DM_MOUSE_WHEEL) {
            if (e->arg) {
                int source=(dm_kbmap_pc_mode() && dm_uiscreen_ingame())?dm_pc_profile_file_status():0;
                int direction=source==1?dm_pc_input_wheel(e->arg):
                    source==0?(e->arg>0?1:-1):0;
                atomic_store(&mouse_scroll,direction);
                atomic_store(&mouse_scroll_until, direction?
                    now_ns()+120000000ull:0);
            }
            continue;
        }
        if (e->op == DM_MOUSE_HOVER) {
            mouse_last_hover=*e; mouse_hover_valid=1;
            mouse_hover_at(e); continue;
        }
        atomic_store(&mouse_active, 1);
        if (e->op == DM_MOUSE_DOWN) {
            // Start a new gesture; never let a previous held-click leak into it.
            if (mouse_ui) dm_ui_touch(3, 0, 0);
            mouse_down = 1; mouse_world = mouse_ui = mouse_eat = mouse_repeat = 0; mouse_seq = 0;
            mouse_x = mouse_tap_x = e->wx; mouse_y = mouse_tap_y = e->wy;
            dm_world_hover_end();
            mouse_hover_valid=0;
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
            dm_world_mouse_release();
            mouse_x = e->wx; mouse_y = e->wy;
            if (mouse_ui) dm_ui_touch(2, e->ux, e->uy);
            if (mouse_eat && atomic_load(&mouse_hud_until) == UINT64_MAX)
                atomic_store(&mouse_hud_until, now_ns() + 100000000ull);
            mouse_down = mouse_world = mouse_ui = mouse_eat = 0;
            mouse_last_hover=*e; mouse_hover_valid=1;
            mouse_hover_at(e);
        }
    }
    if (!mouse_world_ok()) {
        // Closing a world session stops walk/attack immediately. A pressed
        // inventory/stash/equipment mouse button is different: it must stay
        // held across frames until the corresponding mouse-up or cancel.
        // Clearing mouse_down here used to drop every inventory drag after
        // just one frame, silently converting later moves into hover.
        mouse_world = mouse_repeat = 0; mouse_seq = 0;
        if (!mouse_ui) mouse_down = 0;
        dm_world_mouse_stop();
    }
    // Re-evaluate stationary pointer hover across panel open/close, without
    // inventing a finger touch or changing physical mouse-button state.
    if (mouse_hover_valid && !mouse_down && atomic_load(&mouse_active))
        mouse_hover_at(&mouse_last_hover);
    // The game's world-tap answer is published at the end of dm_world_pump.
    // A completed click on empty ground is enough to initiate a single walk;
    // holding the button keeps the target updated, as on macOS.
    if (mouse_seq) {
        int result = dm_world_tap_result(mouse_seq);
        if (result >= 0) {
            mouse_repeat = result == 6 ? 2 : result == 0 ? 1 : 0; // target identity vs. ground movement
            mouse_seq = 0; mouse_last_walk = now_ns();
            if (result == 0 && mouse_world_ok()) dm_world_walk(mouse_tap_x, mouse_tap_y);
        }
    }
    uint64_t now = now_ns();
    if (mouse_world && mouse_down && mouse_repeat && mouse_world_ok() && now - mouse_last_walk >= 33333333ull) {
        mouse_last_walk = now;
        // A monster held at button-down never follows the moving pointer.
        // The world lock re-resolves that monster by type+id every tick.
        dm_world_walk(mouse_repeat==2 ? mouse_tap_x : mouse_x,
                      mouse_repeat==2 ? mouse_tap_y : mouse_y);
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
