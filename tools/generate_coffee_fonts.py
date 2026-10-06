#!/usr/bin/env python3
"""Build the licensed CoffeeNotes glyph subset with lv_font_conv 1.5.3."""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-font', type=Path, default=ROOT / 'assets/fonts/CoffeeNotesSansSC-Regular.otf')
parser.add_argument('--converter', default='lv_font_conv')
args = parser.parse_args()
source_text = (ROOT / 'main/coffee_ui.c').read_text()
points = sorted(set(range(32,127)) | {ord(c) for c in source_text if ord(c) > 127})
font = TTFont(args.source_font)
missing = set(points) - set(font.getBestCmap())
if missing:
    raise SystemExit('Missing source glyphs: ' + ', '.join(f'U+{c:04X}' for c in sorted(missing)))
sub = subset.Subsetter()
sub.populate(unicodes=points)
sub.subset(font)
for record in font['name'].names:
    if record.nameID in (1,3,4,6):
        name = 'CoffeeNotesSansSC' if record.nameID == 6 else 'CoffeeNotes Sans SC'
        record.string = name.encode(record.getEncoding())
output = ROOT / 'assets/fonts'
output.mkdir(exist_ok=True)
font_path = output / 'CoffeeNotesSansSC-Regular.otf'
font.save(font_path)
symbols = ''.join(map(chr,points))
(output / 'coffee_glyphs.txt').write_text(symbols + '\n')
(output / 'coffee_glyphs.h').write_text('#pragma once\n#include <stdint.h>\nstatic const uint32_t coffee_glyphs[] = {\n    ' + ', '.join(f'0x{c:04X}' for c in points) + '\n};\n')
for size in (14,16,20):
    subprocess.run([args.converter, '--font', str(font_path), '--range', '0x20-0x7E', '--symbols', symbols,
                    '--size', str(size), '--bpp', '4', '--format', 'lvgl', '--no-compress', '--lv-include', 'lvgl.h',
                    '--lv-font-name', f'coffee_font_{size}', '--output', str(output / f'coffee_font_{size}.c')], check=True)
(output / 'coffee_font_manifest.json').write_text(json.dumps({'family':'CoffeeNotes Sans SC (Noto Sans CJK SC subset)',
    'converter':'lv_font_conv 1.5.3', 'glyphs':len(points), 'sizes':[14,16,20], 'bpp':4, 'compressed':False,
    'otf_sha256':hashlib.sha256(font_path.read_bytes()).hexdigest()},indent=2) + '\n')
print(f'CoffeeNotes fonts: {len(points)} verified glyphs at 14/16/20 px')
