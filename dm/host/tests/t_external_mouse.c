// Portable regression for the shared iPadOS/Android physical mouse bridge.
// Include its real implementation, with minimal game-thread stubs. This
// exercises press/drag/release lifecycle, not just source-string assertions.
// Real game assets and device frameworks are not required.
#define __ANDROID__ 1
#include "../dm_external_mouse.c"
#include <assert.h>
#include <stdio.h>

static int panel=1,ingame=1,pc_mode,native_status,mouse_role[3]={0,1,2};
static int native_special_calls,native_wheel_calls;
static int touch_phases[32],touch_count,world_stops,right_uses,map_toggles;
static unsigned long long ticks=1000000000ull;

uint64_t now_ns(void) {return ticks;}
int dm_game_paused(void) {return 0;}
uint64_t dm_padmode_legend_click(float x,float y) {(void)x;(void)y;return 0;}
int dm_padmode_hud_click(float x,float y,uint64_t *out) {
    (void)x;(void)y;*out=0;return 0;
}
int dm_uiscreen_ingame(void) {return ingame;}
int dm_uiscreen_panel(void) {return panel;}
int dm_ui_item_at_now(float x,float y) {(void)x;(void)y;return 1;}
void dm_ui_touch(int phase,float x,float y) {
    (void)x;(void)y;
    assert(touch_count<(int)(sizeof touch_phases/sizeof touch_phases[0]));
    touch_phases[touch_count++]=phase;
}
int dm_ui_mouse_hover_now(float x,float y) {(void)x;(void)y;return 1;}
void dm_ui_mouse_right_use(float x,float y) {
    (void)x;(void)y;right_uses++;
}
void dm_world_hover_move(float x,float y) {(void)x;(void)y;}
void dm_world_hover_end(void) {}
unsigned dm_world_click(float x,float y) {(void)x;(void)y;return 1;}
int dm_world_tap_result(unsigned seq) {(void)seq;return -1;}
void dm_world_walk(float x,float y) {(void)x;(void)y;}
void dm_world_mouse_stop(void) {world_stops++;}
void dm_world_mouse_release(void) {}
void dm_world_mouse_map_toggle(void) {map_toggles++;}
int dm_kbmap_mouse_action(unsigned physical) {
    return physical<3?mouse_role[physical]:-1;
}
int dm_kbmap_pc_mode(void) {return pc_mode;}
int dm_pc_profile_file_status(void) {return native_status;}
int dm_pc_input_mouse_special(unsigned pseudo) {
    assert(pseudo==0x100);native_special_calls++;return 1;
}
int dm_pc_input_wheel(int delta) {native_wheel_calls++;return delta>0?1:-1;}

static void event_at(int op) {
    dm_external_mouse_event(op,0.4f,0.4f,0.4f,0.4f,0);
    dm_external_mouse_step();
}
int main(void) {
    // Inventory, equipment, stash and cube all use the panel UI pathway.
    // Holding left click across several game frames must keep the native UI
    // drag alive instead of changing the later MOVE into a hover.
    event_at(DM_MOUSE_DOWN);
    assert(touch_count==1 && touch_phases[0]==0);
    dm_external_mouse_step(); // an extra frame without any pointer event
    event_at(DM_MOUSE_DRAG);
    dm_external_mouse_step();
    event_at(DM_MOUSE_HOVER_END); // UIKit may temporarily end hover
    event_at(DM_MOUSE_DRAG);
    assert(touch_count==3);
    assert(touch_phases[1]==1 && touch_phases[2]==1);
    event_at(DM_MOUSE_UP);
    assert(touch_count==4 && touch_phases[3]==2);
    assert(world_stops>0);

    // Cancel means cancel, never synthesize a release/drop into another slot.
    event_at(DM_MOUSE_DOWN);
    event_at(DM_MOUSE_LEAVE);
    assert(touch_count==6 && touch_phases[4]==0 && touch_phases[5]==3);
    event_at(DM_MOUSE_UP); // stale release after mouse leave is ignored
    assert(touch_count==6);

    // A physical button may be rebound while it is held. The release must
    // still end the role selected at mouse-down, not the new assignment.
    panel=0;
    mouse_role[0]=1;
    event_at(DM_MOUSE_DOWN); // physical left becomes logical secondary
    uint64_t buttons=0;int32_t rx=0,ry=0;
    dm_external_mouse_apply(&buttons,&rx,&ry);
    assert(buttons&DM_BTN_B);
    mouse_role[0]=0;
    event_at(DM_MOUSE_UP); // release routes to the latched secondary role
    buttons=0;dm_external_mouse_apply(&buttons,&rx,&ry);
    assert(!(buttons&DM_BTN_B));
    event_at(DM_MOUSE_LEAVE);
    assert(touch_count==6);

    // A saved character .key can exist while showing the front-end menu.
    // Wheel and middle must keep menu-safe navigation and only dispatch
    // native bindings after entering an actual game.
    ingame=0;pc_mode=1;native_status=1;mouse_role[0]=0;
    int before_map=map_toggles;
    event_at(DM_MOUSE_MIDDLE_DOWN);
    assert(map_toggles==before_map+1 && native_special_calls==0);
    event_at(DM_MOUSE_MIDDLE_UP);
    dm_external_mouse_event(DM_MOUSE_WHEEL,0.4f,0.4f,0.4f,0.4f,1);
    dm_external_mouse_step();
    buttons=0;rx=ry=0;dm_external_mouse_apply(&buttons,&rx,&ry);
    assert(ry==32767 && native_wheel_calls==0);
    ingame=1;
    event_at(DM_MOUSE_MIDDLE_DOWN);
    assert(native_special_calls==1);
    event_at(DM_MOUSE_MIDDLE_UP);
    dm_external_mouse_event(DM_MOUSE_WHEEL,0.4f,0.4f,0.4f,0.4f,1);
    dm_external_mouse_step();
    assert(native_wheel_calls==1);
    (void)right_uses;
    puts("physical mouse panel drag / cancel / role latch / menu fallback: PASS");
    return 0;
}
