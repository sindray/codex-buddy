#!/usr/bin/env python3
"""Build the LVGL fonts from the vendored, OFL-licensed interface subset."""
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[2]
text=''.join((ROOT/'main'/name).read_text() for name in ('buddy_state.c','buddy_ui.c'))
symbols=''.join(sorted(set(re.findall(r'[\u3400-\u9fff]',text))))
for size in (16,20,24):
    subprocess.run([str(ROOT/'tools/font-tool/node_modules/.bin/lv_font_conv'),
        '--font',str(ROOT/'assets/fonts/NotoSansSC-Buddy.ttf'),
        '--range','0x20-0x7E','--symbols',symbols,'--size',str(size),'--bpp','4',
        '--format','lvgl','--no-compress','--lv-font-name',f'buddy_font_{size}',
        '--lv-include','lvgl.h','--output',str(ROOT/f'assets/fonts/buddy_font_{size}.c')],check=True)
print(f'Generated fonts: 95 ASCII + {len(symbols)} Chinese glyphs at 16/20/24 px')
