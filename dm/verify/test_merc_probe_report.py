#!/usr/bin/env python3
import unittest
from merc_probe_report import analyze, parse_snapshot


def snapshot(tick, srv, cli, owner=4):
    s = ",".join(srv)
    c = ",".join(cli)
    return (f"DM_DUAL_MERC_PROBE：拍 {tick} 玩家 id {owner} 服务端宠物类型7={len(srv)} "
            f"[{s}] 客户端属主佣兵={len(cli)} [{c}]")


def cmd(tick, owner=4):
    return (f"DM_DUAL_MERC_PROBE：拍 {tick} 玩家 id {owner} "
            "收到候选雇佣包 0x36 len=17 前16字节=abcdef")


OLD = "100:271:1:a1:inv2/1:h000000ff"
NEW = "101:338:1:a2:inv0/1:h00000000"
C_OLD = "100:271:1:f0:a1"
C_NEW = "101:338:1:f0:a2"


class MercLogAnalysisTests(unittest.TestCase):
    def test_same_tick_snapshot_is_before_not_after(self):
        result = analyze([snapshot(39, [OLD], [C_OLD]), cmd(40),
                          snapshot(40, [OLD], [C_OLD]),
                          snapshot(41, [NEW], [C_NEW])])
        x = result["attempts"][0]
        self.assertEqual(x["observation"], "replacement_in_server_list")
        self.assertEqual(x["disappeared_server_ids"], [100])
        self.assertEqual(x["new_server_ids"], [101])
        self.assertEqual(x["old_inventory_observations"]["100"]["count"], 2)
        self.assertEqual(x["old_inventory_observations"]["100"]["status"], 1)

    def test_two_lists_but_no_combat_claim(self):
        x = analyze([cmd(40), snapshot(40, [OLD], [C_OLD]),
                     snapshot(41, [OLD, NEW], [C_OLD, C_NEW])])["attempts"][0]
        self.assertTrue(x["both_lists_two"])
        self.assertEqual(x["observation"], "two_units_in_both_lists")
        self.assertIn("cannot prove", analyze([])["limitations"])

    def test_server_only_is_not_confirmed(self):
        x = analyze([cmd(40), snapshot(40, [OLD], [C_OLD]),
                     snapshot(41, [OLD, NEW], [C_OLD])])["attempts"][0]
        self.assertEqual(x["observation"], "two_units_in_server_list_only")
        self.assertFalse(x["both_lists_two"])

    def test_missing_packet_or_after_does_not_infer_success(self):
        self.assertEqual(analyze([snapshot(41, [OLD, NEW], [C_OLD, C_NEW])])
                         ["candidate_requests"], 0)
        x = analyze([cmd(40), snapshot(40, [OLD], [C_OLD])])["attempts"][0]
        self.assertEqual(x["observation"], "insufficient_snapshots")

    def test_truncated_or_legacy_gear_entries_remain_unverified(self):
        s = parse_snapshot(snapshot(40, ["200:560:12:a5:inv5/3:hffffffff",
                                          "201:561:2:a5"], []))
        self.assertEqual(s["server"][200]["inventory_status"], 3)
        self.assertIsNone(s["server"][201]["inventory_status"])

    def test_new_session_tick_reset_prevents_fabricated_transition(self):
        r = analyze([cmd(400), snapshot(400, [OLD], [C_OLD]),
                     snapshot(1, [NEW], [C_NEW])])["attempts"][0]
        self.assertEqual(r["observation"], "insufficient_snapshots")

    def test_large_tick_distance_does_not_claim_replacement(self):
        r = analyze([cmd(40), snapshot(40, [OLD], [C_OLD]),
                     snapshot(151, [NEW], [C_NEW])])["attempts"][0]
        self.assertEqual(r["observation"], "insufficient_snapshots")


if __name__ == "__main__":
    unittest.main()
