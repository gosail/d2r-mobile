#!/usr/bin/env python3
"""Inspect the owner's prepared iOS Guest.framework for C2S handler offsets.

Read-only: reconstructs the original address-space prefix from signed Mach-O
sections; no game code is written or uploaded. Does not claim an entry point
is the native hire/replace transaction without gameplay evidence.
"""
import argparse
import json
from pathlib import Path
import struct
import zipfile

from hire_handler import inspect, SUPPORTED_MAIN_BUILD_ID

BASE = "Payload/DmIOS.app/"
GUEST = BASE + "Frameworks/Guest.framework/Guest"
FIXUPS = BASE + "dm/main.fixups.tsv"
CUTS = BASE + "dm/main.cuts.tsv"
MH_MAGIC_64 = 0xFEEDFACF
CPU_ARM64 = 0x0100000c
LC_SEGMENT_64 = 0x19


def _name(blob):
    return blob.split(b"\0", 1)[0].decode("ascii")


def extract_macho_sections(data):
    """Return file-backed sections {(segment,section): bytes}; reject overlap."""
    if len(data) < 32:
        raise ValueError("truncated Mach-O")
    magic, cpu, subtype, filetype, ncmds, commands_size, flags, reserved = (
        struct.unpack_from("<IiiIIIII", data))
    if magic != MH_MAGIC_64 or cpu != CPU_ARM64 or filetype != 6:
        raise ValueError("expected an arm64 MH_DYLIB Guest.framework")
    limit = 32 + commands_size
    if ncmds > 128 or commands_size > 0x4000 or limit > len(data):
        raise ValueError("invalid Mach-O load command bounds")
    off = 32
    found = {}
    for _ in range(ncmds):
        if off + 8 > limit:
            raise ValueError("truncated load command")
        cmd, length = struct.unpack_from("<II", data, off)
        if length < 8 or off + length > limit:
            raise ValueError("invalid load command size")
        if cmd == LC_SEGMENT_64:
            if length < 72:
                raise ValueError("short LC_SEGMENT_64")
            segname = _name(data[off + 8:off + 24])
            count, = struct.unpack_from("<I", data, off + 64)
            if count > 64 or 72 + count * 80 > length:
                raise ValueError("invalid number of Mach-O sections")
            for i in range(count):
                pos = off + 72 + i * 80
                secname = _name(data[pos:pos + 16])
                section_seg = _name(data[pos + 16:pos + 32])
                if section_seg != segname:
                    raise ValueError("mismatched segment and section name")
                size, = struct.unpack_from("<Q", data, pos + 40)
                file_off, = struct.unpack_from("<I", data, pos + 48)
                if size > len(data) or file_off > len(data) - size:
                    raise ValueError("Mach-O section outside file")
                key = segname, secname
                if key in found:
                    raise ValueError("duplicate Mach-O section " + str(key))
                found[key] = data[file_off:file_off + size]
        off += length
    if off != limit:
        raise ValueError("Mach-O commands do not fill declared area")
    return found


def read_properties(txt):
    out = {}
    for line in txt.splitlines():
        if line.startswith("# "):
            parts = line[2:].split()
            if len(parts) >= 2:
                out[parts[0]] = parts[1]
    return out


def reconstruct(main_guest, fixups, cuts):
    params = read_properties(fixups)
    build_id = read_properties(cuts).get("BUILD_ID")
    if build_id != SUPPORTED_MAIN_BUILD_ID:
        raise ValueError("unverified prepared game build ID: " + str(build_id))
    try:
        text_end = int(params["TEXT_END"], 0)
        patch_off = int(params["PATCH_OFF"], 0)
        target = int(params["TARGET"], 0)
    except (ValueError, KeyError) as e:
        raise ValueError("missing or invalid main.fixups.tsv fields") from e
    if target != 0 or text_end < 4096 or patch_off < text_end:
        raise ValueError("not an Apple runtime image of the supported build")
    segments = extract_macho_sections(main_guest)
    x0 = segments.get(("__TEXT", "__dmmainx0"))
    w = segments.get(("__DMmainW", "__img"))
    rounded_text = (text_end + 0x3fff) & ~0x3fff
    if x0 is None or w is None or len(x0) != rounded_text or (
            len(x0) + len(w) != patch_off):
        raise ValueError("Guest sections disagree with main.fixups.tsv")
    image = x0 + w
    mod0, = struct.unpack_from("<I", image, 4)
    if mod0 + 4 > len(image) or image[mod0:mod0 + 4] != b"MOD0":
        raise ValueError("reassembled guest image has no MOD0")
    return image, text_end


def analyze_payload(payload):
    import nsoimg  # lz4 dependency comes from the project's NSO helper
    with zipfile.ZipFile(payload) as archive:
        names = set(archive.namelist())
        if not {GUEST, FIXUPS, CUTS} <= names:
            raise ValueError("prepared source-payload.zip lacks signed game sections or identity metadata")
        image, text_end = reconstruct(archive.read(GUEST),
                                      archive.read(FIXUPS).decode("utf-8"),
                                      archive.read(CUTS).decode("utf-8"))
    result = inspect(image, text_end, nsoimg.relocs(image),
                     nsoimg.func_starts(image, text_end))
    result["source"] = "owner_prepared_ios_guest_macho"
    result["verified_build_id"] = SUPPORTED_MAIN_BUILD_ID
    # Disclose function RVAs and call RVAs only, not proprietary game bytes.
    for e in result["entries"]:
        e.pop("first_16_bytes", None)
    return result


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source_payload_zip", type=Path,
                    help="Verified source-payload.zip from your own prepared package")
    ap.add_argument("--out", type=Path)
    args = ap.parse_args(argv)
    result = analyze_payload(args.source_payload_zip)
    rendered = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.out:
        args.out.write_text(rendered, encoding="utf-8")
    print(rendered, end="")


if __name__ == "__main__":
    main()
