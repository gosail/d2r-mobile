"""The shared settings bar must retain the game's Controls tab and append Custom.
No player game assets are needed to exercise the tab-array transformation.
"""
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from touch_layout import _settings_tabs, pad_settings, _settings_no_legend

class ControlsTabs(unittest.TestCase):
    def test_control_between_game_settings_and_custom(self):
        src = json.dumps({"tabCount":5,
            "inactiveFrames":[0]*5,
            "activeFrames":[1]*5,
            "textStrings":["@OptionsMenuVideo","@Audio","@Gameplay","@GameSettingsTab","@Controls"],
            "tabMessages":["SettingsPanelMessage:CheckChanges:VideoOptionsPanel",
                           "SettingsPanelMessage:CheckChanges:AudioOptionsPanel",
                           "SettingsPanelMessage:CheckChanges:GameplayOptionsPanel",
                           "SettingsPanelMessage:CheckChanges:GameSettingsPanel",
                           "SettingsPanelMessage:CheckChanges:KeyBindingMenu"]})
        actual, _ = _settings_tabs(src)
        out = json.loads(actual)
        self.assertEqual(out["tabCount"], 5)
        self.assertEqual(out["textStrings"], ["@Audio","@Gameplay","@dmTabGameSettings","@Controls","@dmTabCustom"])
        self.assertEqual(len(out["inactiveFrames"]), 5)
        self.assertEqual(len(out["activeFrames"]), 5)
        self.assertEqual(out["tabMessages"][-2:], [
            "SettingsPanelMessage:CheckChanges:KeyBindingMenu",
            "SettingsPanelMessage:CheckChanges:VideoOptionsPanel"])
        # The controller tab layout must show the same Controls-before-Custom
        # ordering; a simple Video-label replacement would put Custom first.
        self.assertEqual(json.loads(pad_settings(src)), out)
        # A second native controller settings resource sometimes owns the
        # actual tabs, rather than acting as a legend-only wrapper.
        wrapper = dict(json.loads(src))
        wrapper["children"]=[{"type":"ButtonLegendWidget","name":"SettingsLegend",
                              "fields":{"rect":{"x":0,"y":0}}}]
        plain, _ = _settings_no_legend(json.dumps(wrapper))
        self.assertEqual(json.loads(plain)["textStrings"],out["textStrings"])
        self.assertIn('"@dmTabCustom"',plain)
        self.assertNotIn('"@OptionsMenuVideo"',plain)

if __name__ == "__main__":
    unittest.main()
