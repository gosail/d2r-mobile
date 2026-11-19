#!/usr/bin/env python3
"""Asset-free guards for experimental offline dual-hireling pet type cap."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from dual_merc_data import UnsupportedPetTable, patch_pettype

HEADER = ["pet type", "group", "pool", "basemax", "warp", "range", "partysend", "sentinel"]
ROWS = [
    ["none", "", "", "", "", "", "", "none-keep"],
    ["single", "", "", "1", "1", "", "1", "single-keep"],
    ["valkyrie", "", "", "0", "1", "", "1", "valk-keep"],
    ["golem", "", "", "0", "1", "", "1", "golem-keep"],
    ["skeleton", "", "", "0", "1", "", "1", "skel-keep"],
    ["skeletonmage", "", "", "0", "1", "", "1", "mage-keep"],
    ["revive", "", "", "0", "1", "", "1", "revive-keep"],
    ["hireable", "", "", "1", "1", "", "1", "hire-keep"],
    ["raven", "", "", "0", "1", "", "", "raven-keep"],
]


def example(bom=False, crlf=False, final=True):
    eol = "\r\n" if crlf else "\n"
    content = eol.join("\t".join(row) for row in [HEADER, *ROWS])
    content += eol if final else ""
    return (b"\xef\xbb\xbf" if bom else b"") + content.encode("utf-8")


def row_for(data, name):
    lines = data.decode("utf-8-sig").splitlines()
    hdr = lines[0].split("\t")
    row = next(x.split("\t") for x in lines[1:] if x.split("\t")[0] == name)
    return dict(zip(hdr, row))


class TestDualMercPettype(unittest.TestCase):
    def test_only_hireable_limit_changes(self):
        original = example()
        changed, report = patch_pettype(original)
        self.assertEqual(row_for(changed, "hireable")["basemax"], "2")
        self.assertEqual(row_for(changed, "single")["basemax"], "1")
        self.assertEqual(row_for(changed, "skeleton")["basemax"], "0")
        self.assertEqual(row_for(changed, "hireable")["sentinel"], "hire-keep")
        self.assertEqual(report["native_hire_replace_logic"], "unmodified_and_unverified")
        self.assertEqual(report["save_format"], "unchanged_original_single_hireling")
        self.assertEqual(changed.count(b"\n"), original.count(b"\n"))

    def test_bom_crlf_and_no_trailing_newline(self):
        for bom in (False, True):
            for final in (False, True):
                original = example(bom=bom, crlf=True, final=final)
                changed, _ = patch_pettype(original)
                self.assertEqual(changed.startswith(b"\xef\xbb\xbf"), bom)
                self.assertNotIn(b"\n", changed.replace(b"\r\n", b""))
                self.assertEqual(changed.endswith(b"\r\n"), final)

    def test_refuse_reapplication(self):
        changed, _ = patch_pettype(example())
        with self.assertRaises(UnsupportedPetTable):
            patch_pettype(changed)

    def test_refuse_malformed_or_changed_version(self):
        for original in (
            example().replace(b"hireable\t\t\t1", b"hireable\t\t\t3"),
            example().replace(b"single\t\t\t1", b"single\t\t\t2"),
            example().replace(b"hireable\t\t\t1\t1", b"hireable\t\t\t1\t0"),
            example().replace(b"basemax", b"maxp"),
            example() + b"hireable\t\t\t1\t1\t\t1\tduplicate\n",
            example().replace(b"hireable", b"hireablX"),
            example() + b"\x00",
            example().replace(b"golem", b"gole\nm"),
            example().replace(b"revive", b"\xffevive"),
        ):
            with self.subTest(case=original[-55:]):
                with self.assertRaises(UnsupportedPetTable):
                    patch_pettype(original)


if __name__ == "__main__":
    unittest.main()
