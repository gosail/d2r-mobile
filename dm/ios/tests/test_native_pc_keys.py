"""Source contracts for the independent Diablo II PC .key and runtime command bridge.

Full native game behavior still requires real game files and keyboard hardware.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
def src(path):return (ROOT/path).read_text(encoding="utf-8")

class NativePcKeyContracts(unittest.TestCase):
    def test_native_keyfile_and_runtime_controller_bridge(self):
        c=src("host/dm_kbmap.c")
        self.assertNotIn('.kbm',c)
        self.assertNotIn('fopen(',c)
        self.assertIn('int status=ingame?dm_pc_profile_file_status():0;',c)
        self.assertIn('if(status==1 || (ingame && arrow && (status<0 || seen_native)))',c)
        self.assertIn('native_held[usage]',c)
        self.assertIn('dm_pc_input_event(usage,1)',c)
        self.assertIn('dm_pc_input_event(usage,0)',c)
        self.assertIn('if(status<0 || seen_native)return;',c)
        self.assertIn('memset(down,0,sizeof down)',c)
        profile=src("host/dm_pc_profile.c")
        self.assertIn('if(dm_fs_native_key_writers())',profile)
        self.assertIn('dm_fs_save_poison(errno?errno:EIO)',profile)
        fs=src("host/hle_fs.c")
        self.assertIn('g_fkey[fd]=is_native_key(path)',fs)
        self.assertIn('g_key_writers',fs)
        self.assertIn('KEY_FD_LOCK(h);',fs)
        mac=src("host/front_mac.m")
        self.assertIn('mac_modifier_changed(e);',mac)
        self.assertIn('g_mac_modifier_down[usage]',mac)
        hover=src("host/dm_world.c")
        self.assertIn('if (view.immune_mask)',hover)
        self.assertNotIn('if (view.details[0])\n                                strlcat(view.details,dm_lang_zh()?"免疫"',hover)
        self.assertIn('return enabled!=0;',c)
        self.assertIn('dm_pc_profile_select(name)',c)
        self.assertIn('dm_pc_input_reset(); // no stale hero-specific held command',c)
    def test_codecs_are_independent_of_uploaded_character_config(self):
        c=src("host/dm_keyfile.c")
        self.assertIn("dm_keyfile_new_legacy",c)
        self.assertIn("dm_keyfile_new_resurrected",c)
        self.assertIn("dm_keyfile_bind_checked",c)
        self.assertIn("le32(p+6)",c)
        self.assertIn("O_CREAT|O_EXCL",c)
        self.assertIn("link(temp,path)",c)
        self.assertIn("patch_guest_keyfile(f,path)",c)
        self.assertIn("pwrite(fd,f->raw+pos+wrote",c)
        profile=src("host/dm_pc_profile.c")
        self.assertIn("dm_save_workdir()",profile)
        self.assertNotIn("dm_keyfile_new_resurrected(&temp)",profile)
        self.assertNotIn("dm_keyfile_new_legacy(&temp)",profile)
        self.assertIn("int rc=dm_keyfile_read(&temp,fn);",profile)
        self.assertIn("dm_keyfile_free(&profile);",profile)
        self.assertIn("dm_pc_profile_pre_commit_locked(workdir);",
                      src("host/dm_kbmap.c"))
        self.assertIn("edited_vk[id][slot]=vk;",profile)
        self.assertIn("dm_keyfile_write_atomic(&latest,fn);",profile)
        self.assertNotIn("SummonMancer",c)
    def test_native_input_mapping_is_latched_and_gamepad_separate(self):
        c=src("host/dm_pc_input.c")
        self.assertIn('dm_pc_profile_action_for_hid(hid,&id,&slot)',c)
        self.assertIn('held_mask[hid]',c)
        self.assertIn('DM_BTN_ZL',c)
        self.assertIn('slot_buttons[6]',c)
        self.assertIn('DM_BTN_A,DM_BTN_B,DM_BTN_Y,DM_BTN_X,DM_BTN_R,DM_BTN_ZR',c)
        self.assertIn('skill_slot(id)>=0',c)
        self.assertIn('sampled-skill_sample[i]>=2',c)
        self.assertIn('dm_input_samples()',c)
        hid=src("host/hle_hid.c")
        self.assertIn('atomic_fetch_add(&g_valid_sampling,1);',hid)
        self.assertIn('return atomic_load(&g_valid_sampling);',hid)
        self.assertIn('dm_pc_input_guest_step',c)
        self.assertIn('dm_kb_set(bits,lx,ly,0,right_y)',c)
        self.assertIn('dm_pc_input_wheel(int delta)',c)
        self.assertIn('int dm_pc_input_middle_or_default(void)',c)
        self.assertIn('int dm_pc_input_wheel_or_default(int delta)',c)
        self.assertIn('rc==DM_KEYFILE_NOT_FOUND',c)
        self.assertIn('dm_ui_pc_toggle_panel(batch[i])',c)
    def test_native_mouse_skill_binding_is_guest_owned(self):
        pc=src("host/dm_pc_input.c")
        ui=src("host/dm_cursor.c")
        mouse=src("host/dm_external_mouse.c")
        self.assertIn('dm_ui_pc_skill_bind_request(command[hid])',pc)
        self.assertIn('command[hid]=UNMAPPED;',pc)
        self.assertIn('uint64_t dm_pc_input_skill_buttons(uint32_t id)',pc)
        self.assertIn('static int pc_skill_hit_walk(',ui)
        self.assertIn('!modal_any(root,1,NULL)',ui)
        self.assertIn('int dm_ui_pc_skill_bind_request(uint32_t id)',ui)
        self.assertIn('atomic_compare_exchange_strong(&g_pc_skill_bind_req',ui)
        self.assertIn('ui_pc_skill_bind_pump(proot, fr);',ui)
        self.assertIn('dm_ui_key_pulse(DM_BTN_Y,5)',ui)
        self.assertIn('dm_ui_key_pulse(DM_BTN_MINUS,4)',ui)
        self.assertIn('dm_ui_key_pulse(DM_BTN_ZL,30)',ui)
        self.assertIn('dm_ui_pc_skill_hover_end();',mouse)
        # Skill 13..16 have no validated original controller slots.
        self.assertIn('if(id>=46 && id<=49)',pc)
        self.assertIn('return -1;',pc.split('static int skill_slot(unsigned id)')[1].split('}',1)[0])

    def test_game_thread_panel_and_original_weapon_swap(self):
        world=src("host/dm_world.c")
        cursor=src("host/dm_cursor.c")
        actions=src("host/dm_actions.c")
        self.assertIn('dm_pc_input_guest_step();',world)
        self.assertIn('void dm_ui_pc_toggle_panel(unsigned command_id)',cursor)
        self.assertIn('fe_send("PanelManager",visible?"ClosePanel":"OpenPanel",panel)',cursor)
        self.assertIn('dm_swapskill_manual_request();',cursor)
        self.assertIn('swap0(w);',actions)
    def test_three_platform_control_editors_call_native_keyfile(self):
        ios=src("ios/Sources/front_ios.m")
        mac=src("host/front_mac.m")
        activity=src("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java")
        java=src("android/app/src/main/java/com/xiaoboluo/d2r/NativePcKeyboardSettings.java")
        for text in (ios,mac,java):
            self.assertIn('Primary',text)
            self.assertIn('Secondary',text)
        self.assertIn("dm_pc_profile_bind(",ios)
        self.assertIn("g_pc_capture_action",ios)
        self.assertIn("dm_pc_profile_bind(",mac)
        self.assertIn("g_mac_pc_capture_id",mac)
        self.assertIn("dm_kbmap_key_event((unsigned)usage,down)",mac)
        self.assertIn("nativePcSetMode",activity)
        self.assertIn("nativePcBind",java)
        # Guest-provided files may omit commands; editors must never
        # assume command zero or a fixed 57-pair payload is present.
        self.assertIn("dm_pc_profile_file_status()!=1",mac)
        self.assertIn("dm_pc_profile_file_status()!=1",ios)
        self.assertIn("if(ok==DM_KEYFILE_NOT_FOUND)item.enabled=NO",ios)
        self.assertIn("nativePcStatus",activity)
        self.assertIn("button.setEnabled(vk>=0)",java)
        self.assertIn("KeyboardInput.hidUsage(keyCode)",java)
        for source in (ios,mac,java):
            self.assertIn("0x100",source) # original mouse-middle pseudo key
            self.assertIn("0x103",source) # wheel-up
            self.assertIn("0x104",source) # wheel-down
        self.assertIn("dm_pc_profile_file_status()",src("host/dm_external_mouse.c"))
        self.assertIn("dm_pc_profile_file_status()",mac)
    def test_main_build_includes_modules(self):
        make=src("host/Makefile")
        for path in ("dm_keyfile.c","dm_pc_profile.c","dm_pc_input.c"):
            self.assertIn(path,make)

if __name__=="__main__":
    unittest.main()
