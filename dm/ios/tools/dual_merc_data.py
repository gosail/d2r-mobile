#!/usr/bin/env python3
"""Offline experimental dual-hireling pet-type cap, never enabled by default.

D2R's hireable pet type has basemax=1. Raising the pet slot capacity to 2
is a *hypothesis*, not proof that the NPC hire logic supports two living
hirelings. Require an exact shipped 3.3 binary match before any overlay.
Do not use on a valuable save before runtime testing establishes behavior.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess

PETTYPE_TXT = "data/global/excel/pettype.txt"
PETTYPE_BIN = "data/global/excel/pettype.bin"
COMPILER_SHA = "fe6078123d6f011f46af9ea0802739ba672b0547"


class UnsupportedPetTable(ValueError):
    pass


def patch_pettype(original):
    if b"\0" in original:
        raise UnsupportedPetTable("Expected pettype.txt, not compiled binary")
    bom = original.startswith(b"\xef\xbb\xbf")
    try:
        text = original.decode("utf-8-sig")
    except UnicodeError as e:
        raise UnsupportedPetTable("pettype source is not UTF-8") from e
    if "\r" in text.replace("\r\n", ""):
        raise UnsupportedPetTable("Unsupported line endings in pettype")
    eol = "\r\n" if "\r\n" in text else "\n"
    if eol == "\r\n" and "\n" in text.replace("\r\n", ""):
        raise UnsupportedPetTable("Mixed line endings")
    trail = text.endswith(("\n", "\r\n"))
    rows = [ln.split("\t") for ln in text.splitlines()]
    if len(rows) < 9:
        raise UnsupportedPetTable("Unexpectedly short pettype table")
    hdr = rows[0]
    if len(set(hdr)) != len(hdr) or any(len(row) != len(hdr) for row in rows):
        raise UnsupportedPetTable("Invalid pettype columns/row width")
    expected = ("pet type", "basemax", "warp", "partysend")
    if any(k not in hdr for k in expected):
        raise UnsupportedPetTable("Missing pettype columns")
    names = [row[hdr.index("pet type")] for row in rows[1:]]
    if names.count("hireable") != 1 or names.count("single") != 1:
        raise UnsupportedPetTable("Unexpected hireable or single row count")
    hireable = rows[names.index("hireable") + 1]
    single = rows[names.index("single") + 1]
    for col in ("warp", "partysend"):
        if hireable[hdr.index(col)] != "1":
            raise UnsupportedPetTable("Unrecognized hireable." + col)
    if hireable[hdr.index("basemax")] != "1" or single[hdr.index("basemax")] != "1":
        raise UnsupportedPetTable("Expected unmodified Switch 3.3 pet caps")
    hireable[hdr.index("basemax")] = "2"
    encoded = eol.join("\t".join(row) for row in rows) + (eol if trail else "")
    if bom:
        encoded = "\ufeff" + encoded
    patched = encoded.encode("utf-8")
    return patched, {
        "mode": "offline_dual_hireling_experiment_only",
        "pettype": "hireable",
        "column": "basemax",
        "original": 1,
        "experiment": 2,
        "single_pet_type_unchanged": True,
        "native_hire_replace_logic": "unmodified_and_unverified",
        "two_mercenaries_can_coexist": "unverified",
        "save_format": "unchanged_original_single_hireling",
        "known_risks": [
            "Hiring a new mercenary may still replace the existing one",
            "Original save format may persist only the original hireling",
            "Second mercenary could lose its inventory on save/reload",
        ],
    }


def romfs_sources(romfs):
    manifest = Path(romfs, "data", "build_manifest.psv").read_text(encoding="utf-8")
    paths = set()
    for line in manifest.splitlines():
        if line.startswith("#"):
            continue
        fields = line.split("|")
        if len(fields) < 11:
            continue
        path = fields[9].lower().split(":", 1)[-1].replace("\\", "/")
        if not path.startswith("data/") or ".." in path.split("/"):
            continue
        if path.startswith("data/global/excel/") and path.endswith(".txt"):
            paths.add(path)
        if (path.startswith("data/local/lng/strings/") and path.endswith(".json")
                and path.count("/") == 4):
            paths.add(path)
    txt = sum(path.endswith(".txt") for path in paths)
    strings = sum("/lng/strings/" in path for path in paths)
    if PETTYPE_TXT not in paths or txt < 80 or strings < 15:
        raise UnsupportedPetTable("Incomplete original game Excel or localization sources")
    return sorted(paths)


def build(romfs, outdir):
    """Return overlays for pettype.txt/bin using the owner's original resources."""
    import romfs_art
    original = romfs_art.read(romfs, PETTYPE_TXT)
    patched, report = patch_pettype(original)
    tool = os.environ.get("DM_D2R_BINTOOL", "")
    if not tool or not Path(tool).is_file() or not os.access(tool, os.X_OK):
        raise UnsupportedPetTable("DM_D2R_BINTOOL must be the pinned 3.3 datatable compiler")
    root = Path(outdir) / "dual_merc_excel"
    excel = root / "data" / "global" / "excel"
    baseline = root / "base_bin"
    changed = root / "changed_bin"
    for logical in romfs_sources(romfs):
        dest = root / logical
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(romfs_art.read(romfs, logical))
    if not (root / "data/local/lng/strings").is_dir():
        raise UnsupportedPetTable("Missing game localization strings")
    def compile_all(target):
        process = subprocess.run([tool, "compile", str(excel), str(target)],
                                 capture_output=True, text=True, errors="replace",
                                 timeout=180)
        if process.returncode or "failed=0" not in process.stdout:
            raise UnsupportedPetTable(
                "Native 3.3 datatable compile failed: "
                + (process.stderr + process.stdout)[-1800:])

    compile_all(baseline)
    shipped = romfs_art.read(romfs, PETTYPE_BIN)
    baseline_bytes = (baseline / "pettype.bin").read_bytes()
    if shipped != baseline_bytes:
        raise UnsupportedPetTable(
            "The compiled pettype.bin does not byte-match the owner Switch 3.3 "
            "ROMFS. Refusing an unverified hireling-cap experiment.")
    (root / PETTYPE_TXT).write_bytes(patched)
    compile_all(changed)
    outbin = (changed / "pettype.bin").read_bytes()
    if outbin == shipped:
        raise UnsupportedPetTable("pettype.bin was unchanged after cap modification")
    report["binary"] = {
        "compiler_commit": COMPILER_SHA,
        "shipped_baseline_sha256": hashlib.sha256(shipped).hexdigest(),
        "patched_sha256": hashlib.sha256(outbin).hexdigest(),
        "baseline_byte_identical": True,
        "verified_effect": "game_data_only_not_npc_hire_or_save_behavior",
    }
    out_txt = Path(outdir) / "offline_dualmerc_pettype.txt"
    out_txt.write_bytes(patched)
    (Path(outdir) / "offline_dualmerc_report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {PETTYPE_TXT: str(out_txt), PETTYPE_BIN: str(changed / "pettype.bin")}
