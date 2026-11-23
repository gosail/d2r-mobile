#!/usr/bin/env python3
"""Source-level regressions for the offline iPad mercenary replacement chooser.

These checks catch a stale dialog dismissing or consuming the nonce of the
next hire transaction. They are not substitutes for UIKit/device tests.
"""
from pathlib import Path
import re
import unittest

FRONT = Path(__file__).resolve().parents[1] / "Sources" / "front_ios.m"
CHANNEL = FRONT.parents[2] / "host" / "dm_dualmerc_ui_channel.h"


def body_between(source: str, start: str, end: str) -> str:
    a = source.index(start)
    b = source.index(end, a + len(start))
    return source[a:b]


class TestDualMercChooserSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = FRONT.read_text(encoding="utf-8")
        cls.channel = CHANNEL.read_text(encoding="utf-8")

    def test_alert_identity_is_owned_by_ui(self):
        request = body_between(
            self.source, "void dm_dualmerc_ui_request(", "int dm_dualmerc_ui_poll(")
        self.assertIn("static uint64_t g_dm_merc_choice_alert_nonce;", self.source)
        self.assertIn("g_dm_merc_choice_alert_nonce = nonce;", request)
        self.assertIn("g_dm_merc_choice_alert_nonce = 0;", request)
        self.assertIn("g_dm_merc_choice_alert == weakA", request)

    def test_dead_target_is_visibly_disabled(self):
        request = body_between(
            self.source, "void dm_dualmerc_ui_request(", "int dm_dualmerc_ui_poll(")
        self.assertIn("unsigned dead_mask", request)
        self.assertIn("dead_mask == 3u", request)
        self.assertIn("BOOL dead = !!(dead_mask & (1u << slot));", request)
        self.assertIn("option.enabled = !dead;", request)
        self.assertIn("已死亡，暂不可替换", request)

    def test_cancel_never_dismisses_successor_alert(self):
        cancel = body_between(
            self.source, "void dm_dualmerc_ui_cancel(", "// 软键盘输入框")
        self.assertRegex(
            cancel, r"if\s*\(g_dm_merc_choice_alert_nonce\s*!=\s*nonce\)\s*return;")
        self.assertLess(
            cancel.index("g_dm_merc_choice_alert_nonce != nonce"),
            cancel.index("dismissViewControllerAnimated:"))
        self.assertIn("g_dm_merc_choice_alert_nonce = 0;", cancel)

    def test_nonce_and_result_are_protected_by_one_mutex(self):
        self.assertIn('#include "../../host/dm_dualmerc_ui_channel.h"', self.source)
        helper = body_between(
            self.channel, "static inline int dm_dualmerc_ui_channel_answer(",
            "static inline int dm_dualmerc_ui_channel_poll(")
        self.assertIn("pthread_mutex_lock(&c->mutex);", helper)
        self.assertIn("c->nonce == nonce", helper)
        self.assertIn("c->answer == DM_DUALMERC_UI_PENDING", helper)
        self.assertIn("c->answer=answer;", helper)
        self.assertLess(
            helper.index("pthread_mutex_lock(&c->mutex);"),
            helper.index("c->answer=answer;"))
        self.assertLess(
            helper.index("c->answer=answer;"),
            helper.index("pthread_mutex_unlock(&c->mutex);"))

    def test_stale_alert_handler_uses_atomic_nonce_result_commit(self):
        request = body_between(
            self.source, "void dm_dualmerc_ui_request(", "int dm_dualmerc_ui_poll(")
        self.assertIn(
            "dm_dualmerc_ui_channel_answer(&g_dm_merc_choice_channel,nonce,(int)selected);",
            request)
        self.assertIn(
            "dm_dualmerc_ui_channel_answer(&g_dm_merc_choice_channel,nonce,-1);",
            request)
        self.assertIn("dm_dualmerc_ui_channel_pending(", request)
        self.assertIn("dm_dualmerc_ui_channel_begin(", request)
        self.assertNotIn("atomic_load(&g_dm_merc_choice_nonce)", request)

    def test_old_poll_and_cancel_cannot_erase_new_nonce(self):
        poll = body_between(
            self.source, "int dm_dualmerc_ui_poll(", "void dm_dualmerc_ui_cancel(")
        cancel = body_between(
            self.source, "void dm_dualmerc_ui_cancel(", "// 软键盘输入框")
        self.assertIn("dm_dualmerc_ui_channel_poll(", poll)
        self.assertIn("dm_dualmerc_ui_channel_cancel(", cancel)
        for name in ("dm_dualmerc_ui_channel_poll", "dm_dualmerc_ui_channel_cancel"):
            self.assertIn("static inline int " + name, self.channel)
        self.assertIn("nonce > c->last_nonce", self.channel)


if __name__ == "__main__":
    unittest.main()
