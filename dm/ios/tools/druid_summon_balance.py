#!/usr/bin/env python3
"""Offline-only Druid summon table patch for the owner's D2R Switch 3.3 resources.

Source material is read from the user's ROMFS at build time; no game data is
checked into this repository. This module reuses the game's existing native Cleave skill handlers for a
120-degree Bear Smite, rather than introducing foreign damage routines.
It does not change pathfinding, entity collision, or the combat hit-list.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

SKILLS_PATH = "data/global/excel/skills.txt"
MONSTATS_PATH = "data/global/excel/monstats.txt"

# A controlled baseline is essential: a game update must not silently patch a
# different skill, reuse a moved parameter, or corrupt a newer table.
SKILL_IDS = {
    "Raven": "221",
    "Summon Spirit Wolf": "227",
    "Summon Fenris": "237",
    "Summon Grizzly": "247",
    "BearSmite": "326",
    "Goatman Cleave": "419",
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
    skills.require("skill", "*Id", "petmax", "Param2", "Param3",
                   "srvstfunc", "srvdofunc", "cltstfunc", "cltdofunc",
                   "calc1", "calc2", "calc5", "calc6", "calc7",
                   "aurafilter", "range", "anim", "seqtrans",
                   "ResultFlags", "HitShift", "SrcDam", *COLD_FIELDS)
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

    # A 3.3 Reign-of-the-Warlock build already has server-side Cleave:
    # Goatman Cleave uses 164 with (calc5, calc6) = (11, 30) slices.
    # We reuse exactly that original engine handler on BearSmite, retaining
    # the bear's own skill ID (326), physical damage, pet animation and skill.
    # 10 of 30 slices of the full 360-degree circle = 120 degrees.
    bear, donor = sr["BearSmite"], sr["Goatman Cleave"]
    for column, value in {
        "srvstfunc": "32", "srvdofunc": "2", "calc1": "ln34",
        "calc2": '"max(250,ln12)"', "calc5": "", "calc6": "",
        "aurafilter": "", "range": "h2h", "anim": "S1",
        "seqtrans": "S1", "ResultFlags": "8", "HitShift": "8",
        "SrcDam": "128",
    }.items():
        skills.expect(bear, column, value)
    for column, value in {
        "srvstfunc": "68", "srvdofunc": "164", "calc5": "11",
        "calc6": "30", "aurafilter": "41347",
        "cltstfunc": "59", "cltdofunc": "103",
    }.items():
        skills.expect(donor, column, value)

    # 5 -> 8 ravens and 3 -> 5 dire wolves, keeping the original scaling
    # formula (so no extra ravens/wolves at skill levels below the cap).
    skills.set(sr["Raven"], "Param2", "8", changes)
    skills.set(sr["Summon Fenris"], "Param3", "5", changes)

    # +30% Spirit Wolf cold damage at all six damage brackets. No changes
    # to Grizzly synergy, elemental type, resistances, or other classes.
    for field in COLD_FIELDS:
        current = sr["Summon Spirit Wolf"][skills.index[field]]
        skills.set(sr["Summon Spirit Wolf"], field, _pct_130(current), changes)

    # Bear Smite remains the bear's native assigned skill / sequence;
    # the server start/do handlers are borrowed from the game's own
    # monster-compatible Goatman Cleave. Keep its original hit flags and
    # summon damage progression. Cleave uses calc2 for flat damage, not
    # Bear Smite's stun length, so explicitly zero it to avoid accidental
    # +250 flat damage. Preserve bear's unchanged S1 animation, which
    # still requires an in-game visual and multi-hit verification.
    for column, value in (
        ("srvstfunc", "68"), ("srvdofunc", "164"),
        ("cltstfunc", "59"), ("cltdofunc", "103"),
        ("aurafilter", "41347"), ("calc2", "0"),
        ("calc5", "10"), ("calc6", "30"), ("calc7", "0"),
    ):
        skills.set(bear, column, value, changes)

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
        "bear_cleave": {
            "implementation": "native_monster_cleave_skill_handlers",
            "source": "Goatman Cleave (419)",
            "skill": "BearSmite (326)",
            "srvstfunc": 68,
            "srvdofunc": 164,
            "arc_degrees": 120,
            "arc_slices": 10,
            "circle_slices": 30,
            "verified": "table_and_formula_only_pending_in_game_multi_target_test",
            "limitations": ["secondary_damage_is_native_cleave_default",
                            "bear_smite_stun_formula_replaced_by_cleave_flat_damage_zero",
                            "pet_AI_pathfinding_unchanged"],
        },
        "pet_pathfinding": "not_implemented_requires_verified_native_ai_hook",
        "raven_attack_animation": "unchanged_ai_delay_only",
        "field_changes": changes,
    }
    return {SKILLS_PATH: skills.encode(), MONSTATS_PATH: monsters.encode()}, report



BINTOOL_SHA = "fe6078123d6f011f46af9ea0802739ba672b0547"
BIN_NAMES = ("skills.bin", "monstats.bin", "skillscode.bin")


def _manifest_paths(romfs):
    # Only copy source tables from the owner's sparse original game resources.
    # A full excel set and localization keys are necessary for formula/index
    # compilation; compiling two changed TXT files alone is incorrect.
    manifest = Path(romfs, "data", "build_manifest.psv").read_text(encoding="utf-8")
    paths = set()
    for line in manifest.splitlines():
        if line.startswith("#"):
            continue
        fields = line.split("|")
        if len(fields) < 11:
            continue
        name = fields[9].lower().split(":", 1)[-1].replace("\\", "/")
        if not name.startswith("data/") or ".." in name.split("/"):
            continue
        excel = name.startswith("data/global/excel/") and name.endswith(".txt")
        strings = (name.startswith("data/local/lng/strings/") and
                   name.endswith(".json") and
                   name.count("/") == 4)
        if excel or strings:
            paths.add(name)
    if (SKILLS_PATH not in paths or MONSTATS_PATH not in paths or
            len([x for x in paths if x.endswith(".txt")]) < 80 or
            len([x for x in paths if "/lng/strings/" in x]) < 15):
        raise IncompatibleGameData("The purchased 3.3 RomFS does not have a complete Excel source corpus")
    return sorted(paths)


def compile_binary_overlays(romfs, outdir, patched, report):
    """Build and byte-verify native game tables, then return BIN overlays.

    D2R normally loads compiled .bin skill tables, so merely changing .txt
    cannot prove that an enhanced summon actually uses the new values.
    The 3.3 compiler is an external build-time tool; no executable or
    original resource file is committed to this repository.
    """
    import romfs_art
    tool = os.environ.get("DM_D2R_BINTOOL", "")
    if not tool or not Path(tool).is_file() or not os.access(tool, os.X_OK):
        raise IncompatibleGameData("DM_D2R_BINTOOL (pinned 3.3 datatable compiler) is required")
    root = Path(outdir) / "offline_druid_bintables"
    excel = root / "data" / "global" / "excel"
    baseline_dir = root / "baseline-bin"
    compiled_dir = root / "patched-bin"
    for logical in _manifest_paths(romfs):
        dst = root / logical
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(romfs_art.read(romfs, logical))
    if not (root / "data/local/lng/strings").is_dir():
        raise IncompatibleGameData("Missing localization keys needed to compile native skill tables")

    def compile_to(destination):
        # The third-party 3.3 compiler prints platform-localized, sometimes
        # non-UTF8 diagnostics; decode lossily *only for its log text*. Source
        # game tables and binary SHA checks always remain byte-exact.
        run = subprocess.run([tool, "compile", str(excel), str(destination)],
                             capture_output=True, text=True, errors="replace",
                             timeout=180)
        if run.returncode:
            raise IncompatibleGameData(
                "3.3 datatable compilation failed: " + (run.stderr or run.stdout)[-1300:])
        if "failed=0" not in run.stdout:
            raise IncompatibleGameData("3.3 datatable compiler did not confirm success")
        return run.stdout

    # First prove that the compiler generates exact shipped 3.3 bytes. This
    # detects a silently incompatible PC table layout or a mismatched Switch
    # build, before any gameplay data is replaced.
    compile_to(baseline_dir)
    baseline_sha = {}
    for name in BIN_NAMES:
        logical = "data/global/excel/" + name
        original_bin = romfs_art.read(romfs, logical)
        generated = (baseline_dir / name).read_bytes()
        baseline_sha[name] = hashlib.sha256(original_bin).hexdigest()
        if generated != original_bin:
            raise IncompatibleGameData(
                f"{name}: 3.3 compiler output differs from shipped Switch bytes; "
                "refusing an unverified binary table patch")

    # Change only the two Druid table files and recompile the whole source set
    # so formula offsets in skillscode.bin stay consistent with skills.bin.
    for logical, data in patched.items():
        (root / logical).write_bytes(data)
    compile_to(compiled_dir)
    changes = {}
    for name in BIN_NAMES:
        output = compiled_dir / name
        original = baseline_dir / name
        if not output.is_file() or not original.is_file():
            raise IncompatibleGameData("Missing generated binary datatable " + name)
        if name != "skillscode.bin" and output.read_bytes() == original.read_bytes():
            raise IncompatibleGameData(name + ": compiled bytes unchanged despite source patch")
        changes["data/global/excel/" + name] = str(output)
    report["native_bin"] = {
        "compiler_sha": BINTOOL_SHA,
        "baseline_byte_exact": True,
        "files": list(BIN_NAMES),
        "baseline_sha256": baseline_sha,
        "warning": "Data and native arc verified; actual multi-target bear combat still requires device test",
    }
    return changes


def build(romfs, outdir):
    """Add to gen_romfs_overlay.REPL only when explicitly opted in at build time."""
    import romfs_art
    originals = {path: romfs_art.read(romfs, path) for path in TABLE_PATHS}
    patched, report = patch_tables(originals[SKILLS_PATH], originals[MONSTATS_PATH])
    # Compile matching BIN files and verify every unmodified native baseline
    # byte-for-byte before emitting any overlay. Fail closed on mismatch.
    generated = compile_binary_overlays(romfs, outdir, patched, report)
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
