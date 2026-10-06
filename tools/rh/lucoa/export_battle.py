# front.png (64x64), back.png (64x320, 5 frames stacked), oak.png (64x96, 8bpp, colours at 64+i)
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bpal import BPAL, ORDER, load_rows, save_indexed
from PIL import Image
S = os.path.dirname(os.path.abspath(__file__)); OUT = S + '/pack'
front = load_rows(S + '/frames/front.txt')
back = open(S + '/frames/back.txt').read().split()
assert len(front) == 64 and len(back) == 320
for rows in (front, back):
    assert all(len(r) == 64 for r in rows) and all(c in ORDER for r in rows for c in r)
save_indexed(front, OUT + '/front.png')
save_indexed(back, OUT + '/back.png')
oak = Image.new('P', (64, 96), 0)
pal = [0] * 768
for i, ch in enumerate(ORDER):
    pal[(64 + i) * 3:(64 + i) * 3 + 3] = list(BPAL[ch][:3]) if ch != '.' else [0, 0, 0]
oak.putpalette(pal)
for y, r in enumerate(front):
    for x, ch in enumerate(r):
        i = ORDER.index(ch)
        oak.putpixel((x, y + 32), 0 if i == 0 else 64 + i)
oak.save(OUT + '/oak.png')
print('ok')
