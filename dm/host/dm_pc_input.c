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
static uint64_t held_mask[256]; // latched at key-down (no re-interpretation of held keys)
enum { UNMAPPED=255 };
enum { PH_IDLE, PH_ZL_LEAD, PH_HELD, PH_ZL_TAP, PH_TAP_PULSE };
static unsigned char skill_phase[256];
static uint64_t skill_sample[256];

// Guest HUD buttons, not positions guessed from a conventional PC key layout.
// See dm_padmode_hud_click: SkillButton1..6 = A, B, Y, X, R, ZR.
// ZL selects the original game's second set of the SAME six skill buttons.
// Skill 13..16 have no verified Switch controller slots and remain unsupported.
static const uint64_t slot_buttons[6]={
    DM_BTN_A,DM_BTN_B,DM_BTN_Y,DM_BTN_X,DM_BTN_R,DM_BTN_ZR
};
static int skill_slot(unsigned id) {
    if(id>=14 && id<=21)return (int)(id-14);
    if(id>=46 && id<=49)return (int)(id-46+8);
    return -1;
}
int dm_pc_input_supported_action(uint32_t id) {
    return id==0 || id==1 || id==4 || id==12 || id==44 || id==54 ||
           id==38 || id==39 || id==40 || id==45 ||
           id==7 || id==23 || id==24 || id==25 || id==26 ||
           id==35 || id==37 || id==56 || skill_slot(id)>=0;
}
static uint64_t command_bits(unsigned id) {
    int slot=skill_slot(id);
    if(slot>=0)return slot_buttons[slot%6] | (slot>=6?DM_BTN_ZL:0);
    switch(id) {
    case 23:return DM_BTN_LEFT;
    case 24:return DM_BTN_UP;
    case 25:return DM_BTN_DOWN;
    case 26:return DM_BTN_RIGHT;
    case 35:return DM_BTN_LS;
    case 38:return dm_uiscreen_panel()?DM_BTN_B:0; // close an existing panel
    case 37:return DM_BTN_L; // show items
    case 56:return dm_uiscreen_ingame()&&!dm_uiscreen_panel()?DM_BTN_PLUS:DM_BTN_B;
    default:return 0;
    }
}
static void publish_locked(void) {
    uint64_t bits=0;int32_t right_y=0;
    for(unsigned i=0;i<256;i++) {
        if(skill_phase[i]==PH_ZL_LEAD || skill_phase[i]==PH_ZL_TAP) {
            // Second-set modifier must reach GetNpadState BEFORE the face edge.
            bits|=DM_BTN_ZL;
            continue;
        }
        if(!down[i] && skill_phase[i]!=PH_TAP_PULSE)continue;
        bits|=held_mask[i];
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
    memset(skill_phase,PH_IDLE,sizeof skill_phase);
    memset(skill_sample,0,sizeof skill_sample);
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
    // Count ACTUAL guest GetNpadState reads, not host wall-clock frames:
    // ZL and its face button in one sample are treated as set I by D2R.
    uint64_t sampled=dm_input_samples();
    pthread_mutex_lock(&input_mu);
    int changed=0;
    for(unsigned i=0;i<256;i++) {
        if((skill_phase[i]==PH_ZL_LEAD || skill_phase[i]==PH_ZL_TAP) &&
           sampled-skill_sample[i]>=2) {
            skill_phase[i]=down[i]?PH_HELD:PH_TAP_PULSE;
            skill_sample[i]=sampled;
            changed=1;
        } else if(skill_phase[i]==PH_TAP_PULSE &&
                  sampled-skill_sample[i]>=2) {
            // Even an extremely short physical key press yields a complete
            // face-button pulse AFTER the modifier has been sampled.
            skill_phase[i]=PH_IDLE;
            command[i]=UNMAPPED;
            held_mask[i]=0;
            changed=1;
        }
    }
    if(changed)publish_locked();
    while(queue_head!=queue_tail && n<sizeof batch)
        batch[n++]=panel_queue[queue_head++%32];
    pthread_mutex_unlock(&input_mu);
    for(unsigned i=0;i<n;i++)
        dm_ui_pc_toggle_panel(batch[i]);
}
int dm_pc_input_mouse_special(unsigned pseudo_vk) {
    if(pseudo_vk<0x100 || pseudo_vk>0x102)return 0;
    uint32_t id=UINT32_MAX;unsigned slot=0;
    if(dm_pc_profile_action_for_vk((uint16_t)pseudo_vk,&id,&slot))return 0;
    if(id==7 || id==45){dm_world_mouse_map_toggle();return 1;}
    if(id==0 || id==1 || id==4 || id==12 || id==44 || id==54) {
        pthread_mutex_lock(&input_mu);
        int ok=queue_tail-queue_head<32;
        if(ok)panel_queue[queue_tail++%32]=(unsigned char)id;
        pthread_mutex_unlock(&input_mu);
        return ok;
    }
    return 0;
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
    if(pressed && !down[hid] &&
       (skill_phase[hid]==PH_ZL_TAP || skill_phase[hid]==PH_TAP_PULSE)) {
        // A second physical press while a short-tap is pending resumes the
        // same latched native command without losing a release edge.
        down[hid]=1;
        skill_phase[hid]=skill_phase[hid]==PH_ZL_TAP?PH_ZL_LEAD:PH_HELD;
        publish_locked();
        pthread_mutex_unlock(&input_mu);
        return;
    }
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
        skill_phase[hid]=PH_IDLE;
        if(skill_slot(command[hid])>=6) {
            skill_phase[hid]=PH_ZL_LEAD;
            skill_sample[hid]=dm_input_samples();
        }
        if(command[hid]==7 || command[hid]==45)dm_world_mouse_map_toggle();
        if(command[hid]==0 || command[hid]==1 || command[hid]==4 ||
           command[hid]==12 || command[hid]==44 || command[hid]==54) {
            if(queue_tail-queue_head<32)panel_queue[queue_tail++%32]=command[hid];
        }
    } else {
        down[hid]=0;
        if(skill_phase[hid]==PH_ZL_LEAD) {
            // Keep ZL and schedule an actual short press, even if key-up
            // arrived before the game sampled the first-set modifier.
            skill_phase[hid]=PH_ZL_TAP;
        } else {
            skill_phase[hid]=PH_IDLE;
            command[hid]=UNMAPPED;
            held_mask[hid]=0;
        }
    }
    publish_locked();
    pthread_mutex_unlock(&input_mu);
}
