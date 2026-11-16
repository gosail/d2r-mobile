#!/usr/bin/env python3
"""Asset-free tests for the strictly read-only NSO hire handler discovery."""
import struct
import unittest
from hire_handler import (handler_address, calls_from_function, inspect,
                          R_AARCH64_RELATIVE, OPCODES)

TABLE = 0x200


def fixture():
    image = bytearray(0x500)
    # Original 0x36, 0x38 and 0x62 command handlers.
    positions = {0x36: 0x40, 0x38: 0x80, 0x62: 0xc0}
    relocs = []
    for op, target in positions.items():
        relocs.append((TABLE + op * 8, R_AARCH64_RELATIVE, None, None, target))
    # BL from address 0x40 to 0x70, and RET.
    struct.pack_into("<II", image, 0x40, 0x9400000C, 0xD65F03C0)
    return bytes(image), positions, relocs


class HireHandlerTests(unittest.TestCase):
    def test_rela_takes_priority_over_raw_slot(self):
        image, _, relocs = fixture()
        mutable = bytearray(image)
        struct.pack_into("<Q", mutable, TABLE + 0x36 * 8, 0x120)
        found = handler_address(mutable, 0x180, TABLE, 0x36, [relocs[0]])
        self.assertEqual(found["handler"], "0x40")
        self.assertEqual(found["source"], "rela")

    def test_raw_pointer_and_missing_handler(self):
        image, _, _ = fixture()
        mutable = bytearray(image)
        struct.pack_into("<Q", mutable, TABLE + 0x36 * 8, 0x44)
        self.assertEqual(handler_address(mutable, 0x180, TABLE, 0x36, [])["handler"], "0x44")
        self.assertIsNone(handler_address(mutable, 0x180, TABLE, 0x38, []))

    def test_invalid_handler_fails_closed(self):
        image, _, relocs = fixture()
        p = TABLE + 0x36 * 8
        for target in (-4, 0x41, 0x180, 0x500):
            with self.subTest(target=target), self.assertRaises(ValueError):
                handler_address(image, 0x180, TABLE, 0x36,
                                [(p, R_AARCH64_RELATIVE, None, None, target)])
        with self.assertRaises(ValueError):
            handler_address(image, 0x180, TABLE, 0x36, [relocs[0], relocs[0]])
        with self.assertRaises(ValueError):
            handler_address(image, 0x180, TABLE, 0x36, [(p, 9999, None, None, 0x40)])
        with self.assertRaises(ValueError):
            handler_address(image, 0x180, TABLE, 0x67, [])

    def test_disassembly_range_and_report(self):
        image, _, relocs = fixture()
        self.assertEqual(calls_from_function(image, 0x40, 0x48),
                         [{"at": "0x40", "target": "0x70"}])
        res = inspect(image, 0x180, relocs, [0x40, 0x48, 0x80, 0x90, 0xc0], table=TABLE)
        self.assertEqual([e["opcode"] for e in res["entries"]],
                         [f"0x{x:02x}" for x in OPCODES])
        self.assertEqual(res["entries"][0]["direct_calls"][0]["target"], "0x70")
        self.assertEqual(res["entries"][0]["function_boundary"], "exact_eh_frame")
        self.assertEqual(res["status"], "candidate_addresses_only")

    def test_missing_handler_not_construed_as_success(self):
        image, _, relocs = fixture()
        with self.assertRaisesRegex(ValueError, "has no handler"):
            inspect(image, 0x180, relocs[1:], [0x40], table=TABLE)


if __name__ == "__main__":
    unittest.main()
