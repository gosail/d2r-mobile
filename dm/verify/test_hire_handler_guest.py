#!/usr/bin/env python3
"""Asset-free Mach-O reconstruction guards: no original or copyrighted code."""
import struct
import unittest
from hire_handler_guest import (extract_macho_sections, reconstruct,
                                read_properties, MH_MAGIC_64, CPU_ARM64)


def _id(s):
    return s.encode("ascii").ljust(16, b"\0")


def _segment(segname, secname, pos, length):
    return (struct.pack("<II16sQQQQiiII", 0x19, 152, _id(segname),
                        pos, length, pos, length, 5, 5, 1, 0) +
            struct.pack("<16s16sQQIIIIIIII", _id(secname), _id(segname),
                        pos, length, pos, 14, 0, 0, 0, 0, 0, 0))


def mock_macho():
    text = bytearray(0x4000)
    struct.pack_into("<I", text, 4, 0x100)
    text[0x100:0x104] = b"MOD0"
    writable = bytes(0x200)
    f0 = 0x200
    f1 = f0 + len(text)
    commands = _segment("__TEXT", "__dmmainx0", f0, len(text))
    commands += _segment("__DMmainW", "__img", f1, len(writable))
    header = struct.pack("<IiiIIIII", MH_MAGIC_64, CPU_ARM64,
                         0, 6, 2, len(commands), 0, 0)
    data = bytearray((header + commands).ljust(f0, b"\0"))
    data.extend(text)
    data.extend(writable)
    return bytes(data), bytes(text) + writable


FIXUPS = "# TARGET 0x0\n# TEXT_END 0x3100\n# PATCH_OFF 0x4200\n"
CUTS = "# BUILD_ID db0507f36b84200ac05f75d76ed5026f\n"


class GuestHandlerTests(unittest.TestCase):
    def test_reassemble_with_verified_metadata(self):
        data, expected = mock_macho()
        image, te = reconstruct(data, FIXUPS, CUTS)
        self.assertEqual(image, expected)
        self.assertEqual(te, 0x3100)
        self.assertEqual(image[0x100:0x104], b"MOD0")

    def test_read_properties(self):
        self.assertEqual(read_properties(FIXUPS)["TEXT_END"], "0x3100")

    def test_reject_bad_macho_and_build_id(self):
        data, _ = mock_macho()
        for wrong in (data[:31], data[:4].replace(b"\xcf", b"\x00") + data[4:]):
            with self.assertRaises(ValueError):
                extract_macho_sections(wrong)
        with self.assertRaises(ValueError):
            reconstruct(data, FIXUPS, CUTS.replace("db0507", "ff0507"))
        with self.assertRaises(ValueError):
            reconstruct(data, FIXUPS.replace("0x4200", "0x4204"), CUTS)
        with self.assertRaises(ValueError):
            reconstruct(data, FIXUPS.replace("0x0", "0x1", 1), CUTS)

    def test_reject_section_outside_guest(self):
        data, _ = mock_macho()
        b = bytearray(data)
        # First section at header(32) + segment(72), size field +40.
        struct.pack_into("<Q", b, 32 + 72 + 40, 1 << 32)
        with self.assertRaises(ValueError):
            extract_macho_sections(bytes(b))


if __name__ == "__main__":
    unittest.main()
