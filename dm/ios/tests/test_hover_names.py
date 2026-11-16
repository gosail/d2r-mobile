#!/usr/bin/env python3
"""Smoke tests for generated NPC mouse-hover string-key tables; no game assets needed."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import types
import unittest

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
# Don't import game-resource readers: emit() is intentionally pure.
module = types.ModuleType('annot_strings')
module.excel = lambda *_: []
sys.modules['annot_strings'] = module
spec = importlib.util.spec_from_file_location('gen_hover_names', TOOLS / 'gen_hover_names.py')
hover = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hover)


class HoverNamesTests(unittest.TestCase):
    def test_preserves_class_indices_and_escapes(self):
        generated = hover.emit([{'NameStr': 'Akara'}, {'NameStr': ''},
                                {'NameStr': 'Quoted " NPC'}, {'NameStr': '0'}])
        self.assertIn('dm_hover_mon_names[4]', generated)
        self.assertIn('[0] = "Akara"', generated)
        self.assertNotIn('[1] =', generated)
        self.assertIn('[2] = ' + json.dumps('Quoted " NPC'), generated)
        self.assertNotIn('[3] =', generated)

    def test_empty_table_has_portable_size(self):
        self.assertIn('dm_hover_mon_names[1]', hover.emit([]))

    def test_rejects_suspicious_long_keys(self):
        generated = hover.emit([{'NameStr': 'A' * 500}])
        self.assertNotIn('[0] =', generated)

    def test_android_runtime_names_are_asset_free_keys(self):
        table = hover.emit_tsv([{'NameStr':'Akara'}, {'NameStr':''}, {'NameStr':'Npc\nNewline'}])
        self.assertEqual(table, '0\tAkara\n')

    def test_valid_c_header(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        if not compiler:
            self.skipTest('C compiler not installed')
        with tempfile.TemporaryDirectory() as root:
            header = Path(root) / 'hover_names.h'
            header.write_text(hover.emit([{'NameStr': 'Akara'}, {'NameStr': ''}, {'NameStr': 'Charsi'}]))
            source = Path(root) / 'hover_names.c'
            source.write_text('#include <stddef.h>\n#include "hover_names.h"\n'
                              'int main(void) { return dm_hover_mon_names[1] != NULL; }\n')
            result = subprocess.run([compiler, '-std=c11', '-Wall', '-Werror',
                                     '-fsyntax-only', str(source)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
