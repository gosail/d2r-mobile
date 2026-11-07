"""The shared settings bar must retain the game's Controls tab and append Custom.
No player game assets are needed to exercise the tab-array transformation.
"""
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from touch_layout import _settings_tabs, pad_settings, _settings_no_legend
from controls_layout import integrate as integrate_native_controls

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


    def test_keyboard_and_gamepad_share_one_guest_binding_row(self):
        text = lambda name: {"type":"TextBoxWidget","name":name,
                             "fields":{"text":"", "rect":{}}}
        button = lambda name, child: {
            "type":"ButtonWidget","name":name,
            "fields":{"rect":{"x":0,"y":0,"width":500,"height":58},
                      "onClickMessage":"GuestOwnsGamepadBinding"},
            "children":[text(child)]
        }
        wrapper = button("BindingNameWrapper","BindingName")
        bind = {"type":"KeyBindingItemWidget","name":"Binding",
                "fields":{"rect":{"width":1494,"height":58}},
                "children":[wrapper,
                            button("PrimaryKeyButton","PrimaryKeyName"),
                            button("SecondaryKeyButton","SecondaryKeyName"),
                            {"type":"Widget","name":"BindingSelector"}]}
        template={"type":"ListItemWidget","name":"Template",
                  "children":[bind],"fields":{"rect":{"width":1494,"height":50}}}
        header={"type":"TableWidget","name":"HeaderTable",
                "fields":{"columns":[{"width":530},{"width":512},{"width":450}]},
                "children":[{"type":"TableRowWidget","name":"Row",
                             "children":[text("BindingNameTitle"),
                                         text("PrimaryKeyTitle"),
                                         text("SecondaryKeyTitle")]}]}
        native={"type":"KeyBindingPanel","name":"KeyBindingMenu",
                "children":[header,
                            {"type":"ListWidget","name":"BindingsList",
                             "children":[template]}]}
        out=json.loads(integrate_native_controls(json.dumps(native)))
        self.assertEqual(out["type"],"KeyBindingPanel")
        self.assertEqual(out["name"],"KeyBindingMenu")
        head=out["children"][0]
        self.assertEqual(len(head["fields"]["columns"]),4)
        self.assertEqual(sum(c["width"] for c in head["fields"]["columns"]),1494)
        self.assertEqual(
            [c["name"] for c in head["children"][0]["children"]],
            ["BindingNameTitle","TouchPcKeyTitle","PrimaryKeyTitle","SecondaryKeyTitle"])
        cloned=out["children"][1]["children"][0]
        names=[c["name"] for c in cloned["children"]]
        self.assertEqual(names,["Binding","TouchPcKeyButton0","TouchPcKeyButton1"])
        guest=cloned["children"][0]
        self.assertEqual([c["name"] for c in guest["children"]][:3],
                         ["BindingNameWrapper","PrimaryKeyButton","SecondaryKeyButton"])
        for guest_button in guest["children"][1:3]:
            self.assertEqual(guest_button["fields"]["onClickMessage"],
                             "GuestOwnsGamepadBinding")
        for slot in (0,1):
            pc=cloned["children"][slot+1]
            self.assertEqual(pc["fields"]["onClickMessage"],
                             f"PanelManager:ClosePanel:TouchPcBind{slot}")
            self.assertEqual(pc["children"][0]["name"],f"TouchPcKeyName{slot}")

if __name__ == "__main__":
    unittest.main()
