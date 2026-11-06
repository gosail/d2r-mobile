"""Static parity checks for physical pointer behavior shared across all ports.
No game binaries or user-owned data required. This does not replace device tests.
"""
from pathlib import Path
import unittest

DM = Path(__file__).resolve().parents[2]

class PhysicalMouseParity(unittest.TestCase):
    def get(self, path):
        return (DM / path).read_text(encoding="utf-8")

    def test_mouse_left_picks_and_drag_is_not_long_press(self):
        controls = self.get("ios/Sources/dm_touch_controls.c")
        header = self.get("ios/Sources/dm_touch_controls.h")
        self.assertIn("int mouse_item_mode;", header)
        self.assertIn("done.mouse_item && s->pointer_item == 1", controls)
        self.assertIn("f->mouse_item || f->pointer_dragged", controls)
        self.assertIn("done.mouse_item || elapsed < DM_TC_POINTER_TAP_S", controls)

    def test_right_uses_current_item_not_old_focus(self):
        cursor = self.get("host/dm_cursor.c")
        mouse = self.get("host/dm_external_mouse.c")
        self.assertIn("dm_ui_mouse_right_use(e->ux,e->uy)", mouse)
        self.assertIn("ci.container==DM_ITEM_AT_SHOP", cursor)
        self.assertIn("dm_ui_item_at_now(nx,ny)", cursor)

    def test_middle_map_each_platform(self):
        shared = self.get("host/dm_external_mouse.c")
        world = self.get("host/dm_world.c")
        ios = self.get("ios/Sources/front_ios.m")
        android = self.get("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java")
        mac = self.get("host/front_mac.m")
        self.assertIn("DM_MOUSE_MIDDLE_DOWN", shared)
        self.assertIn("dm_world_mouse_map_toggle()", shared)
        self.assertIn("dm_ui_map_full(!dm_ui_map_is_full())", world)
        self.assertIn("_mouseMiddleDown", ios)
        self.assertIn("DM_MOUSE_MIDDLE_DOWN point:p", ios)
        self.assertIn("_mouseMiddleDown=NO", ios)
        self.assertIn("BUTTON_TERTIARY", android)
        self.assertIn("NSEventMaskOtherMouseDown", mac)

    def test_android_mouse_routes_existing_item_gesture(self):
        native = self.get("android/touch_android.c")
        front = self.get("android/app/src/main/java/com/xiaoboluo/d2r/TouchOverlay.java")
        self.assertIn("mouse_touch_on", native)
        self.assertIn("tc.mouse_item_mode=e.mouse", native)
        self.assertIn("DM_TC_MAX_TOUCH-1", native)
        self.assertIn("s<15", front)

    def test_monster_snapshot_source_is_verified(self):
        world = self.get("host/dm_world.c")
        self.assertIn("b+0x5b9290", world)
        self.assertIn("stat(unit,6,0)", world)
        self.assertIn("stat(unit,7,0)", world)
        self.assertIn("stat(unit,stat_id[z],0)>=100", world)
        self.assertIn("view.immune_mask", world)
        self.assertIn("view.tier", world)
        self.assertIn("strlcpy(view.details,dm_lang_zh()?zh_tier[rank]:en_tier[rank]", world)


    def test_monster_hold_is_bound_to_original_id(self):
        world = self.get("host/dm_world.c")
        mac = self.get("host/touch_mac.c")
        portable = self.get("host/dm_external_mouse.c")
        self.assertIn("g_mouse_lock", world)
        self.assertIn("w_find(1,id)", world)
        self.assertIn("dm_world_mouse_release(); g_lm.owned", mac)
        self.assertIn("dm_world_mouse_release();", portable)
        self.assertIn("A dead/disappeared monster ends the hold without walking", world)
        self.assertIn("mouse_repeat==2 ? mouse_tap_x : mouse_x", portable)
        self.assertIn("g_lm.repeat==2 ? g_lm.tap_x : g_lm.x",mac)

    def test_item_stays_held_until_explicit_drop(self):
        cursor = self.get("host/dm_cursor.c")
        mac = self.get("host/touch_mac.c")
        self.assertIn("physical_mouse_owns_item()", cursor)
        self.assertIn("dm_ui_hover_now(x,y);", cursor)
        self.assertIn("g_live_hover_latest", mac)
        self.assertIn("controlsMouseCaptured",self.get("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java"))
        self.assertIn("gameMouseCaptured",self.get("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java"))
        self.assertIn("fresh ? fresh : atomic_load(&g_live_hover_latest)", mac)

if __name__ == "__main__":
    unittest.main()
