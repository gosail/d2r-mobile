"""Physical keyboard wiring regression checks; no proprietary ROMFS required."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
def src(name):return (ROOT/name).read_text(encoding="utf-8")
class KeyboardContract(unittest.TestCase):
    def test_independent_channel(self):
        h=src("host/hle_hid.c")
        self.assertIn("g_kb_btn",h)
        self.assertIn("atomic_load(&g_kb_btn)",h)
        self.assertIn("g_kb_axes",h)
        self.assertIn("dm_kb_set(",h)
    def test_ipad_keyboard(self):
        v=src("ios/Sources/front_ios.m")
        for s in ("- (void)pressesBegan:","- (void)pressesEnded:","p.key.keyCode","dm_kbmap_key_event(usage,1)","dm_kbmap_key_event(usage,0)","dm_kbmap_reset()"):
            self.assertIn(s,v)
    def test_android_keyboard(self):
        j=src("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java")
        k=src("android/app/src/main/java/com/xiaoboluo/d2r/KeyboardInput.java")
        n=src("android/jni_android.c")
        for s in ("KeyboardInput.onKey(e)","nativeKeyboardEvent(int hidUsage, boolean down)","nativeKeyboardReset()"):
            self.assertIn(s,j)
        for s in ("KEYCODE_F1","KEYCODE_DPAD_UP","SOURCE_GAMEPAD","KEYCODE_A","nativeKeyboardEvent",
                  "KEYCODE_NUMPAD_DIVIDE","KEYCODE_NUMPAD_MULTIPLY","KEYCODE_NUMPAD_SUBTRACT","KEYCODE_NUMPAD_DOT"):
            self.assertIn(s,k)
        self.assertIn("dm_kbmap_key_event((unsigned)usage,down!=0)",n)
    def test_controls_editor_entry(self):
        native=src("host/dm_kbmap.c")
        ui=src("ios/Sources/front_ios.m")
        android=src("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java")
        editor=src("android/app/src/main/java/com/xiaoboluo/d2r/KeyboardSettings.java")
        self.assertIn('arg && !strcmp(arg,"KeyBindingMenu")',native)
        self.assertIn('dm_kbmap_controls_event(cat, act, arg)',src("host/diag.c"))
        self.assertIn('dm_gamepad_external_input();',src("ios/Sources/front_ios.m"))
        self.assertIn('PadInput.externalInput();',src("android/app/src/main/java/com/xiaoboluo/d2r/KeyboardInput.java"))
        self.assertIn('native_seen=1',native)
        self.assertIn('if(status<0 || seen_native)return;',native)
        self.assertIn('dm_pc_vk_from_hid(usage)>=0',src("ios/Sources/front_ios.m"))
        self.assertIn('[self kbSettingsSync]',ui)
        self.assertIn('[self kbPcOpen]',ui)
        self.assertIn('KeyboardSettings.createInline(this)',android)
        self.assertIn('KeyboardSettings.refreshInline(this,kbInlinePanel)',android)
        self.assertIn('kbKeyboardMode',android)
        self.assertIn('if(libraryLoaded && started)nativeKeyboardReset();',android)
        self.assertIn('controllerSettingsButton',android)
        self.assertIn('mouseHitsControls(e)',android)
        self.assertIn('kbDeviceTabs.getVisibility()==View.VISIBLE)return super.dispatchKeyEvent(e)',android)
        self.assertIn('dm_kbmap_reset(); // clear held keys before editor takes first responder',ui)
        mac=src("host/front_mac.m")
        self.assertIn('!g_mac_keyboard_panel.hidden) return e;',mac)
        event_code=mac.split('NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged handler:')[1]
        self.assertLess(event_code.find('!g_mac_keyboard_panel.hidden) return e;'),
                        event_code.find('mac_modifier_changed(e);'),
                        "Settings editor must intercept modifiers before gameplay")
        self.assertIn('mac_modifier_changed(e);',event_code)
        self.assertIn('dm_kbmap_key_event((unsigned)usage,down)',mac)
        self.assertIn('if(a==DM_KB_ENTER)a=dm_uiscreen_ingame()&&!dm_uiscreen_panel()?10:0',src("host/dm_kbmap.c"))
        self.assertNotIn('g_mac_keys_down',mac) # never maintain a competing Mac mapping
        self.assertNotIn('dm_ui_item_at_now(ux,uy)',mac) # guest-thread only
        self.assertIn('controlsMouseCaptured',android)
        self.assertIn('gameMouseCaptured',android)
        self.assertIn('controlsTouchCaptured',android)
        self.assertIn('if(controlsTouchCaptured || mouseHitsControls(e))',android)
        self.assertIn('if(mac_controls_hit(v,e) && !mac_mouse_down[0])',src("host/front_mac.m"))
        self.assertIn('if (mac_controls_hit(v,e)) return e; // Native NSScrollView scrolls.',src("host/front_mac.m"))
        self.assertIn('UISegmentedControl *_kbDeviceTabs',ui)
        self.assertIn('@selector(kbPcOpen)',ui)
        self.assertIn('g_mac_keyboard_tab',src("host/front_mac.m"))
        self.assertIn('g_mac_pad_tab',src("host/front_mac.m"))
        self.assertIn('原生角色 .key',ui)
        self.assertIn('NativePcKeyboardSettings.show(activity)',editor)
        self.assertIn('nativeMouseBinding(int physical)',android)
        self.assertIn('DM_MOUSE_MIDDLE_UP',src("host/dm_external_mouse.c"))
        self.assertNotIn('nativeKbBind(',editor)
    def test_runtime_bridge_does_not_create_private_character_file(self):
        c=src("host/dm_kbmap.c")
        self.assertNotIn(".kbm",c)
        self.assertNotIn("fopen(",c)
        self.assertNotIn("rename(",c)
        self.assertNotIn("sw_keyboard",c)
        self.assertIn("dm_pc_profile_file_status()==1",c)
        self.assertIn("dm_pc_input_event(usage,pressed)",c)
        self.assertIn("dm_kbmap_pre_commit",c)
        self.assertIn("return enabled!=0;",c)
        self.assertIn("dm_kbmap_select_character",src("host/dm_pages.c"))
        for path in ("android/app/src/main/java/com/xiaoboluo/d2r/KeyboardSettings.java",
                     "ios/Sources/front_ios.m", "host/front_mac.m", "host/dm_kbmap.h"):
            self.assertNotIn(".kbm",src(path))
        self.assertNotIn("nativeMouseBind(",src("android/app/src/main/java/com/xiaoboluo/d2r/KeyboardSettings.java"))
if __name__=="__main__":unittest.main()
