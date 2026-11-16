#!/usr/bin/env python3
"""Asset-free regression tests for the offline Druid table patcher."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from druid_summon_balance import (
    AI_DELAY_FIELDS, COLD_FIELDS, IncompatibleGameData,
    MONSTATS_PATH, SKILLS_PATH, patch_tables,
)


def fixture(bom=False):
    sk_header = ["skill", "*Id", "petmax", "Param2", "Param3", *COLD_FIELDS, "sentinel"]
    cold = ["2", "1", "3", "4", "5", "8", "6", "1", "3", "4", "5", "8"]
    def skill(name, ident, petmax="", p2="", p3="", values=None, sentinel="unchanged"):
        return [name, ident, petmax, p2, p3, *(values or [""] * 12), sentinel]
    sk_rows = [
        sk_header,
        skill("Raven", "221", '"min(lvl,par2)"', "5", sentinel="raven"),
        skill("Summon Spirit Wolf", "227", '"min(lvl,par3)"', p3="5", values=cold),
        skill("Summon Fenris", "237", '"min(lvl,par3)"', p3="3"),
        skill("Summon Grizzly", "247", "1"),
        skill("Teleport", "54", sentinel="do-not-touch"),
    ]
    mo_header = ["Id", "AI", *AI_DELAY_FIELDS, "sentinel"]
    mo_rows = [mo_header] + [
        [name, ai, "15", "15", "15", "unchanged"] for name, ai in (
            ("druidhawk", "Raven"), ("spiritwolf", "DruidWolf"),
            ("fenris", "DruidWolf"), ("druidbear", "DruidBear"))
    ] + [["zombie1", "Zombie", "15", "15", "15", "do-not-touch"]]
    def encode(rows):
        data = ("\r\n".join("\t".join(row) for row in rows) + "\r\n").encode()
        return (b"\xef\xbb\xbf" if bom else b"") + data
    return encode(sk_rows), encode(mo_rows)


def get_row(data, key):
    lines = data.decode("utf-8-sig").splitlines()
    header = lines[0].split("\t")
    item = next(line.split("\t") for line in lines[1:] if line.split("\t")[0] == key)
    return dict(zip(header, item))


class TestOfflineDruidTablePatch(unittest.TestCase):
    def test_expected_changes_and_non_druid_rows(self):
        s, m = fixture()
        result, report = patch_tables(s, m)
        self.assertEqual(set(result), {SKILLS_PATH, MONSTATS_PATH})
        self.assertEqual(get_row(result[SKILLS_PATH], "Raven")["Param2"], "8")
        self.assertEqual(get_row(result[SKILLS_PATH], "Summon Fenris")["Param3"], "5")
        wolf = get_row(result[SKILLS_PATH], "Summon Spirit Wolf")
        self.assertEqual(wolf["EMin"], "3")
        self.assertEqual(wolf["EMax"], "8")
        self.assertEqual(wolf["EMinLev1"], "1")  # 1.30 rounds to 1
        self.assertEqual(wolf["EMinLev2"], "4")  # 3.90 rounds to 4
        for name in ("druidhawk", "spiritwolf", "fenris", "druidbear"):
            pet = get_row(result[MONSTATS_PATH], name)
            for field in AI_DELAY_FIELDS:
                self.assertEqual(pet[field], "10")
        self.assertEqual(get_row(result[SKILLS_PATH], "Teleport")["sentinel"], "do-not-touch")
        self.assertEqual(get_row(result[MONSTATS_PATH], "zombie1")["aidel"], "15")
        self.assertEqual(report["bear_cleave"],
                         "not_implemented_requires_verified_native_combat_hook")
        self.assertEqual(report["raven_attack_animation"], "unchanged_ai_delay_only")
        self.assertTrue(result[SKILLS_PATH].endswith(b"\r\n"))
        self.assertNotIn(b"\n", result[SKILLS_PATH].replace(b"\r\n", b""))

    def test_round_trip_with_bom(self):
        s, m = fixture(bom=True)
        output, _ = patch_tables(s, m)
        self.assertTrue(output[SKILLS_PATH].startswith(b"\xef\xbb\xbf"))
        self.assertTrue(output[MONSTATS_PATH].startswith(b"\xef\xbb\xbf"))

    def test_refuse_reapply(self):
        s, m = fixture()
        first, _ = patch_tables(s, m)
        with self.assertRaises(IncompatibleGameData):
            patch_tables(first[SKILLS_PATH], first[MONSTATS_PATH])

    def test_refuse_other_game_versions(self):
        s, m = fixture()
        with self.assertRaises(IncompatibleGameData):
            patch_tables(s.replace(b"Summon Spirit Wolf\t227", b"Summon Spirit Wolf\t999"), m)
        with self.assertRaises(IncompatibleGameData):
            patch_tables(s.replace(b'Raven\t221\t"min(lvl,par2)"\t5',
                                   b'Raven\t221\t"max(lvl,par2)"\t5'), m)

    def test_refuse_wrong_monster_ai(self):
        s, m = fixture()
        with self.assertRaises(IncompatibleGameData):
            patch_tables(s, m.replace(b"druidbear\tDruidBear", b"druidbear\tZombie"))

    def test_refuse_invalid_cold_column(self):
        s, m = fixture()
        with self.assertRaises(IncompatibleGameData):
            patch_tables(s.replace(b"\t2\t1\t3\t4\t5\t8\t6\t1\t3\t4\t5\t8\t",
                                   b"\t-2\t1\t3\t4\t5\t8\t6\t1\t3\t4\t5\t8\t"), m)

    def test_refuse_missing_and_duplicate_skill(self):
        s, m = fixture()
        with self.assertRaises(IncompatibleGameData):
            patch_tables(s.replace(b"EMinLev5", b"EMinLevSix"), m)
        dup = s + next(line for line in s.splitlines(True) if line.startswith(b"Raven\t"))
        with self.assertRaises(IncompatibleGameData):
            patch_tables(dup, m)


if __name__ == "__main__":
    unittest.main()
