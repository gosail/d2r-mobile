"""Original game Controls tab must precede Custom in both native layouts.

Synthetic game layout: no user-owned assets or new settings windows required.
"""
import json
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from touch_layout import _settings_tabs, pad_settings


class NativeControlsTabOrder(unittest.TestCase):
    def test_touch_and_controller_keep_one_native_control_page(self):
        original = {
            "type": "SettingsPanel", "tabCount": 5,
            "inactiveFrames": [0, 0, 0, 0, 0],
            "activeFrames": [1, 1, 1, 1, 1],
            "textStrings": [
                "@OptionsMenuVideo", "@Audio", "@Gameplay",
                "@GameSettingsTab", "@Controls",
            ],
            "tabMessages": [
                "SettingsPanelMessage:CheckChanges:VideoOptionsPanel",
                "SettingsPanelMessage:CheckChanges:AudioOptionsPanel",
                "SettingsPanelMessage:CheckChanges:GameplayOptionsPanel",
                "SettingsPanelMessage:CheckChanges:GameSettingsPanel",
                "SettingsPanelMessage:CheckChanges:KeyBindingMenu",
            ],
        }
        source = json.dumps(original)
        touch, _ = _settings_tabs(source)
        pad = pad_settings(source)
        self.assertEqual(json.loads(touch), json.loads(pad))
        for result in (touch, pad):
            panel = json.loads(result)
            self.assertEqual(panel["tabCount"], 5)
            self.assertEqual(panel["textStrings"], [
                "@Audio", "@Gameplay", "@dmTabGameSettings",
                "@Controls", "@dmTabCustom",
            ])
            self.assertEqual(panel["tabMessages"][-2:], [
                "SettingsPanelMessage:CheckChanges:KeyBindingMenu",
                "SettingsPanelMessage:CheckChanges:VideoOptionsPanel",
            ])
            self.assertEqual(len(panel["inactiveFrames"]), 5)
            self.assertEqual(len(panel["activeFrames"]), 5)

    def test_wrong_native_layout_is_not_silently_rewritten(self):
        damaged = json.dumps({
            "tabCount": 3, "inactiveFrames": [0, 0, 0],
            "activeFrames": [1, 1, 1], "textStrings": ["A", "B", "C"],
            "tabMessages": ["X", "Y", "Z"],
        })
        with self.assertRaises(SystemExit):
            pad_settings(damaged)


if __name__ == "__main__":
    unittest.main()
