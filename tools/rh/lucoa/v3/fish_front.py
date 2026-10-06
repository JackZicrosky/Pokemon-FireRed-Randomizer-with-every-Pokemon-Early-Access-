# Lucoa front fishing frames 8-11 (32x32), hand-drawn after Leaf's motion: rod up at her side,
# wind-up with the rod raised, cast down to the left, rod pointing straight out in front.
import sys
sys.path.insert(0, sys.argv[1])
from owpal import *
S = sys.argv[1]
U = load(S + '/owner_normal2.txt')
HEAD = [r[:] for r in U[0][10:25]]          # horns .. shoulders (15 rows)
def blank(): return [['.'] * 32 for _ in range(32)]
def put(g, y0, x0, rows):
    for j, r in enumerate(rows):
        for i, c in enumerate(r):
            if c != ' ' and 0 <= y0 + j < 32 and 0 <= x0 + i < 32: g[y0 + j][x0 + i] = c
def rod(g, pts, tip=True):
    """rod: list of (x, y) cells, dark shaft with a light top edge"""
    for k, (x, y) in enumerate(pts):
        g[y][x] = 'V' if k % 2 else 'D'
    x, y = pts[0]
    if tip: g[y][x] = 'p'
BODY_SIDE = [           # hands gripping the rod at her right side (image left), crouched
    "KSSKcOTVTTVTOccK",
    "KSOKcSTTTTTTOScK",
    ".KK.KSDDDDDDDOSK",
    "...KTTDDKKDDTTK.",
    "...KpPKK..KKPpK.",
    "....KK......KK..",
]
BODY_UP = [             # arms raised (wind-up): only the torso below the shoulders, legs bent
    ".KccKTVTTVTKccK.",
    ".KcKTTTTTTTTKcK.",
    "..KKDDDDDDDDKK..",
    "..KTTDDKKDDTTK..",
    "..KpPKK..KKPpK..",
    "...KK......KK...",
]
BODY_FRONT = [          # hands together in front of her holding the rod
    ".KccKSSOOSSKccK.",
    ".KcKKOSSSSOKKcK.",
    "..KKDDDDDDDDKK..",
    "..KTTDDKKDDTTK..",
    "..KpPKK..KKPpK..",
    "...KK......KK...",
]
F = []
# 8: rod held upright at her side, slight crouch
g = blank(); ox, oy = 9, -2
put(g, 10 + oy, ox, HEAD); put(g, 25 + oy, ox, BODY_SIDE)
rod(g, [(9, y) for y in range(2, 23)])
for y in range(2, 23): g[y][8] = 'K'; g[y][10] = 'K' if g[y][10] == '.' else g[y][10]
g[1][9] = 'K'
F.append(g)
# 9: wind-up, rod swung back over her shoulder, arms up (whole figure higher, like Leaf)
g = blank(); ox, oy = 9, -8
put(g, 10 + oy, ox, HEAD); put(g, 25 + oy, ox, BODY_UP)
put(g, 12, 6, ["KSSK", "KSOK", ".KK."])                       # raised hands beside her head
pts = [(5, 1), (5, 2), (6, 3), (6, 4), (6, 5), (7, 6), (7, 7), (7, 8), (8, 9), (8, 10), (8, 11)]
for x, y in pts: g[y][x - 1] = 'K' if g[y][x - 1] == '.' else g[y][x - 1]
rod(g, pts)
F.append(g)
# 10: cast, rod swung down to the left, crouched
g = blank(); ox, oy = 9, -6
put(g, 10 + oy, ox, HEAD); put(g, 25 + oy, ox, BODY_FRONT)
pts = [(5, 29), (5, 28), (6, 27), (6, 26), (7, 25), (7, 24), (8, 23), (8, 22), (9, 21), (10, 20)]
for x, y in pts: g[y][x - 1] = 'K' if g[y][x - 1] == '.' else g[y][x - 1]
rod(g, pts)
put(g, 18, 9, ["KSSK", "KSOK", ".KK."])
F.append(g)
# 11: rod pointing out in front of her (straight down on screen), line in the water
g = blank(); ox, oy = 8, -9
put(g, 10 + oy, ox, HEAD); put(g, 25 + oy, ox, BODY_FRONT)
pts = [(15, y) for y in range(31, 21, -1)]
for x, y in pts:
    g[y][14] = 'K'; g[y][16] = 'K'
rod(g, pts)
F.append(g)
save(F, S + '/fish_front_v3.txt', '# fish 8-11\n')
leaf = []
from PIL import Image
import os
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
im = Image.open(REPO + '/graphics/rh_player/leaf/fish.png')
rg = im.convert('RGBA'); px = rg.load()
for y in range(im.height):
    for x in range(im.width):
        if im.getpixel((x, y)) == 0: px[x, y] = (0, 0, 0, 0)
strip([F, [rg.crop((i * 32, 0, i * 32 + 32, 32)) for i in (8, 9, 10, 11)]], S + '/fish_front.png', 8)
gif(F, S + '/fish_front.gif', [200, 150, 150, 600], 6)
