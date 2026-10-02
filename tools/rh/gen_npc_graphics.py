#!/usr/bin/env python3
"""Romhack NPC graphics (run once; the PNGs/palette are committed):
 - graphics/object_events/pics/people/rh_ball_seller.png : the Celadon Poke Ball seller, the FRLG Camper recoloured
   to look like a Poke Ball (red cap and shirt, black belt with a white button, white shorts).
 - graphics/object_events/pics/misc/rh_barricade.png     : 16x16 striped road barricade (Route 3).
 - graphics/object_events/palettes/rh_ball_seller.pal     : their shared palette."""
import os
from PIL import Image

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')) + '/'
CAMPER = R + 'graphics/object_events/pics/people/camper_frlg.png'

src = Image.open(CAMPER)
pal = [tuple(src.getpalette()[i:i + 3]) for i in range(0, 48, 3)]
# Camper greens -> Poke Ball red (top half); unused slots 5/6 -> greys for the white bottom half.
pal[8] = (248, 96, 88)     # red highlight
pal[9] = (216, 40, 48)     # red
pal[0xa] = (120, 24, 32)   # dark red
pal[5] = (200, 200, 216)   # light grey (white shading)
pal[6] = (136, 136, 152)   # grey

px = src.load()
W, H = src.size
out = Image.new('P', (W, H), 0)
flat = [c for rgb in pal for c in rgb]
out.putpalette(flat + [0] * (768 - len(flat)))
o = out.load()
WHITE = {8: 0xe, 9: 5, 0xa: 6}          # below the belt: green -> white / grey / dark grey
FRONT_FRAMES = {0, 3, 4, 9}
for f in range(W // 16):
    # The belt: the lowest row with a run of 4 black pixels; without one, the shorts start at a fixed row.
    belt = None
    for y in range(26, 21, -1):                 # waist height only (the feet have black runs too)
        row = [px[f * 16 + x, y] for x in range(16)]
        if any(row[x:x + 4] == [0xf] * 4 for x in range(13)):
            belt = y
            break
    bottom = (belt + 1) if belt is not None else (25 if f in (0, 1, 2) else 26)
    for y in range(H):
        for x in range(16):
            i = px[f * 16 + x, y]
            if y >= bottom and i in WHITE:
                i = WHITE[i]
            o[f * 16 + x, y] = i
    if belt is not None and f in FRONT_FRAMES:
        row = [px[f * 16 + x, belt] for x in range(16)]
        x0 = next(x for x in range(13) if row[x:x + 4] == [0xf] * 4)
        o[f * 16 + x0 + 1, belt] = 0xe          # the Poke Ball button
        o[f * 16 + x0 + 2, belt] = 0xe
out.save(R + 'graphics/object_events/pics/people/rh_ball_seller.png')

# Barricade, 16x16: two striped boards (red / white) on grey legs, black outline.
B = [
    "................",
    "fffffffffffffff.",
    "f9e9e9e9e9e9e9f.",
    "fe9e9e9e9e9e9ef.",
    "fffffffffffffff.",
    ".f6f.......f6f..",
    ".f5f.......f5f..",
    "fffffffffffffff.",
    "f9e9e9e9e9e9e9f.",
    "fe9e9e9e9e9e9ef.",
    "fffffffffffffff.",
    ".f6f.......f6f..",
    ".f5f.......f5f..",
    ".f6f.......f6f..",
    "ff6ff.....ff6ff.",
    "fffff.....fffff.",
]
bar = Image.new('P', (16, 16), 0)
bar.putpalette(flat + [0] * (768 - len(flat)))
bp = bar.load()
for y, r in enumerate(B):
    for x, ch in enumerate(r):
        bp[x, y] = 0 if ch == '.' else int(ch, 16)
os.makedirs(R + 'graphics/object_events/pics/misc', exist_ok=True)
bar.save(R + 'graphics/object_events/pics/misc/rh_barricade.png')

with open(R + 'graphics/object_events/palettes/rh_ball_seller.pal', 'w', newline='\r\n') as fh:
    fh.write('JASC-PAL\n0100\n16\n')
    for c in pal:
        fh.write('%d %d %d\n' % c)
print('ok')
