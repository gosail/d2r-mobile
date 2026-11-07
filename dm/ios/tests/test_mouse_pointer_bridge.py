"""Structural regression checks for iPadOS physical mouse input wiring.
Run: python3 -m unittest discover -s dm/ios/tests -p 'test_mouse_pointer_bridge.py'
These checks are game-data-free; real pointer behavior still needs device testing.
"""
from pathlib import Path
import unittest

DM = Path(__file__).resolve().parents[2]

class PhysicalMouseWiring(unittest.TestCase):
    def read(self, path):
        return (DM / path).read_text(encoding="utf-8")

    def test_ipad_pointer_not_in_finger_compatibility_mode(self):
        project = self.read("ios/project.yml")
        self.assertIn("UIApplicationSupportsIndirectInputEvents: true", project)
        self.assertIn("UITouchTypeIndirectPointer", self.read("ios/Sources/front_ios.m"))

    def test_hover_end_does_not_release_mouse_button(self):
        header = self.read("host/dm_external_mouse.h")
        backend = self.read("host/dm_external_mouse.c")
        front = self.read("ios/Sources/front_ios.m")
        self.assertIn("DM_MOUSE_HOVER_END", header)
        self.assertIn("mouse_hover_valid=0; dm_world_hover_end(); continue;", backend)
        self.assertIn("dm_external_mouse_event(DM_MOUSE_HOVER_END", front)
        self.assertIn("if (e->op == DM_MOUSE_LEAVE) { mouse_clear(); continue; }", backend)

    def test_native_target_info_replaces_custom_hover_layers(self):
        ios = self.read("ios/Sources/front_ios.m")
        mac = self.read("host/front_mac.m")
        android = self.read("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java")
        target = self.read("host/hle_target.c")
        self.assertNotIn("worldHoverSync", ios)
        self.assertNotIn("hoverTimer", mac)
        self.assertIn("new MouseHoverOverlay", android)
        self.assertIn("dm_world_hover_monster_id()", target)

    def test_physical_mouse_uses_hud_and_panel_gestures(self):
        front = self.read("ios/Sources/front_ios.m")
        self.assertIn("NSMutableSet<UITouch *> *_pointerAsTouch;", front)
        self.assertIn("(_navigation || [self dmMouseHitsTouchControl:p])", front)
        self.assertIn("if(realFinger) { BOOL wasPad=_padMode; dm_gamepad_touched();", front)
        self.assertIn("if (_pointerAsTouch.count) return;", front)

    def test_monster_hover_and_stationary_panel_hover(self):
        world = self.read("host/dm_world.c")
        mouse = self.read("host/dm_external_mouse.c")
        front = self.read("ios/Sources/front_ios.m")
        self.assertIn("cat != 5 && cat != 1 && cat != 0xa", world)
        self.assertIn("view.kind=cat==5 ? 1 : 3;", world)
        self.assertIn("mouse_hover_valid && !mouse_down", mouse)
        self.assertIn("mouse_hover_valid && !mouse_down", mouse)
        self.assertNotIn("worldHoverSync", front)

    def test_mouse_left_uses_native_item_pickup_in_all_three_ports(self):
        gesture = self.read("ios/Sources/dm_touch_controls.c")
        ios = self.read("ios/Sources/front_ios.m")
        mac = self.read("host/touch_mac.c")
        android = self.read("android/touch_android.c")
        self.assertIn(".mouse_item=!!(s->navigation_mode && s->mouse_item_mode)", gesture)
        self.assertIn("done.mouse_item && s->pointer_item == 1", gesture)
        self.assertIn("pointer_pulse_start(s, done.aim_origin_x, done.aim_origin_y, 1)", gesture)
        self.assertIn("_controls.mouse_item_mode=(t.type==UITouchTypeIndirectPointer)", ios)
        self.assertIn("g_touch.state.mouse_item_mode = g_live_on;", mac)
        self.assertIn("t.type==UITouchTypeIndirectPointer?0:mouseLift", ios)
        self.assertIn("ep.y+(mouse?lift:0)", ios)
        self.assertIn("tc.mouse_item_mode=e.mouse;", android)

    def test_native_monster_hud_uses_original_target_selector(self):
        world = self.read("host/dm_world.c")
        target = self.read("host/hle_target.c")
        self.assertIn("dm_world_hover_monster_id", world)
        self.assertIn("candidate_id==hover", target)
        self.assertIn("gate_obs_set(OBS_TARGET_RANK, target_rank_obs)", target)
        self.assertIn("new MouseHoverOverlay", self.read("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java"))

    def test_mac_right_and_middle_buttons_use_same_actions(self):
        mac = self.read("host/front_mac.m")
        self.assertIn("dm_ui_mouse_right_use(nx,ny);", mac)
        self.assertIn("NSEventMaskOtherMouseDown", mac)
        self.assertIn("dm_world_mouse_map_toggle();", mac)
        self.assertIn("rightItemDown=NO;", mac)

    def test_native_system_pointer_feedback_no_custom_item_sprite(self):
        c = self.read("host/dm_cursor.c")
        mac = self.read("host/front_mac.m")
        ios = self.read("ios/Sources/front_ios.m")
        java = self.read("android/app/src/main/java/com/xiaoboluo/d2r/DmActivity.java")
        self.assertIn("int dm_mouse_pointer_hint(void)", c)
        self.assertIn("NSCursor.closedHandCursor", mac)
        self.assertIn("UIPointerStyle hiddenPointerStyle", ios)
        self.assertIn("PointerIcon.TYPE_GRABBING", java)

    def test_prepared_ci_build_without_npc_source_header(self):
        world = self.read("host/dm_world.c")
        self.assertIn('#if __has_include("dm_hover_names.h")', world)
        self.assertIn('case 148: return "Akara";', world)
        self.assertIn('case 154: return "Charsi";', world)
        self.assertIn('case 511: return "Larzuk";', world)
        self.assertIn('default: return NULL;', world)

if __name__ == "__main__":
    unittest.main()
