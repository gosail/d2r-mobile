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
    sk_header = [
        "skill", "*Id", "petmax", "Param2", "Param3", *COLD_FIELDS,
        "srvstfunc", "srvdofunc", "cltstfunc", "cltdofunc",
        "calc1", "calc2", "calc5", "calc6", "calc7",
        "aurafilter", "range", "anim", "seqtrans",
        "ResultFlags", "HitShift", "SrcDam", "sentinel",
    ]
    cold = ["2", "1", "3", "4", "5", "8", "6", "1", "3", "4", "5", "8"]
    def skill(name, ident, **fields):
        row = {key: "" for key in sk_header}
        row.update({"skill": name, "*Id": ident, "sentinel": "unchanged"})
        row.update(fields)
        return [row[key] for key in sk_header]
    sk_rows = [
        sk_header,
        skill("Raven", "221", petmax='"min(lvl,par2)"', Param2="5", sentinel="raven"),
        skill("Summon Spirit Wolf", "227", petmax='"min(lvl,par3)"',
              Param3="5", **dict(zip(COLD_FIELDS, cold))),
        skill("Summon Fenris", "237", petmax='"min(lvl,par3)"', Param3="3"),
        skill("Summon Grizzly", "247", petmax="1"),
        skill("BearSmite", "326", srvstfunc="32", srvdofunc="2", calc1="ln34",
              calc2='"max(250,ln12)"', range="h2h", anim="S1",
              seqtrans="S1", ResultFlags="8", HitShift="8", SrcDam="128"),
        skill("Goatman Cleave", "419", srvstfunc="68", srvdofunc="164",
              calc5="11", calc6="30", aurafilter="41347",
              cltstfunc="59", cltdofunc="103"),
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
        bear = get_row(result[SKILLS_PATH], "BearSmite")
        self.assertEqual(bear["srvstfunc"], "68")
        self.assertEqual(bear["srvdofunc"], "164")
        self.assertEqual(bear["calc5"], "10")
        self.assertEqual(bear["calc6"], "30")
        self.assertEqual(bear["calc2"], "0")
        self.assertEqual(bear["anim"], "S1")  # Do not steal the goatman's animation
        self.assertEqual(bear["seqtrans"], "S1")
        self.assertEqual(bear["SrcDam"], "128")
        donor = get_row(result[SKILLS_PATH], "Goatman Cleave")
        self.assertEqual(donor["calc5"], "11")  # Donor untouched
        self.assertEqual(donor["calc6"], "30")
        self.assertEqual(report["bear_cleave"]["arc_degrees"], 120)
        self.assertEqual(report["bear_cleave"]["arc_slices"] * 360 //
                         report["bear_cleave"]["circle_slices"], 120)
        self.assertIn("pending_in_game", report["bear_cleave"]["verified"])
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

    def test_refuse_unrecognized_cleave_source(self):
        s, m = fixture()
        # If the owner has a different build where cleave handlers changed,
        # refuse the overlay rather than silently change a random skill.
        bad = s.replace(b"Goatman Cleave\t419", b"Goatman Cleave\t999")
        with self.assertRaises(IncompatibleGameData):
            patch_tables(bad, m)
        bad = s.replace(b"\t68\t164\t59\t103\t", b"\t68\t1\t59\t103\t")
        with self.assertRaises(IncompatibleGameData):
            patch_tables(bad, m)

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
