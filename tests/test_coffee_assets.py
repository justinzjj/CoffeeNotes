#!/usr/bin/env python3
"""Check the maintained glyph inventory against all fixed UI strings."""
import json
import re
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

class AssetTests(unittest.TestCase):
    def test_all_ui_codepoints_in_generated_inventory(self):
        source = (ROOT / 'main/coffee_ui.c').read_text()
        required = set(range(32, 127)) | {ord(c) for c in source if ord(c) > 127}
        generated = {int(c, 16) for c in re.findall(r'0x([0-9A-F]+)', (ROOT / 'assets/fonts/coffee_glyphs.h').read_text())}
        self.assertLessEqual(required, generated)
        self.assertNotIn(0x9F98, generated)
        manifest = json.loads((ROOT / 'assets/fonts/coffee_font_manifest.json').read_text())
        self.assertEqual(len(generated), manifest['glyphs'])
        self.assertEqual(manifest['converter'], 'lv_font_conv 1.5.3')
    def test_license_and_linked_sizes(self):
        self.assertIn('SIL OPEN FONT LICENSE', (ROOT / 'assets/fonts/OFL.txt').read_text())
        build = (ROOT / 'main/CMakeLists.txt').read_text()
        for size in (14, 16, 20):
            path = ROOT / f'assets/fonts/coffee_font_{size}.c'
            self.assertIn(path.name, build)
            self.assertIn(f'const lv_font_t coffee_font_{size}', path.read_text())
        config = (ROOT / 'sdkconfig.defaults').read_text()
        self.assertIn('CONFIG_LV_TXT_ENC_UTF8=y', config)
        self.assertIn('CONFIG_LV_USE_FONT_PLACEHOLDER=y', config)

if __name__ == '__main__':
    unittest.main()
