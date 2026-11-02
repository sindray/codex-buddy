#!/usr/bin/env python3
"""Build the LVGL fonts from the pinned LVGL OFL-licensed Noto Sans SC source."""
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[2]
text=''.join((ROOT/'main'/name).read_text() for name in ('buddy_state.c','buddy_ui.c','buddy_appearance.c'))
symbols=''.join(sorted(set(re.findall(r'[\u3400-\u9fff]',text))))
for size in (16,20,24):
    subprocess.run([str(ROOT/'tools/font-tool/node_modules/.bin/lv_font_conv'),
        '--font',str(ROOT/'managed_components/lvgl__lvgl/tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf'),
        '--range','0x20-0x7E','--symbols',symbols,'--size',str(size),'--bpp','4',
        '--format','lvgl','--no-compress','--lv-font-name',f'buddy_font_{size}',
        '--lv-include','lvgl.h','--output',str(ROOT/f'assets/fonts/buddy_font_{size}.c')],check=True)
for size in (16,20,24):
    output=ROOT/f'assets/fonts/buddy_font_{size}.c'
    generated=output.read_text().replace(str(ROOT)+'/', '').rstrip()+'\n'
    output.write_text(generated)
    covered={int(cp,16) for cp in re.findall(r'/\* U\+([0-9A-F]+)',generated)}
    missing={ord(c) for c in symbols}-covered
    if missing:raise RuntimeError(f'Missing glyphs: {sorted(missing)}')
print(f'Generated fonts: 95 ASCII + {len(symbols)} Chinese glyphs at 16/20/24 px')
