#include "dm_pc_input.h"
#include "dm_pc_profile.h"
#include "dm_input.h"
#include "dm_world.h"
#include "dm_uiscreen.h"
#include <pthread.h>
#include <stdint.h>
#include <string.h>
// Implemented by the original guest PanelManager adapter in dm_cursor.c.
void dm_ui_pc_toggle_panel(unsigned command_id);

static pthread_mutex_t input_mu=PTHREAD_MUTEX_INITIALIZER;
static unsigned char down[256],command[256];
static unsigned char panel_queue[32];
static unsigned queue_head,queue_tail;
static uint64_t held_mask[256]; // latched at key-down; UI changes cannot reinterpret a held Escape
enum { UNMAPPED=255 };
static const uint64_t slot_buttons[6] = {
    DM_BTN_A,DM_BTN_B,DM_BTN_Y,DM_BTN_X,DM_BTN_R,DM_BTN_ZR
};
int dm_pc_input_supported_action(uint32_t id) {
    return id==0 || id==1 || id==4 || id==12 || id==44 || id==54 ||
           id==38 || id==39 || id==40 || id==45 ||
           id==7 || id==23 || id==24 || id==25 || id==26 ||
           id==35 || id==37 || id==56 || (id>=14 && id<=21) ||
           (id>=46 && id<=49);
}
// Skill 13..16 have no direct original Switch controller slots; do not
// activate a different skill by guessing. Character/inventory modal
// commands require game UI message integration, not a fake Plus press.
static uint64_t command_bits(unsigned id) {
    if(id>=14 && id<=21) {
        unsigned slot=id-14;
        return slot_buttons[slot%6] | (slot>=6?DM_BTN_ZL:0);
    }
    if(id>=46 && id<=49) {
        unsigned slot=id-46+8;
        return slot_buttons[slot%6] | DM_BTN_ZL;
    }
    switch(id) {
    case 23:return DM_BTN_LEFT;
    case 24:return DM_BTN_UP;
    case 25:return DM_BTN_DOWN;
    case 26:return DM_BTN_RIGHT;
    case 35:return DM_BTN_LS;
    case 38:return dm_uiscreen_panel()?DM_BTN_B:0; // close panels only when open // native controller RunButton prompt
    case 37:return DM_BTN_L;  // native controller show-items button
    case 56:return dm_uiscreen_ingame()&&!dm_uiscreen_panel()?DM_BTN_PLUS:DM_BTN_B;
    default:return 0;
    }
}
static void publish_locked(void) {
    uint64_t bits=0;int32_t right_y=0;
    for(unsigned i=0;i<256;i++)if(down[i]) {
        bits|=held_mask[i];
        // Original PC previous/next-skill commands are implemented by the
        // existing Switch right-stick skill selector, not fake face buttons.
        if(command[i]==39)right_y=32767;
        if(command[i]==40)right_y=-32767;
    }
    dm_kb_set(bits,0,0,0,right_y);
}
void dm_pc_input_reset(void) {
    pthread_mutex_lock(&input_mu);
    memset(down,0,sizeof down);
    memset(command,UNMAPPED,sizeof command);
    memset(held_mask,0,sizeof held_mask);
    queue_head=queue_tail=0;
    dm_kb_set(0,0,0,0,0);
    pthread_mutex_unlock(&input_mu);
}
int dm_pc_input_latched_command(unsigned hid) {
    if(hid>=256)return -1;
    pthread_mutex_lock(&input_mu);
    int cmd=down[hid] && command[hid]!=UNMAPPED ? (int)command[hid]:-1;
    pthread_mutex_unlock(&input_mu);return cmd;
}
void dm_pc_input_guest_step(void) {
    unsigned char batch[32];unsigned n=0;
    pthread_mutex_lock(&input_mu);
    while(queue_head!=queue_tail && n<sizeof batch)
        batch[n++]=panel_queue[queue_head++%32];
    pthread_mutex_unlock(&input_mu);
    for(unsigned i=0;i<n;i++)
        dm_ui_pc_toggle_panel(batch[i]);
}
int dm_pc_input_wheel(int delta) {
    if(!delta)return 0;
    uint32_t id=UINT32_MAX;unsigned slot=0;
    uint16_t pseudo=delta>0?0x103:0x104;
    if(dm_pc_profile_action_for_vk(pseudo,&id,&slot))return 0;
    if(id==39)return 1;
    if(id==40)return -1;
    // The map toggle and panel shortcuts can be emitted as one-shot events.
    if(id==7 || id==45){dm_world_mouse_map_toggle();return 0;}
    if(id==0 || id==1 || id==4 || id==12 || id==44 || id==54) {
        pthread_mutex_lock(&input_mu);
        if(queue_tail-queue_head<32)panel_queue[queue_tail++%32]=(unsigned char)id;
        pthread_mutex_unlock(&input_mu);
    }
    return 0; // refuse to trigger unverified commands on a scroll event
}
void dm_pc_input_event(unsigned hid,int pressed) {
    if(hid>=256)return;
    pthread_mutex_lock(&input_mu);
    if(down[hid]==(unsigned char)!!pressed) {
        pthread_mutex_unlock(&input_mu);return;
    }
    if(pressed) {
        uint32_t id=0;unsigned slot=0;
        int rc=dm_pc_profile_action_for_hid(hid,&id,&slot);
        down[hid]=1;
        command[hid]=rc==0 && id<=56 && dm_pc_input_supported_action(id)?
            (unsigned char)id:UNMAPPED;
        held_mask[hid]=command[hid]==UNMAPPED?0:command_bits(command[hid]);
        if(command[hid]==7 || command[hid]==45)dm_world_mouse_map_toggle();
        if(command[hid]==0 || command[hid]==1 || command[hid]==4 ||
           command[hid]==12 || command[hid]==44 || command[hid]==54) {
            // Keep menu operations off the platform input thread.
            if(queue_tail-queue_head<32)panel_queue[queue_tail++%32]=command[hid];
        }
    } else {
        down[hid]=0;
        command[hid]=UNMAPPED;
        held_mask[hid]=0;
    }
    publish_locked();
    pthread_mutex_unlock(&input_mu);
}
