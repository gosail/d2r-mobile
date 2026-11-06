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
        for s in ("KEYCODE_F1","KEYCODE_DPAD_UP","SOURCE_GAMEPAD","KEYCODE_A","nativeKeyboardEvent"):
            self.assertIn(s,k)
        self.assertIn("dm_kbmap_key_event((unsigned)usage,down!=0)",n)
    def test_controls_editor_entry(self):
        native=src("host/dm_kbmap.c")
        ui=src("ios/Sources/front_ios.m")
        android=src("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java")
        editor=src("android/app/src/main/java/com/xiaoboluo/d2r/KeyboardSettings.java")
        self.assertIn('arg && !strcmp(arg,"KeyBindingMenu")',native)
        self.assertIn('[self kbSettingsSync]',ui)
        self.assertIn('dm_kbmap_bind(usage,(unsigned)action)',ui)
        self.assertIn('KeyboardSettings.createInline(this)',android)
        self.assertIn('KeyboardSettings.refreshInline(this,kbInlinePanel)',android)
        self.assertIn('kbKeyboardMode',android)
        self.assertIn('controllerSettingsButton',android)
        self.assertIn('mouseHitsControls(e)',android)
        self.assertIn('kbDeviceTabs.getVisibility()==View.VISIBLE)return super.dispatchKeyEvent(e)',android)
        self.assertIn('dm_kbmap_reset(); // clear held keys before editor takes first responder',ui)
        self.assertIn('!g_mac_keyboard_panel.hidden) return e;',src("host/front_mac.m"))
        self.assertIn('controlsMouseCaptured',android)
        self.assertIn('gameMouseCaptured',android)
        self.assertIn('if(mac_controls_hit(v,e) && !mac_mouse_down[0])',src("host/front_mac.m"))
        self.assertIn('if (mac_controls_hit(v,e)) return e; // Native NSScrollView scrolls.',src("host/front_mac.m"))
        self.assertIn('UISegmentedControl *_kbDeviceTabs',ui)
        self.assertIn('kbBindingTapped:',ui)
        self.assertIn('g_mac_keyboard_tab',src("host/front_mac.m"))
        self.assertIn('g_mac_pad_tab',src("host/front_mac.m"))
        self.assertIn('dm_kbmap_bind_mouse(physical,role)',ui)
        self.assertIn('DmActivity.nativeMouseBind(physical,which)',editor)
        self.assertIn('nativeMouseBinding(int physical)',android)
        self.assertIn('DM_MOUSE_MIDDLE_UP',src("host/dm_external_mouse.c"))
        self.assertIn('DmActivity.nativeKbBind(KEYS[idx],ACTIONS[which])',editor)
    def test_local_non_pc_config(self):
        c=src("host/dm_kbmap.c")
        self.assertIn("%s/%s/%s.key",c)
        self.assertIn("dm_kbmap_select_character",c)
        self.assertIn("foreign_format=1",c)
        self.assertIn("dm_kbmap_pre_commit",c)
        pages=src("host/dm_pages.c")
        fs=src("host/hle_fs.c")
        self.assertIn("dm_kbmap_select_character((const char *)pd)",pages)
        self.assertIn("dm_kbmap_pre_commit(g_mnt[i].dir)",fs)
        self.assertIn("rename(tmp,fn)",c)
        self.assertNotIn(".ctl",c.split("static void defaults(")[1])
if __name__=="__main__":unittest.main()
