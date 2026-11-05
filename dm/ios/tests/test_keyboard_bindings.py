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
        self.assertIn('KeyboardSettings.show(this)',android)
        self.assertIn('DmActivity.nativeKbBind(KEYS[idx],ACTIONS[which])',editor)
    def test_local_non_pc_config(self):
        c=src("host/dm_kbmap.c")
        self.assertIn("sw_keyboard.txt",c)
        self.assertIn("rename(tmp,fn)",c)
        self.assertNotIn(".ctl",c.split("static void defaults(")[1])
if __name__=="__main__":unittest.main()
