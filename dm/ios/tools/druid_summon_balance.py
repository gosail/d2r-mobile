#!/usr/bin/env python3
"""Offline-only Druid summon table patch for the owner's D2R Switch 3.3 resources.

Source material is read from the user's ROMFS at build time; no game data is
checked into this repository. This module intentionally does not modify AI
functions, collision handling, or the native combat hit-list.
"""
import argparse
import json
from pathlib import Path

SKILLS_PATH = "data/global/excel/skills.txt"
MONSTATS_PATH = "data/global/excel/monstats.txt"

# A controlled baseline is essential: a game update must not silently patch a
# different skill, reuse a moved parameter, or corrupt a newer table.
SKILL_IDS = {
    "Raven": "221",
    "Summon Spirit Wolf": "227",
    "Summon Fenris": "237",
    "Summon Grizzly": "247",
}
PET_AI = {
    "druidhawk": "Raven",
    "spiritwolf": "DruidWolf",
    "fenris": "DruidWolf",
    "druidbear": "DruidBear",
}
COLD_FIELDS = ("EMin", "EMinLev1", "EMinLev2", "EMinLev3",
               "EMinLev4", "EMinLev5", "EMax", "EMaxLev1",
               "EMaxLev2", "EMaxLev3", "EMaxLev4", "EMaxLev5")
AI_DELAY_FIELDS = ("aidel", "aidel(N)", "aidel(H)")
TABLE_PATHS = (SKILLS_PATH, MONSTATS_PATH)


class IncompatibleGameData(ValueError):
    """Refuse to patch an unverified source table."""


class Tsv:
    def __init__(self, data, path):
        self.path = path
        self.bom = data.startswith(b"\xef\xbb\xbf")
        try:
            text = data.decode("utf-8-sig")
        except UnicodeError as exc:
            raise IncompatibleGameData(f"{path}: expected UTF-8 text") from exc
        if "\r" in text.replace("\r\n", ""):
            raise IncompatibleGameData(f"{path}: unsupported line endings")
        self.eol = "\r\n" if "\r\n" in text else "\n"
        if self.eol == "\r\n" and "\n" in text.replace("\r\n", ""):
            raise IncompatibleGameData(f"{path}: mixed line endings")
        self.trailing_eol = text.endswith(("\n", "\r\n"))
        lines = text.splitlines()
        if len(lines) < 2:
            raise IncompatibleGameData(f"{path}: missing rows")
        self.rows = [line.split("\t") for line in lines]
        self.header = self.rows[0]
        if len(set(self.header)) != len(self.header):
            raise IncompatibleGameData(f"{path}: duplicate columns")
        if any(len(row) != len(self.header) for row in self.rows):
            raise IncompatibleGameData(f"{path}: irregular row width")
        self.index = {name: i for i, name in enumerate(self.header)}

    def require(self, *names):
        missing = [name for name in names if name not in self.index]
        if missing:
            raise IncompatibleGameData(f"{self.path}: missing {missing}")

    def find(self, key_column, key):
        self.require(key_column)
        pos = self.index[key_column]
        rows = [row for row in self.rows[1:] if row[pos] == key]
        if len(rows) != 1:
            raise IncompatibleGameData(
                f"{self.path}: expected one {key_column}={key!r}, found {len(rows)}")
        return rows[0]

    def expect(self, row, col, value):
        self.require(col)
        current = row[self.index[col]]
        if current != str(value):
            raise IncompatibleGameData(
                f"{self.path}: {row[0]}.{col}: expected {value!r}, found {current!r}")

    def set(self, row, col, value, changes):
        old = row[self.index[col]]
        new = str(value)
        # Small damage brackets can round to the same integer (e.g. 1 * 1.30).
        # Baseline skill caps and monster AI delays are validated separately.
        if old == new:
            return
        row[self.index[col]] = new
        changes.append({"table": self.path, "row": row[0], "field": col,
                        "from": old, "to": new})

    def encode(self):
        result = self.eol.join("\t".join(row) for row in self.rows)
        if self.trailing_eol:
            result += self.eol
        if self.bom:
            result = "\ufeff" + result
        return result.encode("utf-8")


def _pct_130(value):
    # Preserve integer table data. Positive values are rounded to nearest
    # integer, ties up: new = (old * 130 + 50) // 100.
    if not value.isascii() or not value.isdecimal():
        raise IncompatibleGameData(f"Expected positive integer cold damage, got {value!r}")
    n = int(value)
    if not 0 < n < 100000:
        raise IncompatibleGameData(f"Unexpected cold damage magnitude {n}")
    return (n * 130 + 50) // 100


def patch_tables(skills_bytes, monstats_bytes):
    """Return patched bytes and exact field-level audit report, without mutation."""
    skills = Tsv(skills_bytes, SKILLS_PATH)
    monsters = Tsv(monstats_bytes, MONSTATS_PATH)
    skills.require("skill", "*Id", "petmax", "Param2", "Param3", *COLD_FIELDS)
    monsters.require("Id", "AI", *AI_DELAY_FIELDS)
    changes = []

    # Verify all expected rows and references before changing anything.
    sr = {name: skills.find("skill", name) for name in SKILL_IDS}
    mr = {name: monsters.find("Id", name) for name in PET_AI}
    for name, expected_id in SKILL_IDS.items():
        skills.expect(sr[name], "*Id", expected_id)
    for name, expected_ai in PET_AI.items():
        monsters.expect(mr[name], "AI", expected_ai)
        for field in AI_DELAY_FIELDS:
            monsters.expect(mr[name], field, "15")
    skills.expect(sr["Raven"], "Param2", "5")
    skills.expect(sr["Summon Fenris"], "Param3", "3")
    for name, column in (("Raven", "petmax"),
                         ("Summon Fenris", "petmax")):
        expected = "min(lvl,par2)" if name == "Raven" else "min(lvl,par3)"
        got = sr[name][skills.index[column]].strip('"').replace(" ", "")
        if got != expected:
            raise IncompatibleGameData(f"{name}.petmax: unexpected formula {got!r}")
    if sr["Summon Spirit Wolf"][skills.index["petmax"]].strip('"').replace(" ", "") != "min(lvl,par3)":
        raise IncompatibleGameData("Summon Spirit Wolf.petmax changed")
    for field in COLD_FIELDS:
        _pct_130(sr["Summon Spirit Wolf"][skills.index[field]])

    # 5 -> 8 ravens and 3 -> 5 dire wolves, keeping the original scaling
    # formula (so no extra ravens/wolves at skill levels below the cap).
    skills.set(sr["Raven"], "Param2", "8", changes)
    skills.set(sr["Summon Fenris"], "Param3", "5", changes)

    # +30% Spirit Wolf cold damage at all six damage brackets. No changes
    # to Grizzly synergy, elemental type, resistances, or other classes.
    for field in COLD_FIELDS:
        current = sr["Summon Spirit Wolf"][skills.index[field]]
        skills.set(sr["Summon Spirit Wolf"], field, _pct_130(current), changes)

    # Shorten the idle AI decision delay of the existing summons. This is
    # *not* an engine-level attack-animation or pathfinding change.
    for name, row in mr.items():
        for field in AI_DELAY_FIELDS:
            monsters.set(row, field, "10", changes)

    report = {
        "mode": "offline-build-opt-in",
        "raven_cap": 8,
        "dire_wolf_cap": 5,
        "spirit_wolf_cold_percent": 130,
        "pet_ai_delay_before": 15,
        "pet_ai_delay_after": 10,
        "bear_cleave": "not_implemented_requires_verified_native_combat_hook",
        "pet_pathfinding": "not_implemented_requires_verified_native_ai_hook",
        "raven_attack_animation": "unchanged_ai_delay_only",
        "field_changes": changes,
    }
    return {SKILLS_PATH: skills.encode(), MONSTATS_PATH: monsters.encode()}, report


def build(romfs, outdir):
    """Add to gen_romfs_overlay.REPL only when explicitly opted in at build time."""
    import romfs_art
    originals = {path: romfs_art.read(romfs, path) for path in TABLE_PATHS}
    patched, report = patch_tables(originals[SKILLS_PATH], originals[MONSTATS_PATH])
    generated = {}
    for path, data in patched.items():
        target = Path(outdir) / ("offline_druid_" + Path(path).name)
        target.write_bytes(data)
        generated[path] = str(target)
    (Path(outdir) / "offline_druid_patch_report.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return generated


def main():
    p = argparse.ArgumentParser(description="Patch owner-extracted D2R 3.3 summon tables for offline testing")
    p.add_argument("--skills", type=Path, required=True)
    p.add_argument("--monstats", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    source = {SKILLS_PATH: args.skills.read_bytes(), MONSTATS_PATH: args.monstats.read_bytes()}
    patched, report = patch_tables(source[SKILLS_PATH], source[MONSTATS_PATH])
    args.out.mkdir(parents=True, exist_ok=True)
    for path, data in patched.items():
        (args.out / Path(path).name).write_bytes(data)
    (args.out / "patch-report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Patched {len(report['field_changes'])} cells; see {args.out / 'patch-report.json'}")


if __name__ == "__main__":
    main()
