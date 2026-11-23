#!/usr/bin/env python3
"""Validate observed Switch 3.3 hire-path branches without patching game code.

Only fixed game build IDs and signed owner-prepared Guest images are accepted.
This proves an instruction target, not the semantic role of any called function.
"""
import argparse
import json
from pathlib import Path
import struct
import zipfile

from hire_handler_guest import reconstruct, GUEST, FIXUPS, CUTS

# Branch relationships independently observed in the real prepared 3.3 guest
# image (private read-only job #43916067631). This is not a patch plan.
EXPECTED = (
    (0x6dadcc, "BL", 0x7f2320),  # C2S handler -> target inspection
    (0x6dadf8, "B", 0x7ef690),   # tail call into transaction routine
    (0x7ef778, "BL", 0x6c3130), # existing pet query
    (0x7efd64, "BL", 0x6875a0), # transaction path
    (0x687648, "BL", 0x6c3130), # existing pet query again
    (0x6877f0, "BL", 0x6c0c30), # existing pet processing
    (0x68786c, "BL", 0x7cb6d0), # old pet handling
    (0x687940, "BL", 0x6c1b00), # pet-list processing
)


def decode_branch(image, rva, text_end):
    if rva < 0 or rva & 3 or rva + 4 > text_end or rva + 4 > len(image):
        raise ValueError("branch site outside verified executable text")
    insn, = struct.unpack_from("<I", image, rva)
    opcode = insn & 0xfc000000
    if opcode not in (0x14000000, 0x94000000):
        raise ValueError(f"site main+0x{rva:x} is neither B nor BL")
    displacement = insn & 0x03ffffff
    if displacement & 0x02000000:
        displacement -= 0x04000000
    dest = rva + displacement * 4
    if dest < 0 or dest >= text_end or dest & 3:
        raise ValueError(f"branch target outside verified executable text at main+0x{rva:x}")
    return ("BL" if opcode == 0x94000000 else "B"), dest


def verify(image, text_end, expected=EXPECTED):
    matches = []
    errors = []
    for site, opname, target in expected:
        try:
            actual = decode_branch(image, site, text_end)
            if actual != (opname, target):
                errors.append(f"main+0x{site:x}: expected {opname} 0x{target:x}, "
                              f"got {actual[0]} 0x{actual[1]:x}")
            else:
                matches.append({"site": f"0x{site:x}", "kind": opname,
                                "target": f"0x{target:x}"})
        except ValueError as e:
            errors.append(str(e))
    if errors:
        raise ValueError("Verified native hire-path changed: " + "; ".join(errors))
    return {"status": "read_only_branch_relationships_verified",
            "verified_sites": len(matches), "branches": matches,
            "warning": "Direct branches do not establish hireling lifecycle or safe patch points."}


def inspect_prepared(payload):
    with zipfile.ZipFile(payload) as z:
        image, text_end = reconstruct(z.read(GUEST),
                                      z.read(FIXUPS).decode("utf-8"),
                                      z.read(CUTS).decode("utf-8"))
    return verify(image, text_end)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source_payload_zip", type=Path)
    ap.add_argument("--out", type=Path)
    args = ap.parse_args(argv)
    report = inspect_prepared(args.source_payload_zip)
    body = json.dumps(report, indent=2) + "\n"
    print(body, end="")
    if args.out:
        args.out.write_text(body, encoding="utf-8")


if __name__ == "__main__":
    main()
