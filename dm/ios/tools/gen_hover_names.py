#!/usr/bin/env python3
"""Emit a class-ID -> localized string-key table from the player's monstats.txt.
Only keys are compiled in; names are looked up through the game's original
language table at runtime. This file contains no copied game strings.
Usage: gen_hover_names.py <Switch RomFS>
"""
import json
import sys
from annot_strings import excel


def emit(rows):
    # Class IDs correspond to monstats rows in the supported 3.3.94012 data.
    # NameStr is a string lookup key, NOT a translated/display name.
    keys = [(i, r.get('NameStr', '').strip()) for i, r in enumerate(rows)]
    keys = [(i, k) for i, k in keys if k and k != '0' and len(k) <= 128]
    lines = ['// Generated from player-owned game data. Do not edit or commit.',
             '#pragma once',
             'static const char *const dm_hover_mon_names[%d] = {' % max(1, len(rows))]
    lines += ['    [%d] = %s,' % (i, json.dumps(key)) for i, key in keys]
    return '\n'.join(lines + ['};', ''])


def emit_tsv(rows):
    # Runtime-only keys for Android import bundle; no localized game text in APK.
    result=[]
    for i, row in enumerate(rows):
        key=row.get('NameStr','').strip()
        if key and key!='0' and len(key)<=128 and not any(ord(c)<32 for c in key):
            result.append('%d\t%s' % (i,key))
    return '\n'.join(result)+'\n'

if __name__ == '__main__':
    if len(sys.argv) not in (2,3) or (len(sys.argv)==3 and sys.argv[2]!='--tsv'):
        raise SystemExit('Usage: gen_hover_names.py <Switch RomFS> [--tsv]')
    rows=excel(sys.argv[1], 'monstats.txt')
    print(emit_tsv(rows) if len(sys.argv)==3 else emit(rows), end='')
