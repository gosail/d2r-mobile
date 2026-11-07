"""Source-level contracts for PC-pointer world feedback on all host platforms.

These assertions intentionally do not claim game runtime, mouse hardware or
the proprietary original D2R ROMFS was tested.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]

def source(path):
    return (ROOT / path).read_text(encoding="utf-8")

class PointerWorldRender(unittest.TestCase):
    def test_real_guest_combat_status(self):
        world = source("host/dm_world.c")
        self.assertIn("stat(unit,6,0)", world)
        self.assertIn("stat(unit,7,0)", world)
        self.assertIn("view.hp_max=", world)
        self.assertIn("view.immune_mask|=", world)
        self.assertIn("view.tier=rank", world)
        self.assertIn("dm_world_hover_native_name",world)
        self.assertIn("g_native_hover_name_id",world)
        self.assertIn("hover_native_monster_title",source("host/diag.c"))
        self.assertIn("dm_threat_tier", world)
        self.assertIn("view.kind=4", world)
        self.assertIn("view.w=fminf(1-view.x,wide/W)",world)
        self.assertNotIn('Monster #%u',world)

    def test_hover_is_passive(self):
        world = source("host/dm_world.c")
        start=world.index("static void w_hover_step(")
        end=world.index("static _Atomic uint64_t g_tap;", start)
        hover=world[start:end]
        for prohibited in ("w_interact(", "w_go(", "w_attack(", "dm_world_walk("):
            self.assertNotIn(prohibited, hover)
        self.assertIn("atomic_load(&g_hover_request) != req", hover)

    def test_android_overlay_is_mounted(self):
        activity = source("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java")
        overlay = source("android/app/src/main/java/com/xiaoboluo/d2r/MouseHoverOverlay.java")
        self.assertIn("new MouseHoverOverlay(this)", activity)
        self.assertIn("gameFrame.addView(mouseHoverOverlay", activity)
        self.assertIn("nativeMouseHover(h)", overlay)
        self.assertIn("(int)h[0] >= 1 && (int)h[0] <= 4", overlay)
        self.assertIn("h[5]/h[6]", overlay)
        self.assertIn("h[9]==0", overlay)
        self.assertIn("new float[10]", overlay)
        self.assertIn("dm_world_hover_native_visible()", source("android/jni_android.c"))
        self.assertIn("g_native_hover_name_id==current", source("host/dm_world.c"))

    def test_apple_hovers_are_painted(self):
        ios = source("ios/Sources/front_ios.m")
        mac = source("host/front_mac.m")
        self.assertIn("[self mouseHoverSync]", ios)
        self.assertIn("dm_world_hover_read(&h)", ios)
        self.assertIn("g_mouse_outline", mac)
        self.assertIn("mouse_hover_draw();", mac)
        self.assertIn("dm_world_hover_read(&h)", mac)
        self.assertIn('h.kind>=1 && h.kind<=4',ios)
        self.assertIn('h.kind>=1 && h.kind<=4',mac)
        # The native MonsterHealth panel must be the *only* Apple monster
        # health/nameplate presenter. A passive selection outline is allowed.
        for code in (ios, mac):
            self.assertIn('if (h.kind==1 && h.title[0])',code)
            self.assertNotIn('h.title[0] || h.details[0] || h.hp_max>0',code)
            self.assertNotIn('if (health)',code)
            self.assertNotIn('g_mouse_hp_bg',code)
            self.assertNotIn('_mouseHoverHpBg',code)
        android=source("android/app/src/main/java/com/xiaoboluo/d2r/MouseHoverOverlay.java")
        self.assertIn('if(!title.isEmpty() || !detail.isEmpty() || health)',android)
        self.assertIn('boolean health=monster && h[6]>0',android)

    def test_held_items_do_not_snap_back(self):
        cursor=source("host/dm_cursor.c")
        ios=source("ios/Sources/front_ios.m")
        android=source("android/touch_android.c")
        self.assertIn("if (ok) dm_cursor_clear()", cursor)
        self.assertIn("physical_mouse_owns_item()", cursor)
        self.assertIn("g_mouse_holding_owner", cursor)
        self.assertIn("dm_ui_mouse_ownership_release()", ios)
        self.assertIn("dm_ui_mouse_ownership_release()", android)
        self.assertIn("_controls.pointer_valid=0", ios)
        self.assertIn("!mouse_touch_on && tc.pointer_slot==DM_TC_NONE", android)

if __name__ == "__main__":
    unittest.main()
