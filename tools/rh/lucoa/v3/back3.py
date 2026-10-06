# Lucoa back pic v3: Red's FR/LG throw frames (cap, face, arms), recoloured; Lucoa's long hair over her back;
# ringed horns out of the cap.
import os, sys, math
sys.path.insert(0, sys.argv[1])
from pal3 import *
from PIL import Image, ImageDraw
OUT = sys.argv[1]
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
im = Image.open(REPO + '/graphics/rh_player/red_fr/back.png')
RF = [[[('.' if im.getpixel((x, f * 64 + y)) == 0 else '0123456789abcdef'[im.getpixel((x, f * 64 + y))])
        for x in range(64)] for y in range(64)] for f in range(5)]
OFF = [(0, 0), (-7, 4), (-3, 1), (-11, -1), (-10, 5)]           # Red's cap position vs frame 0
HAIRPOLY = [
    [(27, 28), (43, 28), (44, 34), (43, 40), (42, 46), (41, 52), (42, 58), (41, 63), (17, 63), (18, 58), (16, 53),
     (18, 48), (16, 43), (19, 38), (22, 32)],
    None, None,
    [(16, 27), (32, 27), (33, 33), (30, 37), (25, 41), (19, 44), (11, 46), (5, 46), (1, 44), (0, 39), (2, 34), (8, 30)],
    [(17, 32), (33, 32), (34, 38), (30, 43), (24, 47), (15, 49), (7, 48), (2, 46), (0, 42), (3, 37), (9, 34)],
]
for f in (1, 2):
    dx, dy = OFF[f]
    HAIRPOLY[f] = [(x + dx, y + dy) for x, y in HAIRPOLY[0]]
WRIST = [[], [(0, 44, 16, 58)], [(4, 30, 18, 40)], [(42, 25, 52, 33)], [(50, 57, 60, 61)]]
# throwing arm that stays in front of the hair
ONTOP = [[(38, 44, 63, 63)], [(0, 40, 16, 63), (38, 44, 63, 63)], [(0, 26, 18, 50)], [(36, 22, 63, 37), (0, 46, 30, 63)],
         [(40, 42, 63, 63), (0, 52, 14, 63)]]
# horns in frame-0 coordinates (body spans per row); near = left side of the cap, far = right
NEAR = {10: (31, 32), 11: (30, 32), 12: (29, 31), 13: (29, 31), 14: (29, 32), 15: (29, 32), 16: (30, 33),
        17: (30, 34), 18: (31, 34), 19: (32, 34)}
FAR = {9: (47, 48), 10: (47, 49), 11: (48, 49), 12: (48, 50), 13: (48, 50), 14: (47, 49), 15: (46, 49), 16: (46, 48)}
SH = {'Y': 'g', 'h': 'e', 'c': 'D'}
def hair(y, x):
    b = 0 if y < 42 else 1 if y < 51 else 2
    if y in (41, 50) and (x + y) % 2 == 0: b += 1
    if y in (42, 51) and (x + y) % 2 == 1: b -= 1
    return 'Yhc'[max(0, min(2, b))]

def frame(f):
    dx, dy = OFF[f]
    R = RF[f]
    m = Image.new('L', (64, 64), 0)
    ImageDraw.Draw(m).polygon(HAIRPOLY[f], fill=1, outline=1)
    inside = lambda x, y: m.getpixel((x, y)) == 1
    ontop = lambda x, y: any(a <= x <= c and b <= y <= d for a, b, c, d in ONTOP[f])
    g = [['.'] * 64 for _ in range(64)]
    caprow = 28 + dy                                   # below this: body
    for y in range(64):
        for x in range(64):
            c = R[y][x]
            if c == '.': continue
            wrist = any(a <= x <= cc and b <= y <= d for a, b, cc, d in WRIST[f])
            if wrist and c in '8d2': o = 'O'
            elif wrist and c == '1': o = 'o'
            elif c in '1f0': o = 'K'
            elif c == '7': o = 'S'
            elif c == '6': o = 'O'
            elif y < caprow and c in '349': o = {'4': 'P', '3': 'p', '9': 'q'}[c]
            elif c in '29' and y < 42 + dy: o = 'Y' if c == '2' else 'g'   # Red's hair -> blonde
            elif c == 'c': o = 'W' if y <= caprow + 1 and x >= 44 + dx else 'S'
            elif c == 'a': o = 'w' if y <= caprow + 1 else 'T'
            elif c == '8' and f <= 2 and x >= 42 + dx and y >= 42 + dy: o = 'O'   # bare upper arm, not Red's shirt
            elif c == '8': o = 'K' if y <= caprow + 1 else 'T'
            elif c == 'd': o = 'S'
            elif c in 'b4': o = 'T'
            elif c in '2359e': o = 'T'
            else: o = '?'
            g[y][x] = o
    # selective outline on the cap where it meets the background
    for y in range(64):
        for x in range(64):
            if g[y][x] in 'Pp' and any(not (0 <= x + i < 64 and 0 <= y + j < 64) or g[y + j][x + i] == '.' for i, j in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                g[y][x] = 'q'
    # Red's backpack strap / shirt next to the skin becomes outline; skin outline brown
    skin = [[g[y][x] in 'SO' for x in range(64)] for y in range(64)]
    for y in range(64):
        for x in range(64):
            if g[y][x] == 'K' and y > caprow:
                nb = [g[y + j][x + i] for i, j in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= x + i < 64 and 0 <= y + j < 64]
                if any(n in 'SO' for n in nb) and not any(n in 'TV' for n in nb): g[y][x] = 'o'
    # hair mass over her back (throwing arm stays in front)
    for y in range(64):
        for x in range(64):
            if inside(x, y) and not (ontop(x, y) and g[y][x] in 'SOo'):
                c = hair(y - dy, x - dx)
                yr = y - dy
                if (x - dx + int(round(1.8 * math.sin(yr / 3.2 + (x - dx) / 7.0)))) % 5 == 0 and yr > 31: c = SH[c]
                g[y][x] = c
    edge = [(x, y) for y in range(63) for x in range(64)
            if g[y][x] in 'YghecD' and inside(x, y) and any(
                n in '.TKV' for n in [g[y + j][x + i] for i, j in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= x + i < 64 and 0 <= y + j < 64])]
    for x, y in edge: g[y][x] = 'K'
    # horns (behind the cap edge: drawn only where the cap isn't)
    for horn in (NEAR, FAR):
        cells = {}
        for y, (a, b) in horn.items():
            for x in range(a - 1, b + 2): cells[(x, y)] = 'K'
            for x in range(a, b + 1): cells[(x, y)] = 'R' if y % 2 == 0 else 'H'
        top = min(horn); a, b = horn[top]
        for x in range(a, b + 1): cells[(x, top - 1)] = 'K'
        for (x, y), c in cells.items():
            X, Y = x + dx, y + dy
            if 0 <= X < 64 and 0 <= Y < 64 and g[Y][X] not in 'Pp':
                if c == 'K' and g[Y][X] != '.': continue
                g[Y][X] = c
    return [''.join(r) for r in g]

F = [frame(f) for f in range(5)]
open(OUT + '/back3.txt', 'w').write('\n'.join('\n'.join(fr) for fr in F) + '\n')
o = Image.new('RGBA', (5 * 264 + 8, 272), (120, 176, 96, 255))
for i, fr in enumerate(F):
    b = render(fr).resize((256, 256), Image.NEAREST); o.paste(b, (8 + i * 264, 8), b)
o.save(OUT + '/back3.png')
bad = sum(r.count('?') for fr in F for r in fr); print('unmapped', bad)
