#!/usr/bin/env python3
"""No proprietary binary needed: exact 3.3 callsite and host SVC encodings."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from dualmerc_native_cuts import (BUILD_ID, SITES, branch_word, svc_word,
                                  host_audit_cuts)


class NativeMercCallsiteTests(unittest.TestCase):
    def test_verified_original_calls_are_expected(self):
        cuts = host_audit_cuts(BUILD_ID)
        self.assertEqual(len(cuts), 4)
        self.assertEqual([c[1] for c in cuts],
                         ["baee0094", "10e50094", "990f0594", "70e80094"])
        self.assertEqual([c[2] for c in cuts],
                         ["811200d4", "211200d4", "411200d4", "611200d4"])
        self.assertTrue(all(c[4] == "仅宿主" for c in cuts))
        self.assertEqual([c[0] for c in cuts], [0x687648, 0x6877f0, 0x68786c, 0x687940])

    def test_call_targets_do_not_drift(self):
        for site, target, _, _ in SITES:
            self.assertEqual(branch_word(site, target) & 0xfc000000, 0x94000000)

    def test_reject_unverified_build_and_bad_alignment(self):
        with self.assertRaisesRegex(ValueError, "unverified"):
            host_audit_cuts("wrong")
        with self.assertRaises(ValueError):
            branch_word(0, 3)
        with self.assertRaises(ValueError):
            branch_word(0, 1 << 27)
        with self.assertRaises(ValueError):
            branch_word(0, -(1 << 27) - 4)
        with self.assertRaises(ValueError):
            svc_word(0x7f)


if __name__ == "__main__":
    unittest.main()
