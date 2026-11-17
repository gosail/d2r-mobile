#!/usr/bin/env python3
"""Rebuild three *pass-through* host call gates in an owner-prepared iOS Guest.

Only for DM_OFFLINE_DUAL_MERC_TEST=1, before IPA signing. No source code,
game NSO, decrypted game data, or modified Guest are committed to GitHub.

The original BL calls are relocated through gates which ALWAYS dispatch to
the original native functions in dm/host/diag.c. This does NOT enable two
mercenaries; it permits before/after lifecycle observation on the real game.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from dualmerc_native_cuts import BUILD_ID, host_audit_cuts

PAGE = 0x4000
STUB_WORDS = 10
STUB_BYTES = STUB_WORDS * 4


def _name(b):
    return b.partition(b"\0")[0].decode("ascii")


def macho_sections(raw):
    if len(raw) < 32:
        raise ValueError("Truncated Guest Mach-O")
    magic, cpu, _, filetype, ncmds, sizecmds, _, _ = struct.unpack_from("<IiiIIIII", raw)
    if (magic, cpu, filetype) != (0xfeedfacf, 0x100000c, 6):
        raise ValueError("Not the expected unsigned arm64 MH_DYLIB")
    stop = 32 + sizecmds
    if not 0 < ncmds < 128 or sizecmds > 0x4000 or stop > len(raw):
        raise ValueError("Invalid Mach-O command table")
    sections = {}
    pos = 32
    for _ in range(ncmds):
        if pos + 8 > stop:
            raise ValueError("Truncated Mach-O command")
        command, n = struct.unpack_from("<II", raw, pos)
        if n < 8 or pos + n > stop:
            raise ValueError("Invalid Mach-O command size")
        if command == 0x19:
            if n < 72:
                raise ValueError("Truncated segment")
            seg = _name(raw[pos + 8:pos + 24])
            count, = struct.unpack_from("<I", raw, pos + 64)
            if count > 64 or 72 + count * 80 > n:
                raise ValueError("Invalid Mach-O section count")
            for i in range(count):
                sp = pos + 72 + 80 * i
                sec = _name(raw[sp:sp + 16])
                if _name(raw[sp + 16:sp + 32]) != seg:
                    raise ValueError("Invalid section/segment relation")
                size, = struct.unpack_from("<Q", raw, sp + 40)
                off, = struct.unpack_from("<I", raw, sp + 48)
                if off > len(raw) or size > len(raw) - off:
                    raise ValueError("Out-of-bounds Guest section")
                key = (seg, sec)
                if key in sections:
                    raise ValueError("Duplicate Guest section")
                sections[key] = (off, size)
        pos += n
    if pos != stop:
        raise ValueError("Malformed Mach-O commands")
    return sections


def properties(content):
    result = {}
    for line in content.splitlines():
        if not line.startswith("# "):
            continue
        tokens = line[2:].split()
        if len(tokens) == 2:
            result[tokens[0]] = tokens[1]
    return result


def b_branch(at, target):
    delta = target - at
    if delta % 4 or not -(1 << 27) <= delta < (1 << 27):
        raise ValueError("ARM64 B out of range")
    return 0x14000000 | ((delta >> 2) & 0x3ffffff)


def adrp(reg, at, target):
    delta = ((target & ~0xfff) - (at & ~0xfff)) >> 12
    if not -(1 << 20) <= delta < (1 << 20):
        raise ValueError("ARM64 ADRP out of range")
    return (0x90000000 | ((delta & 3) << 29) |
            (((delta >> 2) & 0x7ffff) << 5) | reg)


def gate_stub(at, return_to, key_page, kgate):
    if at % 4 or return_to % 4 or key_page % 4 or not 0 <= kgate < 4096:
        raise ValueError("Invalid gate argument alignment")
    words = [
        0xa9bf7bfd,                                # stp x29,x30,[sp,#-16]!
        0xd53bd07e,                                # mrs x30,tpidrro_el0
        adrp(29, at + 8, key_page),
        0xf9400000 | (kgate << 10) | (29 << 5) | 29, # ldr x29,[x29,#kgate*8]
        0x8b000000 | (29 << 16) | (30 << 5) | 30,  # add x30,x30,x29
        0xf9400000 | (30 << 5) | 30,               # ldr x30,[x30]
        0xf9400000 | (31 << 5) | 29,               # ldr x29,[sp]
        0xd63f0000 | (30 << 5),                   # blr x30
        0xa8c17bfd,                               # ldp x29,x30,[sp],#16
        b_branch(at + 9 * 4, return_to),
    ]
    return struct.pack("<10I", *words)


def retrofit_bytes(guest, fixups, cuts, gates, sites=None):
    """Return patched files in memory; any failed check leaves caller files alone.

    sites argument is only for synthetic no-game-bytes unit tests. Real builds
    always use host_audit_cuts(BUILD_ID), the exact verified 3.3 old bytes.
    """
    fp, cp = properties(fixups), properties(cuts)
    if cp.get("BUILD_ID") != BUILD_ID:
        raise ValueError("Unverified original Switch 3.3 build ID")
    try:
        text_end = int(fp["TEXT_END"], 0)
        patch_off = int(fp["PATCH_OFF"], 0)
        key_page = int(fp["KEY_PAGE_OFF"], 0)
        kgate = int(fp["KEY_KGATE_IDX"], 0)
        target = int(fp["TARGET"], 0)
    except (KeyError, ValueError) as e:
        raise ValueError("Incomplete or invalid prepared Guest fixups") from e
    if (target != 0 or patch_off % PAGE or text_end <= 0 or
            not text_end < key_page < patch_off or key_page % PAGE):
        raise ValueError("Unsupported Apple guest layout/key-page offset")
    sections = macho_sections(guest)
    try:
        x0, sz0 = sections[("__TEXT", "__dmmainx0")]
        x1, sz1 = sections[("__TEXT", "__dmmainx1")]
        w, wsz = sections[("__DMmainW", "__img")]
    except KeyError as e:
        raise ValueError("Prepared Guest missing main code/image sections") from e
    x0_size = (text_end + PAGE - 1) & ~(PAGE - 1)
    if sz0 != x0_size or wsz != patch_off - x0_size or sz1 % PAGE:
        raise ValueError("Guest sections do not match original fixups")
    if not (x0 + sz0 <= len(guest) and x1 + sz1 <= len(guest)):
        raise ValueError("Guest section bounds exceed source")
    # Read-only verify original byte-verified call sites, and existing gate rows.
    entries = sites if sites is not None else host_audit_cuts(BUILD_ID)
    old_gates = [line for line in gates.splitlines() if line and not line.startswith("#")]
    old_returns = set()
    for line in old_gates:
        columns = line.split()
        if len(columns) != 4:
            raise ValueError("Malformed original gate entry")
        rv = int(columns[0], 0)
        if rv in old_returns:
            raise ValueError("Duplicate original gate return offset")
        old_returns.add(rv)
    original = bytearray(guest)
    for site, old, _, _, scope in entries:
        if scope != "仅宿主" or site % 4 or site < 0 or site + 4 > text_end:
            raise ValueError("Invalid native site/scope")
        if original[x0 + site:x0 + site + 4].hex() != old:
            raise ValueError(f"Unverified or already patched original callsite 0x{site:x}")
    if any("双佣兵实验原生调用观测" in line for line in cuts.splitlines()):
        raise ValueError("Native dual-merc audit already applied")
    # Aligned zero-filled trailing slack in the executable x1 section.
    body = original[x1:x1 + sz1]
    used = len(body.rstrip(b"\0"))
    first = (used + 3) & ~3
    need = len(entries) * STUB_BYTES
    if first + need > sz1 or body[first:first + need] != bytes(need):
        raise ValueError("Not enough verified unused executable x1 padding")
    new_cuts = []
    new_gates = []
    for i, (site, old, svc, desc, scope) in enumerate(entries):
        stub_rva = patch_off + first + i * STUB_BYTES
        trampoline = gate_stub(stub_rva, site + 4, key_page, kgate)
        if len(trampoline) != STUB_BYTES:
            raise ValueError("Wrong gate length")
        ret_rva = stub_rva + 8 * 4
        if ret_rva in old_returns:
            raise ValueError("Gate return collision")
        original[x1 + first + i * STUB_BYTES:
                 x1 + first + (i + 1) * STUB_BYTES] = trampoline
        # The site jumps to the hosted gate, whose return path resumes at BL+4.
        original[x0 + site:x0 + site + 4] = struct.pack("<I", b_branch(site, stub_rva))
        new_cuts.append(f"{site:#x}\t{old}\t{original[x0+site:x0+site+4].hex()}\t{scope}\t{desc}")
        # The original site is a B to the new trampoline; the host gate
        # registration encodes the SVC that would have appeared in patch_nso.
        # The cut's four-byte hex string is little-endian ARM64 instruction data.
        svc_insn, = struct.unpack("<I", bytes.fromhex(svc))
        if (svc_insn & 0xffe0001f) != 0xd4000001:
            raise ValueError("Invalid host SVC instruction encoding")
        num = (svc_insn >> 5) & 0xffff
        new_gates.append(f"{ret_rva:#x}\tsvc\t{num}\t{site:#x}")
    # No bytes outside the exact 3 code sites and 120-byte scratch area changed.
    endings = lambda body: body + ("\n" if body and not body.endswith("\n") else "")
    return (bytes(original), endings(cuts) + "\n".join(new_cuts) + "\n",
            endings(gates) + "\n".join(new_gates) + "\n",
            {"status": "host_native_pass_through_gates_only", "sites": len(entries),
             "patch_start_rva": f"0x{patch_off + first:x}",
             "source_text_end": f"0x{text_end:x}", "new_gate_count": len(new_gates),
             "original_hire_policy_changed": False, "second_mercenary_enabled": False})


def retrofit(stage):
    stage = Path(stage)
    guest = stage / "Frameworks/Guest.framework/Guest"
    fixup = stage / "dm/main.fixups.tsv"
    cut = stage / "dm/main.cuts.tsv"
    gate = stage / "dm/main.gates.tsv"
    old = [p.read_bytes() for p in (guest, fixup, cut, gate)]
    new_guest, new_cuts, new_gates, report = retrofit_bytes(
        old[0], old[1].decode("utf-8"), old[2].decode("utf-8"),
        old[3].decode("utf-8"))
    # Caller stages a disposable build tree. All validations complete before
    # modifying files, then os.replace preserves original mode/safe signing.
    import os
    for path, data in ((guest, new_guest), (cut, new_cuts.encode()),
                       (gate, new_gates.encode())):
        tmp = path.with_name(path.name + ".dualmerc.tmp")
        tmp.write_bytes(data)
        os.chmod(tmp, path.stat().st_mode & 0o777)
        os.replace(tmp, path)
    print(json.dumps(report, sort_keys=True))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", type=Path, help="Temporary unsigned iOS payload staging dir")
    args = parser.parse_args()
    retrofit(args.stage)


if __name__ == "__main__":
    main()
