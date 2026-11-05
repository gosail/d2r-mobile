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
        self.assertIn("if (e->op == DM_MOUSE_HOVER_END) { dm_world_hover_end(); continue; }", backend)
        self.assertIn("dm_external_mouse_event(DM_MOUSE_HOVER_END", front)
        self.assertIn("if (e->op == DM_MOUSE_LEAVE) { mouse_clear(); continue; }", backend)

    def test_world_name_labels_are_not_clipped_to_bottom_ui(self):
        front = self.read("ios/Sources/front_ios.m")
        self.assertIn("CALayer *hoverHost=g_full && self.superview ? self.superview.layer : self.layer;", front)
        self.assertIn("wr=[self convertRect:wr toView:self.superview]", front)

    def test_prepared_ci_build_without_npc_source_header(self):
        world = self.read("host/dm_world.c")
        self.assertIn('#if __has_include("dm_hover_names.h")', world)
        self.assertIn('case 148: return "Akara";', world)
        self.assertIn('case 154: return "Charsi";', world)
        self.assertIn('case 511: return "Larzuk";', world)
        self.assertIn('default: return NULL;', world)

if __name__ == "__main__":
    unittest.main()
