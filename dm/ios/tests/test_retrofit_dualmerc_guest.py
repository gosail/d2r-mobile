#!/usr/bin/env python3
"""Pure synthetic Mach-O tests. No original copyrighted game code is needed."""
import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from retrofit_dualmerc_guest import (retrofit_bytes, macho_sections, gate_stub,
                                     b_branch, STUB_BYTES)
from dualmerc_native_cuts import branch_word, svc_word, BUILD_ID


def _id(s):
    return s.encode().ljust(16, b"\0")


def segment(name, sections):
    n = 72 + len(sections) * 80
    blob = struct.pack("<II16sQQQQiiII", 0x19, n, _id(name), 0, 0,
                       0, 0, 5, 5, len(sections), 0)
    for sec, offset, size in sections:
        blob += struct.pack("<16s16sQQIIIIIIII", _id(sec), _id(name),
                            offset, size, offset, 14, 0, 0, 0, 0, 0, 0)
    return blob


FIXUPS = ("# TARGET 0x0\n# TEXT_END 0x4000\n# PATCH_OFF 0x9000\n"
          "# KEY_PAGE_OFF 0x5000\n# KEY_KGATE_IDX 3\n")
CUTS = "# BUILD_ID " + BUILD_ID + "\n"
GATES = "0x9004 svc 129 0x10\n"
SITES = tuple((site, struct.pack("<I", branch_word(site, dest)).hex(),
               struct.pack("<I", svc_word(svc)).hex(), "synthetic " + str(i), "仅宿主")
              for i, (site, dest, svc) in enumerate(
                  ((0x100, 0x200, 0x91), (0x300, 0x200, 0x92),
                   (0x500, 0x400, 0x93))))


def fixture():
    x0_size = 0x4000
    x1_size = 0x4000
    writable_size = 0x5000
    off0 = 0x400
    off1 = off0 + x0_size
    offw = off1 + x1_size
    c = segment("__TEXT", (("__dmmainx0", off0, x0_size),
                            ("__dmmainx1", off1, x1_size)))
    c += segment("__DMmainW", (("__img", offw, writable_size),))
    h = struct.pack("<IiiIIIII", 0xfeedfacf, 0x100000c,
                    0, 6, 2, len(c), 0, 0)
    raw = bytearray((h + c).ljust(off0, b"\0"))
    raw.extend(bytes(x0_size + x1_size + writable_size))
    for site, old, *_ in SITES:
        raw[off0 + site:off0 + site + 4] = bytes.fromhex(old)
    # Three existing host guest bytes and zero-filled executable padding.
    raw[off1:off1 + 32] = bytes.fromhex("1f2003d5") * 8
    return bytes(raw), (off0, off1, x0_size, x1_size)


class RetrofitGuestTests(unittest.TestCase):
    def test_pass_through_gate_placement_and_bytes(self):
        raw, (off0, off1, _, x1_size) = fixture()
        changed, cut, gate, report = retrofit_bytes(
            raw, FIXUPS, CUTS, GATES, sites=SITES)
        self.assertEqual(len(changed), len(raw))
        self.assertEqual(report["new_gate_count"], 3)
        self.assertFalse(report["second_mercenary_enabled"])
        self.assertEqual(macho_sections(changed), macho_sections(raw))
        self.assertEqual(changed[off1:off1 + 32], raw[off1:off1 + 32])
        for i, (site, old, svc, _, _) in enumerate(SITES):
            start = 0x9000 + 32 + i * STUB_BYTES
            self.assertEqual(changed[off0 + site:off0 + site + 4],
                             struct.pack("<I", b_branch(site, start)))
            expected = gate_stub(start, site + 4, 0x5000, 3)
            self.assertEqual(changed[off1 + 32 + i * STUB_BYTES:
                                     off1 + 32 + (i + 1) * STUB_BYTES], expected)
            self.assertIn(f"{start+32:#x}\tsvc\t{0x91+i}", gate)
            self.assertIn(old, cut)
        # Other original game code and signed sections unchanged.
        self.assertEqual(changed[off0 + 0x700:off0 + 0x800],
                         raw[off0 + 0x700:off0 + 0x800])

    def test_reapply_and_mismatched_old_bytes_refuse(self):
        raw, _ = fixture()
        updated = retrofit_bytes(raw, FIXUPS, CUTS, GATES, SITES)
        with self.assertRaisesRegex(ValueError, "already patched"):
            retrofit_bytes(updated[0], FIXUPS, CUTS, GATES, SITES)
        b = bytearray(raw)
        b[0x400 + 0x100] ^= 0x40
        with self.assertRaisesRegex(ValueError, "Unverified"):
            retrofit_bytes(bytes(b), FIXUPS, CUTS, GATES, SITES)
        with self.assertRaisesRegex(ValueError, "build ID"):
            retrofit_bytes(raw, FIXUPS, "# BUILD_ID invalid\n", GATES, SITES)

    def test_reject_missing_sections_and_layout(self):
        raw, (_, off1, _, x1_size) = fixture()
        with self.assertRaisesRegex(ValueError, "target|layout"):
            retrofit_bytes(raw, FIXUPS.replace("TARGET 0x0", "TARGET 0x1"),
                           CUTS, GATES, SITES)
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            retrofit_bytes(raw, FIXUPS.replace("KEY_KGATE_IDX", "NOT_KGATE"),
                           CUTS, GATES, SITES)
        b = bytearray(raw)
        b[off1:off1 + x1_size] = bytes([1]) * x1_size
        with self.assertRaisesRegex(ValueError, "padding"):
            retrofit_bytes(bytes(b), FIXUPS, CUTS, GATES, SITES)

    def test_gate_word_and_branch_range(self):
        with self.assertRaisesRegex(ValueError, "out of range"):
            b_branch(0, 1 << 27)
        with self.assertRaisesRegex(ValueError, "out of range"):
            gate_stub(0x9000, 1 << 29, 0x5000, 3)
        self.assertEqual(gate_stub(0x9020, 0x104, 0x5000, 3)[:4],
                         bytes.fromhex("fd7bbfa9"))


if __name__ == "__main__":
    unittest.main()
