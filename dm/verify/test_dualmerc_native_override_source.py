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

    def test_dead_mercanary_fails_before_native_replacement(self):
        source = SOURCE.read_text(encoding="utf-8")
        hire = source.split(
            "static int dm_dualmerc_hire_preflight(", 1)[1].split(
            "static void dm_dualmerc_pending_tick(", 1)[0]
        pending = source.split(
            "static void dm_dualmerc_pending_tick(", 1)[1].split(
            "static void dm_dualmerc_native_lookup(", 1)[0]
        self.assertIn(
            "lease.plan.action == DM_DUALMERC_ADD && roster.slot[0].dead", hire
        )
        self.assertIn("if (roster.slot[choice].dead)", hire)
        self.assertIn("if (roster.slot[target].dead)", pending)
        self.assertIn("atomic_store(&g_dm_trial_full_notice,5);", hire)
        self.assertIn("atomic_store(&g_dm_trial_full_notice,5);", pending)

    def test_native_offer_checks_authoritative_table_prefix(self):
        source = SOURCE.read_text(encoding="utf-8")
        verify = source.split(
            "static int dm_dualmerc_native_hire_rows_match(", 1)[1].split(
            "static int dm_dualmerc_native_offer(", 1)[0]
        offer = source.split(
            "static int dm_dualmerc_native_offer(", 1)[1].split(
            "static void dm_dualmerc_pending_reset(", 1)[0]
        self.assertIn("DM_DUALMERC_HIRE_STRIDE", verify)
        self.assertIn("dm_dualmerc_hire_check_ok(&result)", verify)
        self.assertIn("table_version>3", verify)
        self.assertIn("!dm_dualmerc_native_hire_rows_match(G,&wanted)", offer)
        self.assertLess(
            offer.index("!dm_dualmerc_native_hire_rows_match(G,&wanted)"),
            offer.index("*offer = (dm_merc_txn_offer)")
        )

    def test_failed_post_hire_confirmation_aborts_trial(self):
        source = SOURCE.read_text(encoding="utf-8")
        pending = source.split(
            "static void dm_dualmerc_pending_tick(", 1)[1].split(
            "static void dm_dualmerc_native_lookup(", 1)[0]
        self.assertIn(
            'die("DM_DUAL_MERC_TRIAL：原生替换未经过选定单位查询', pending
        )
        self.assertIn(
            'die("DM_DUAL_MERC_TRIAL：替换后身份未通过校验', pending
        )
        self.assertIn(
            'die("DM_DUAL_MERC_TRIAL：同幕替换后另一佣兵或新佣兵不匹配',
            source
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
