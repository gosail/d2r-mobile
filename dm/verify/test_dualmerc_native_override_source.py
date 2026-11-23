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
        self.assertIn("dm_dualmerc_trial_selected_target(", code)
        self.assertIn("g_dm_dualmerc_selected_id", code)
        self.assertIn("g_dm_dualmerc_pending.peer_id", code)
        self.assertIn("&roster,gear", code)
        self.assertLess(
            code.index("dm_dualmerc_trial_selected_target("),
            code.index("regs[0] = target;")
        )

    def test_truncated_native_hire_never_bypasses_trial_preflight(self):
        source = SOURCE.read_text(encoding="utf-8")
        preflight = source.split(
            "static int dm_dualmerc_hire_preflight(", 1)[1].split(
            "static void dm_dualmerc_pending_tick(", 1)[0]
        self.assertLess(
            preflight.index("if (!tk_rd(regs[0]+4, &opcode, 1)"),
            preflight.index("if (regs[1] != DM_DUALMERC_NATIVE_PACKET_BYTES)"))
        self.assertNotIn(
            "regs[1] < DM_DUALMERC_NATIVE_PACKET_BYTES", preflight
        )


    def test_zero_length_packet_has_no_readable_hire_opcode(self):
        source = SOURCE.read_text(encoding="utf-8")
        preflight = source.split(
            "static int dm_dualmerc_hire_preflight(", 1)[1].split(
            "static void dm_dualmerc_pending_tick(", 1)[0]
        self.assertIn("!regs[1] || regs[0] > UINT64_MAX-5u", preflight)
        self.assertLess(
            preflight.index("!regs[1] ||"),
            preflight.index("tk_rd(regs[0]+4, &opcode, 1)")
        )

    def test_native_candidate_gear_checked_before_selected_unlink(self):
        code = self.selected
        self.assertIn("dm_merc_trace_gear candidate_gear={0};", code)
        self.assertIn("dm_merc_probe_inventory(regs[19],&candidate_gear);", code)
        self.assertIn("dm_dualmerc_trial_candidate_empty(&candidate_gear)", code)
        self.assertLess(
            code.index("dm_dualmerc_trial_candidate_empty(&candidate_gear)"),
            code.index("regs[0] = target;")
        )

    def test_unverified_revive_candidates_are_only_observed(self):
        source = SOURCE.read_text(encoding="utf-8")
        obs = source.split("static void c2s_obs(", 1)[1].split(
            "    if (n >= 1 && b[0] == 0x20)", 1)[0]
        self.assertIn(
            "(b[0] == 0x36 || b[0] == 0x38 || b[0] == 0x62)", obs)
        self.assertIn("dm_dual_merc_probe_tick(", obs)
        self.assertNotIn("die(", obs)
        self.assertNotIn("return 1;", obs)

    def test_candidate_unit_id_is_unique_before_unlink(self):
        code = self.selected
        self.assertIn("tk_rd(regs[19] + 8, &candidate_id, 4)", code)
        self.assertIn("candidate_id,candidate_cls,&selected_slot)", code)
        self.assertLess(
            code.index("tk_rd(regs[19] + 8, &candidate_id, 4)"),
            code.index("regs[0] = target;")
        )

    def test_both_native_replacement_paths_preserve_peer_state(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertEqual(
            source.count("dm_dualmerc_trial_replacement_complete("), 2
        )
        pending = source.split("static void dm_dualmerc_pending_tick(", 1)[1].split(
            "static void dm_dualmerc_native_lookup(", 1)[0]
        self.assertIn("p->peer_act=roster.slot[1-target].act;", pending)
        self.assertIn("p->peer_dead=roster.slot[1-target].dead;", pending)
        self.assertIn("p->peer_dead,live.offered_act", pending)
        self.assertIn("g_dm_dualmerc_pending.peer_act,", source)
        self.assertIn("g_dm_dualmerc_pending.peer_dead,", source)

    def test_second_hire_cannot_silently_replace_original(self):
        source = SOURCE.read_text(encoding="utf-8")
        preflight = source.split("static int dm_dualmerc_hire_preflight(", 1)[1].split(
            "static void dm_dualmerc_pending_tick(", 1)[0]
        lookup = self.lookup
        dispatch = source.split(
            "    recv(regs[0], regs[1], regs[2], regs[3]);", 1)[1].split(
            "/* Direct same-Act replacement is synchronous.", 1)[0]
        self.assertIn("g_dm_dualmerc_add=(dm_dualmerc_add_intent)", preflight)
        self.assertIn("if (!g_dm_dualmerc_add.active) return;", lookup)
        self.assertIn("g_dm_dualmerc_add.keep_seen=1;", lookup)
        self.assertIn("g_dm_dualmerc_add.candidate_id=new_id;", lookup)
        self.assertIn("禁止回落原版替换", lookup)
        self.assertIn("if (g_dm_dualmerc_add.active)", dispatch)
        self.assertIn("!intent.link_verified", dispatch)
        self.assertIn("dm_dualmerc_trial_add_complete(", dispatch)
        self.assertIn("memset(&g_dm_dualmerc_add,0", dispatch)

    def test_native_second_hire_link_rejects_wrong_identity(self):
        source = SOURCE.read_text(encoding="utf-8")
        link = source.split(
            "if (num == SVC_HOST_DUALMERC_LINK && G &&", 1)[1].split(
            "static void hud_tick(", 1)[0]
        self.assertIn("g_dm_dualmerc_add.keep_seen", link)
        self.assertIn("g_dm_dualmerc_add.candidate_id", link)
        self.assertIn("dm_dualmerc_trial_add_complete(", link)
        self.assertIn("die(\"DM_DUAL_MERC_TRIAL：第二名原生绑定后身份错误", link)

    def test_replace_requires_exact_new_unit_and_refreshes_client(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertEqual(source.count("dm_dualmerc_trial_replacement_complete("), 2)
        self.assertIn("g_dm_dualmerc_pending.candidate_id = candidate_id;", source)
        self.assertIn("p->candidate_id,\n                    p->peer_act", source)
        self.assertIn("g_dm_dualmerc_pending.candidate_id,", source)
        self.assertEqual(source.count("dm_dualmerc_track_client_pair("), 3)
        self.assertIn("撤销此前客户端验证", source)
        self.assertIn("atomic_store(&g_dm_trial_verified_client,0);", source)

    def test_client_pair_requires_exact_two_unique_owner_entries(self):
        source = SOURCE.read_text(encoding="utf-8")
        client = source.split(
            "static int dm_dualmerc_client_both(", 1)[1].split(
            "/* Native replacement is complete only in the SERVER roster", 1)[0]
        self.assertIn("old_id == new_id", client)
        self.assertIn("if (!mask || (seen & mask) || ++count>DM_DUALMERC_SLOTS)", client)
        self.assertIn("return !node && seen == 3u && count == DM_DUALMERC_SLOTS;", client)

    def test_client_tick_pair_validation_is_read_only_and_cheap(self):
        source = SOURCE.read_text(encoding="utf-8")
        tracker = source.split("static int dm_dualmerc_trial_pet_pair(", 1)[1].split(
            "/* The NPC picker is an optional iOS-only frontend", 1)[0]
        self.assertIn("dm_dualmerc_trial_pet_nodes(player,&first)", tracker)
        self.assertIn("a==UINT32_MAX || b==UINT32_MAX || a==b", tracker)
        tick = source.split(
            "/* Server identity may change on death, area transition", 1)[1].split(
            "/* A running simulation frame MUST NOT execute", 1)[0]
        self.assertIn("dm_dualmerc_trial_pet_pair(player,&id0,&id1)", tick)
        self.assertNotIn("dm_dualmerc_runtime_roster(", tick)
        self.assertIn("atomic_store(&g_dm_trial_verified_client,0);", tick)

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

    def test_future_sidecar_commit_shares_atomic_fs_transaction(self):
        fs = (ROOT / "host" / "hle_fs.c").read_text(encoding="utf-8")
        header = (ROOT / "host" / "hle.h").read_text(encoding="utf-8")
        self.assertIn(
            "int (*g_dm_dualmerc_pre_commit_hook)(const char *workdir);", fs)
        self.assertIn(
            "extern int (*g_dm_dualmerc_pre_commit_hook)(const char *workdir);", header)
        self.assertIn(
            "void (*g_dm_pre_commit_hook[5])(const char *workdir);", fs)
        commit = fs.split('HLE_FORCE("_ZN2nn2fs6CommitEPKc"', 1)[1].split(
            'HLE_FORCE("_ZN2nn2fs7UnmountEPKc"', 1)[0]
        self.assertLess(
            commit.index("pthread_mutex_lock(&g_lock);"),
            commit.index("if (g_dm_dualmerc_pre_commit_hook)"))
        self.assertLess(
            commit.index("if (g_dm_dualmerc_pre_commit_hook)"),
            commit.index("int perr = atomic_exchange(&g_save_poison, 0);"))
        self.assertIn("save_poison(err);", commit)
        self.assertIn("if (perr) {", commit)
        self.assertIn("return RES_FAIL;", commit)

    def test_post_hire_inventory_must_still_be_empty(self):
        source = SOURCE.read_text(encoding="utf-8")
        policy = (ROOT / "host" / "dm_dualmerc_trial.h").read_text(
            encoding="utf-8")
        self.assertIn("dm_dualmerc_trial_post_gear_empty(", policy)
        self.assertEqual(source.count("dm_dualmerc_trial_add_complete("), 2)
        self.assertEqual(source.count("dm_dualmerc_trial_replacement_complete("), 2)
        self.assertIn("&after,gear,intent.original_id", source)
        self.assertIn("&after,gear,g_dm_dualmerc_add.original_id", source)
        self.assertIn("&after,ag,target_id,peer_id", source)
        self.assertIn("&after,gear,g_dm_dualmerc_pending.target_id", source)

    def test_native_add_and_replacement_require_same_monster_classes(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("slot->monster_class = cls;", source)
        self.assertIn("g_dm_dualmerc_pending.candidate_class = candidate_cls;", source)
        self.assertIn("g_dm_dualmerc_add.candidate_class=new_cls;", source)
        self.assertIn("old_class != g_dm_dualmerc_add.original_class", source)
        self.assertEqual(
            source.count("dm_dualmerc_trial_classes_preserved("), 4)
        self.assertIn("after.slot[0].monster_class != intent.original_class", source)

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
        self.assertIn("table_version<1 || table_version>3", verify)
        self.assertIn("!dm_dualmerc_native_hire_rows_match(G,&wanted)", offer)
        self.assertLess(
            offer.index("!dm_dualmerc_native_hire_rows_match(G,&wanted)"),
            offer.index("*offer = (dm_merc_txn_offer)")
        )

    def test_first_vanilla_hire_is_observed_but_never_blocked(self):
        source = SOURCE.read_text(encoding="utf-8")
        preflight = source.split(
            "static int dm_dualmerc_hire_preflight(", 1)[1].split(
            "static void dm_dualmerc_pending_tick(", 1)[0]
        first = preflight.split("if (count == 0) {", 1)[1].split(
            "dm_merc_txn_offer offer;", 1)[0]
        self.assertIn("dm_dualmerc_native_offer(", first)
        self.assertIn("return 0;", first)
        self.assertNotIn("return 1;", first)

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
