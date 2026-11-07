"""Augment the *original* KeyBindingMenuHD row template in place.

The guest still owns KeyBindingPanel/BindingsList and its .ctl button widgets.
Two extra buttons beside the original pair edit the character .key through
native UI messages; no UIKit, AppKit or Android settings page is created.
The same resource is shared by controller and touch layout prefixes.
"""
import copy
import json
import re

LAYOUT = "keybindingmenuhd.json"
# Original native row width is 1494; preserve the outer panel, scrollview,
# header, list-item class, focus selector and both gamepad button names.
ROW_WIDTH = 1494
NAME_X, NAME_W = 20, 380
PC_X, PC_SLOT_W = 415, 166
PAD1_X, PAD1_W = 760, 365
PAD2_X, PAD2_W = 1134, 358


def _parse(raw):
    # Extracted original JSON permits trailing commas and comment lines.
    without_comments = re.sub(r"(?m)^\s*//[^\n]*$", "", raw)
    without_comments = re.sub(r"/\*[\s\S]*?\*/", "", without_comments)
    without_comments = re.sub(r",(\s*[}\]])", r"\1", without_comments)
    return json.loads(without_comments)


def _child(parent, name, kind=None):
    found = [c for c in parent.get("children", [])
             if c.get("name") == name and (kind is None or c.get("type") == kind)]
    if len(found) != 1:
        raise ValueError("native KeyBindingMenu changed: %s (%d)" % (name, len(found)))
    return found[0]


def integrate(original):
    root = _parse(original)
    if root.get("type") != "KeyBindingPanel" or root.get("name") != "KeyBindingMenu":
        raise ValueError("expected the game's native KeyBindingPanel")
    header = _child(root, "HeaderTable", "TableWidget")
    list_widget = _child(root, "BindingsList", "ListWidget")
    template = _child(list_widget, "Template", "ListItemWidget")
    item = _child(template, "Binding", "KeyBindingItemWidget")

    pbutton = _child(item, "PrimaryKeyButton", "ButtonWidget")
    sbutton = _child(item, "SecondaryKeyButton", "ButtonWidget")
    wrapper = _child(item, "BindingNameWrapper", "ButtonWidget")
    title_row = _child(header, "Row", "TableRowWidget")
    columns = header["fields"]["columns"]
    if len(columns) != 3 or len(title_row["children"]) != 3:
        raise ValueError("native controls are not the verified three-column layout")
    if item["fields"]["rect"].get("width") != ROW_WIDTH:
        raise ValueError("native KeyBindingItemWidget width changed")
    if len([x for x in template.get("children", []) if x.get("name") == "Binding"]) != 1:
        raise ValueError("binding template changed")

    # Preserve the game's own table cell alignment definitions, adjusting
    # only their widths. The controller name, first and alternate buttons
    # remain native KeyBindingItemWidget children.
    widths = (410, 342, 377, 365)
    c0 = copy.deepcopy(columns[0])
    c1 = copy.deepcopy(columns[1])
    c2 = copy.deepcopy(columns[2])
    columns[:] = [c0, copy.deepcopy(c1), c1, c2]
    for col, width in zip(columns, widths):
        col["width"] = width

    name_title = _child(title_row, "BindingNameTitle", "TextBoxWidget")
    pad_title = _child(title_row, "PrimaryKeyTitle", "TextBoxWidget")
    alt_title = _child(title_row, "SecondaryKeyTitle", "TextBoxWidget")
    pc_title = copy.deepcopy(pad_title)
    pc_title["name"] = "TouchPcKeyTitle"
    pc_title["fields"]["text"] = "@dmControlsKeyboard"
    title_row["children"].insert(1, pc_title)
    pad_title["fields"]["text"] = "@dmControlsPadOne"
    alt_title["fields"]["text"] = "@dmControlsPadTwo"

    wrapper["fields"]["rect"].update(x=NAME_X, width=NAME_W)
    name_text = _child(wrapper, "BindingName", "TextBoxWidget")
    name_text["fields"].setdefault("rect", {}).update(width=NAME_W-20)
    pbutton["fields"]["rect"].update(x=PAD1_X, width=PAD1_W)
    sbutton["fields"]["rect"].update(x=PAD2_X, width=PAD2_W)

    # The original list widget clones the complete Template for every
    # binding. Our buttons are its siblings, not children of the guest's
    # KeyBindingItemWidget, so its native .ctl message handler is untouched.
    original_key_text = _child(pbutton, "PrimaryKeyName", "TextBoxWidget")
    for slot in (0, 1):
        button = copy.deepcopy(pbutton)
        button["name"] = "TouchPcKeyButton%d" % slot
        button["fields"]["rect"] = {
            "x": PC_X + (PC_SLOT_W + 5)*slot,
            "y": 0, "width": PC_SLOT_W, "height": 58,
        }
        # An unknown PanelManager ClosePanel target is an existing
        # project pattern for sending host-consumed UI messages without
        # closing the real KeyBindingMenu.
        button["fields"]["onClickMessage"] = "PanelManager:ClosePanel:TouchPcBind%d" % slot
        button["children"] = [copy.deepcopy(original_key_text)]
        label = button["children"][0]
        label["name"] = "TouchPcKeyName%d" % slot
        label["fields"]["text"] = "—"
        label["fields"].setdefault("rect", {}).update(x=8, y=0, width=PC_SLOT_W-16, height=58)
        template["children"].append(button)

    if len(title_row["children"]) != 4:
        raise ValueError("native control header now has wrong column count")
    return json.dumps(root, ensure_ascii=False, indent=2) + "\n"
