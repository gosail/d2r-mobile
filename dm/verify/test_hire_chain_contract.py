#!/usr/bin/env python3
"""Synthetic ARM64 branches; no game binaries are required for these tests."""
import struct
import unittest
from hire_chain_contract import decode_branch, verify


def branch_at(image, at, target, link):
    diff = target - at
    assert diff % 4 == 0
    v = (0x94000000 if link else 0x14000000) | ((diff // 4) & 0x3ffffff)
    struct.pack_into("<I", image, at, v)


class HireChainContractTests(unittest.TestCase):
    def test_forward_branch_and_backward_call(self):
        image = bytearray(0x200)
        branch_at(image, 0x20, 0x80, False)
        branch_at(image, 0xe0, 0x40, True)
        self.assertEqual(decode_branch(image, 0x20, 0x200), ("B", 0x80))
        self.assertEqual(decode_branch(image, 0xe0, 0x200), ("BL", 0x40))
        report = verify(image, 0x200, ((0x20, "B", 0x80),
                                       (0xe0, "BL", 0x40)))
        self.assertEqual(report["verified_sites"], 2)

    def test_reject_changed_opcode_or_branch_target(self):
        image = bytearray(0x200)
        branch_at(image, 0x40, 0x80, True)
        with self.assertRaisesRegex(ValueError, "changed"):
            verify(image, 0x200, ((0x40, "B", 0x80),))
        with self.assertRaisesRegex(ValueError, "changed"):
            verify(image, 0x200, ((0x40, "BL", 0x84),))
        struct.pack_into("<I", image, 0x40, 0xd503201f)
        with self.assertRaisesRegex(ValueError, "neither B nor BL"):
            decode_branch(image, 0x40, 0x200)

    def test_reject_out_of_text_and_unaligned(self):
        b = bytearray(0x100)
        branch_at(b, 0x40, 0x100, True)
        with self.assertRaises(ValueError):
            decode_branch(b, 0x40, 0x100)
        with self.assertRaises(ValueError):
            decode_branch(b, 0x3, 0x100)
        with self.assertRaises(ValueError):
            decode_branch(b, 0xff, 0x100)
        with self.assertRaises(ValueError):
            decode_branch(b, -4, 0x100)


if __name__ == "__main__":
    unittest.main()
