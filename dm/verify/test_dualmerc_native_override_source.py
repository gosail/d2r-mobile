#!/usr/bin/env python3
"""Source contract for the disposable offline mercenary native hook.

No claim that these checks verify the actual Switch Guest or the native ABI.
They only prevent the dangerous fallback to the singleton getter on an
unverified selected target (which can unlink the wrong mercenary).
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "host" / "diag.c"


class TestNativeSelectedMercFailStop(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = SOURCE.read_text(encoding="utf-8")
        cls.lookup = source.split(
            "static void dm_dualmerc_native_lookup(", 1)[1].split(
            "/* Exact verified 3.3 native hire call-site gates.", 1)[0]
        cls.selected = cls.lookup.split(
            "/* A selected replacement, including a different Act", 1
        )[1].split("if (!dm_dualmerc_trial_enabled() || !old_unit", 1)[0]

    def test_selected_and_peer_unit_ids_are_rechecked(self):
        code = self.selected
        self.assertIn("g_dm_dualmerc_pending.target_id ==", code)
        self.assertIn("g_dm_dualmerc_pending.peer_id", code)
        self.assertIn("roster.slot[j].runtime_unit_id", code)
        self.assertIn("if (count == 2 && peer_present && regs[19]", code)
        self.assertLess(
            code.index("if (count == 2 && peer_present && regs[19]"),
            code.index("regs[0] = target;")
        )

    def test_failure_must_not_fall_back_to_unselected_native_unit(self):
        code = self.selected
        self.assertIn("die(\"DM_DUAL_MERC_TRIAL", code)
        self.assertLess(
            code.index("die(\"DM_DUAL_MERC_TRIAL"),
            code.rindex("return;")
        )
        self.assertNotIn("regs[0] = old_unit", code)


if __name__ == "__main__":
    unittest.main()
