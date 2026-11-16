#!/usr/bin/env python3
"""Read-only offline D2R dual-mercenary log correlation for a throwaway character.

No access to game memory, no patches, no save writes. Results are evidence of
observed pet lists, NOT proof of independent combat or item persistence.
"""
import argparse
import json
from pathlib import Path
import re

CANDIDATE = re.compile(
    r"DM_DUAL_MERC_PROBE：拍 (\d+) 玩家 (?:id )?(\d+) 收到候选雇佣包 0x36 len=(\d+)")
SNAPSHOT = re.compile(
    r"DM_DUAL_MERC_PROBE：拍 (\d+) 玩家 id (\d+) 服务端宠物类型7=(\d+) "
    r"\[([^\]]*)\] 客户端属主佣兵=(\d+) \[([^\]]*)\]")


def parse_units(raw, server):
    out = {}
    for field in raw.split(","):
        if not field:
            continue
        parts = field.split(":")
        if len(parts) < 3:
            continue
        try:
            unit_id, cls, mode = (int(x) for x in parts[:3])
        except ValueError:
            continue
        unit = {"class": cls, "mode": mode}
        if server:
            unit["inventory_count"] = None
            unit["inventory_status"] = None
            unit["fingerprint"] = None
            for part in parts[3:]:
                if part.startswith("a") and part[1:].isdigit():
                    unit["act_guess"] = int(part[1:])
                if part.startswith("inv") and "/" in part:
                    count, status = part[3:].split("/", 1)
                    if count.isdigit() and status.isdigit():
                        unit["inventory_count"] = int(count)
                        unit["inventory_status"] = int(status)
                if part.startswith("h") and len(part) == 9:
                    try:
                        unit["fingerprint"] = int(part[1:], 16)
                    except ValueError:
                        pass
        out[unit_id] = unit
    return out


def parse_snapshot(line):
    m = SNAPSHOT.search(line)
    if not m:
        return None
    tick, owner, server_count, srv, client_count, cli = m.groups()
    return {"tick": int(tick), "owner": int(owner),
            "server_count": int(server_count),
            "client_count": int(client_count),
            "server": parse_units(srv, True), "client": parse_units(cli, False)}


def summarize_attempt(a):
    before = a.pop("_before", None)
    after = a.pop("_after", [])
    previous = set(before["server"]) if before else set()
    last = after[-1] if after else None
    current = set(last["server"]) if last else set()
    a["before_server_ids"] = sorted(previous)
    a["last_server_ids"] = sorted(current) if last else []
    a["last_client_ids"] = sorted(last["client"]) if last else []
    a["disappeared_server_ids"] = sorted(previous - current) if last else []
    a["new_server_ids"] = sorted(current - previous) if last else []
    a["old_inventory_observations"] = {
        str(uid): {"count": before["server"][uid].get("inventory_count"),
                   "status": before["server"][uid].get("inventory_status"),
                   "fingerprint": before["server"][uid].get("fingerprint")}
        for uid in a["disappeared_server_ids"]} if before and last else {}
    a["max_simultaneously_listed_server"] = max(
        (s["server_count"] for s in after), default=0)
    a["both_lists_two"] = any(
        s["server_count"] >= 2 and s["client_count"] >= 2 and
        len(set(s["server"]).intersection(s["client"])) >= 2 for s in after)
    a["server_only_two"] = any(s["server_count"] >= 2 for s in after)
    if not before or not after:
        a["observation"] = "insufficient_snapshots"
    elif a["both_lists_two"]:
        a["observation"] = "two_units_in_both_lists"
    elif a["disappeared_server_ids"] and a["new_server_ids"]:
        a["observation"] = "replacement_in_server_list"
    elif a["server_only_two"]:
        a["observation"] = "two_units_in_server_list_only"
    else:
        a["observation"] = "no_dual_list_evidence"
    a["last_tick"] = last["tick"] if last else None
    a.pop("_before_is_forced", None)
    return a


def analyze(lines):
    attempts = []
    active = {}
    latest = {}
    for line in lines:
        c = CANDIDATE.search(line)
        if c:
            tick, owner, length = map(int, c.groups())
            if owner in active:
                summarize_attempt(active.pop(owner))
            old = latest.get(owner)
            if old and old["tick"] > tick:
                old = None
            item = {"tick": tick, "owner": owner,
                    "candidate_packet_length": length,
                    "_before": old, "_before_is_forced": False,
                    "_after": []}
            attempts.append(item)
            active[owner] = item
        s = parse_snapshot(line)
        if not s:
            continue
        owner = s["owner"]
        old = latest.get(owner)
        if old and s["tick"] < old["tick"]:
            if owner in active:
                summarize_attempt(active.pop(owner))
        latest[owner] = s
        a = active.get(owner)
        if not a:
            continue
        dt = s["tick"] - a["tick"]
        if dt < 0 or dt > 100:
            summarize_attempt(active.pop(owner))
        elif dt == 0 and not a["_before_is_forced"]:
            # Probe is invoked synchronously before native C2S dispatch.
            a["_before"] = s
            a["_before_is_forced"] = True
        else:
            a["_after"].append(s)
    for a in list(active.values()):
        summarize_attempt(a)
    return {
        "candidate_requests": len(attempts),
        "attempts": attempts,
        "limitations": ("Pet-list membership and non-cryptographic runtime item IDs "
                        "cannot prove gear ownership, combat, death/revive identity, "
                        "or persistence after saving and reloading.")
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("game_log", type=Path,
                    help="Player-exported experimental DM_DUAL_MERC_PROBE log")
    ap.add_argument("--json", type=Path, help="Optional JSON report file")
    args = ap.parse_args()
    result = analyze(args.game_log.read_text(encoding="utf-8", errors="replace").splitlines())
    print(f"Candidate 0x36 requests: {result['candidate_requests']}")
    for i, entry in enumerate(result["attempts"], 1):
        print(f"#{i} tick={entry['tick']}, owner={entry['owner']}, "
              f"state={entry['observation']}, old={entry['disappeared_server_ids']}, "
              f"new={entry['new_server_ids']}")
    if not result["attempts"]:
        print("No candidate 0x36 observed; the operation opcode is not confirmed.")
    print("Only read-only observations; cannot prove equipment survival or dual combat.")
    if args.json:
        args.json.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n",
                             encoding="utf-8")


if __name__ == "__main__":
    main()
