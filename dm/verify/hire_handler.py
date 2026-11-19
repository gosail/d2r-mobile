#!/usr/bin/env python3
"""Read-only 3.3 NSO hire-handler discovery; never patches or distributes an NSO.

Uses the original game's command-handler table documented by the project's
command-packet verification work. Produces candidate offsets, not proof of
the internal hire / replace / save semantics.
"""
import argparse
import json
from pathlib import Path
import struct

SUPPORTED_MAIN_BUILD_ID = "db0507f36b84200ac05f75d76ed5026f"
C2S_TABLE = 0x20A4190
C2S_ENTRY_COUNT = 0x67
OPCODES = (0x36, 0x38, 0x62)  # hire; NPC action; resurrect (legacy hypotheses)
R_AARCH64_ABS64 = 257
R_AARCH64_RELATIVE = 1027


def handler_address(image, text_end, table, opcode, relocations):
    """Resolve an NSO pointer after the loader's RELA relocation.

    RELA addends (not the original table bytes) are authoritative if present.
    We reject unknown relocation types and ambiguous pointer definitions.
    """
    if not 0 <= opcode < C2S_ENTRY_COUNT:
        raise ValueError("invalid command number")
    pos = table + opcode * 8
    if pos < text_end or pos + 8 > len(image):
        raise ValueError("command handler table is not in rodata/data")
    entries = [r for r in relocations if r[0] == pos]
    if len(entries) > 1:
        raise ValueError("multiple relocations for command handler slot")
    raw, = struct.unpack_from("<Q", image, pos)
    source = "raw"
    if entries:
        off, kind, name, symbol, addend = entries[0]
        if kind == R_AARCH64_RELATIVE:
            if symbol is not None:
                raise ValueError("unexpected symbol in relative relocation")
            ptr = addend
        elif kind == R_AARCH64_ABS64 and symbol is not None:
            ptr = symbol + addend
        else:
            raise ValueError(f"unsupported handler relocation type {kind}")
        source = "rela"
    else:
        ptr = raw
    if not ptr:
        return None
    if not 0 <= ptr < text_end or ptr % 4:
        raise ValueError(f"handler for opcode 0x{opcode:02x} outside text: 0x{ptr:x}")
    return {"opcode": f"0x{opcode:02x}", "table_slot": f"0x{pos:x}",
            "handler": f"0x{ptr:x}", "source": source,
            "raw_slot": f"0x{raw:x}"}


def calls_from_function(image, start, end):
    """List direct ARM64 BL calls; no speculative indirect-call claims."""
    calls = []
    for addr in range(start, end - 3, 4):
        w, = struct.unpack_from("<I", image, addr)
        if (w & 0xFC000000) != 0x94000000:
            continue
        imm26 = w & 0x3FFFFFF
        delta = (imm26 - (1 << 26) if imm26 & (1 << 25) else imm26) * 4
        target = addr + delta
        if 0 <= target < len(image):
            calls.append({"at": f"0x{addr:x}", "target": f"0x{target:x}"})
    return calls


def inspect(image, text_end, relocs, starts, table=C2S_TABLE):
    by_pos = {}
    for relocation in relocs:
        if table <= relocation[0] < table + C2S_ENTRY_COUNT * 8:
            by_pos.setdefault(relocation[0], []).append(relocation)
    entries = []
    for opcode in OPCODES:
        candidate = handler_address(image, text_end, table, opcode,
                                    by_pos.get(table + opcode * 8, []))
        if candidate is None:
            raise ValueError(f"opcode 0x{opcode:02x} has no handler in this build")
        at = int(candidate["handler"], 16)
        # Original code can be a thunk or a function without an FDE. Do not
        # pretend an arbitrary nearest start is necessarily the function.
        if at in starts:
            i = starts.index(at)
            nxt = starts[i + 1] if i + 1 < len(starts) else text_end
            end = min(at + 0x800, nxt, text_end)
            candidate["function_boundary"] = "exact_eh_frame"
        else:
            end = min(at + 0x80, text_end)
            candidate["function_boundary"] = "unknown_short_scan"
        candidate["first_16_bytes"] = image[at:min(at + 16, text_end)].hex()
        candidate["direct_calls"] = calls_from_function(image, at, end)
        entries.append(candidate)
    return {"status": "candidate_addresses_only", "table": f"0x{table:x}",
            "entries": entries,
            "warning": ("Finding a C2S handler does not prove where the native "
                        "one-hireling replacement, gear ownership, or save path lives.")}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("main_nso", help="Owner-extracted original Switch 3.3 main NSO")
    ap.add_argument("--out", type=Path, help="Optional JSON report; no NSO bytes copied")
    args = ap.parse_args(argv)
    raw = Path(args.main_nso).read_bytes()
    if len(raw) < 0x70 or raw[:4] != b"NSO0":
        ap.error("not a valid NSO0 file")
    build = raw[0x40:0x50].hex()
    if build != SUPPORTED_MAIN_BUILD_ID:
        ap.error("unverified NSO build ID " + build + "; refusing offset guesses")
    # Import only when a real NSO is read; pure tests need no third-party lz4.
    import nsoimg
    image = nsoimg.load(args.main_nso)
    text_end, = struct.unpack_from("<I", raw, 0x18)
    report = inspect(image, text_end, nsoimg.relocs(image),
                     nsoimg.func_starts(image, text_end))
    report["build_id"] = build
    out = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    if args.out:
        args.out.write_text(out, encoding="utf-8")
    print(out, end="")


if __name__ == "__main__":
    main()
