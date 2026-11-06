"""Source contracts for the independent Diablo II PC .key and runtime command bridge.

Full native game behavior still requires real game files and keyboard hardware.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
def src(path):return (ROOT/path).read_text(encoding="utf-8")

class NativePcKeyContracts(unittest.TestCase):
    def test_native_keyfile_and_private_emulation_are_disjoint(self):
        c=src("host/dm_kbmap.c")
        self.assertIn('"%s/%s/%s.kbm"',c)
        self.assertIn('"%s/%s/%s.key"',c)
        self.assertIn('if(pc_mode)',c)
        self.assertIn('int rc=dm_pc_profile_get(0,0,&probe);',c)
        self.assertIn('native_held[usage]',c)
        self.assertIn('dm_pc_input_event(usage,pressed)',c)
        self.assertIn('pcMode=%d',c)
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
        self.assertNotIn("SummonMancer",c)
    def test_native_input_mapping_is_latched_and_gamepad_separate(self):
        c=src("host/dm_pc_input.c")
        self.assertIn('dm_pc_profile_action_for_hid(hid,&id,&slot)',c)
        self.assertIn('held_mask[hid]',c)
        self.assertIn('DM_BTN_ZL',c)
        self.assertIn('slot_buttons[4]',c)
        self.assertIn('dm_pc_input_guest_step',c)
        self.assertIn('dm_kb_set(bits,0,0,0,right_y)',c)
        self.assertIn('dm_pc_input_wheel(int delta)',c)
        self.assertIn('dm_ui_pc_toggle_panel(batch[i])',c)
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
        self.assertIn("dm_kbmap_key_event((unsigned)u,down)",mac)
        self.assertIn("nativePcSetMode",activity)
        self.assertIn("nativePcBind",java)
        self.assertIn("KeyboardInput.hidUsage(keyCode)",java)
    def test_main_build_includes_modules(self):
        make=src("host/Makefile")
        for path in ("dm_keyfile.c","dm_pc_profile.c","dm_pc_input.c"):
            self.assertIn(path,make)

if __name__=="__main__":
    unittest.main()
