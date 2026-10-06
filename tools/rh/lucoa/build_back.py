# Lucoa battle back pic (5 x 64x64 throw frames). Leaf's FRLG throw poses for the body/arm; her hat
# is removed and Lucoa's head (cap, horns, hair, cheek) is drawn on; hair/top recoloured to Lucoa's.
import os, sys, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bpal import render, save_indexed
from PIL import Image
S = os.path.dirname(os.path.abspath(__file__))
CH = '0123456789abcdef'
im = Image.open(os.path.join(S, '..', '..', '..', 'graphics', 'rh_player', 'leaf', 'back.png'))
LF = [[[('.' if im.getpixel((x, f * 64 + y)) == 0 else CH[im.getpixel((x, f * 64 + y))]) for x in range(64)]
       for y in range(64)] for f in range(5)]
OFF = [(0, 0), (-3, 4), (5, 1), (-3, -1), (-1, 3)]      # Leaf's hat position relative to frame 0
HAT = {'2', '4', '5', '8', 'a', 'e'}
hatmask = [(x, y) for y in range(15, 31) for x in range(64) if LF[0][y][x] in HAT]

def _head():
    """Lucoa's head from behind (frame-0 coordinates): ringed horns, round cap with the visor
    pointing right (toward the foe), hair spilling out under the cap, a sliver of cheek."""
    H = {}

    def put(y, x, s):
        H.setdefault(y, []).append((x, s))

    def span(y, x0, x1, c):
        put(y, x0, c * (x1 - x0 + 1))
    # near horn (left), far horn (right): body spans per row, rings on even rows
    NEAR = {7: (17, 18), 8: (16, 18), 9: (16, 18), 10: (16, 19), 11: (16, 19), 12: (16, 20), 13: (17, 21),
            14: (17, 22), 15: (18, 23), 16: (19, 24), 17: (21, 25)}
    FAR = {7: (41, 42), 8: (41, 43), 9: (41, 43), 10: (40, 43), 11: (40, 42), 12: (39, 42), 13: (38, 41),
           14: (37, 40), 15: (36, 39)}
    for horn in (NEAR, FAR):
        for y, (a, b) in horn.items():
            span(y, a - 1, b + 1, 'K')
            span(y, a, b, 'o' if y % 2 == 0 else 'O')
        y0 = min(horn); a, b = horn[y0]
        span(y0 - 1, a, b, 'K')
    # hair under the cap's back edge
    for y, (a, b) in {18: (21, 22), 19: (20, 22), 20: (19, 22), 21: (18, 22), 22: (18, 22), 23: (17, 22),
                      24: (17, 22), 25: (17, 22), 26: (16, 22), 27: (16, 23), 28: (16, 24), 29: (15, 25)}.items():
        span(y, a, b, 'Y'); put(y, a - 1, 'K')
        if b - a > 2: put(y, a + 1, 'g')
    # cap dome: (left edge, right edge) per row
    DOME = {16: (26, 37), 17: (25, 38), 18: (24, 39), 19: (23, 40), 20: (23, 41), 21: (22, 41), 22: (22, 41),
            23: (22, 41), 24: (22, 41), 25: (22, 42), 26: (22, 45), 27: (23, 47), 28: (24, 47)}
    span(15, 28, 35, 'K')
    for y, (a, b) in DOME.items():
        span(y, a, b, 'K')
        span(y, a + 1, b - 1, 'P')
        span(y, a + 1, min(a + 3, b - 1), 'p')              # shaded back-left of the dome
    span(28, 25, 39, 'p')                                    # lower band
    span(27, 41, 46, 'P'); span(28, 40, 46, 'W')             # visor, white edge
    span(29, 25, 47, 'K')
    # cheek under the visor, hair lock in front of the ear
    put(30, 15, "KYgYYgYYYYYYYYYYgYYYYYYgKoSSSSK")
    put(31, 14, "KYYgYYgYYYYgYYYYgYYYYYYgYoSSSOK")
    put(32, 14, "KYgYYgYYYgYYYYgYYYYYYgYYYoSSSOK")
    put(33, 14, "KYgYYgYYYgYYYYgYYYYYYgYYYoSSOK")
    put(34, 13, "KYYgYYgYYYgYYYYgYYYYYgYYYYoSSOK")
    put(35, 13, "KYgYYgYYYgYYYYgYYYYYgYYYYYoSOK")
    put(36, 13, "KYgYYgYYgYYYYgYYYYYgYYYYYYoOK")
    return H


HEAD = _head()
SH = {'Y': 'g', 'h': 'e', 'c': 'D'}


def hair(y, light, shade, x):
    b = 0 if y < 42 else 1 if y < 50 else 2
    if y in (41, 49) and (x + y) % 2 == 0: b += 1
    if y in (42, 50) and (x + y) % 2 == 1: b -= 1
    b = max(0, min(2, b))
    base = 'Yhc'[b]
    return SH[base] if shade else base


# Lucoa's hair mass per frame (her hair is far bigger than Leaf's): polygons in frame coordinates
HAIRPOLY = [
    [(16, 29), (37, 29), (38, 35), (38, 39), (37, 43), (36, 47), (38, 52), (37, 56), (39, 60), (39, 63),
     (10, 63), (11, 60), (9, 56), (10, 52), (8, 48), (9, 44), (9, 40), (11, 36), (13, 32)],
    [(13, 33), (34, 33), (35, 39), (35, 43), (34, 47), (33, 51), (35, 56), (34, 60), (36, 63), (7, 63),
     (8, 60), (6, 56), (7, 52), (5, 48), (6, 44), (6, 40), (8, 37), (10, 35)],
    [(21, 30), (42, 30), (43, 36), (43, 40), (42, 44), (41, 48), (43, 53), (42, 57), (44, 61), (44, 63),
     (15, 63), (16, 60), (14, 56), (15, 52), (13, 48), (14, 44), (14, 40), (16, 36), (18, 33)],
    [(14, 28), (36, 28), (37, 34), (35, 40), (31, 44), (26, 47), (19, 48), (13, 47), (8, 47), (3, 45),
     (0, 42), (0, 38), (2, 34), (7, 31)],
    [(16, 31), (38, 31), (39, 37), (36, 43), (30, 47), (22, 50), (14, 49), (8, 48), (3, 46), (0, 43),
     (0, 39), (3, 35), (9, 33)],
]
# Leaf's neck/chin under the cheek is covered by Lucoa's hair (frame-0 coordinates)
NECK = (32, 37, 45, 43)


# arms that are in front of her hair (the throwing arm); anything else skin-coloured inside the hair
# mass (Leaf's hanging arm, her neck) is covered by Lucoa's hair
ONTOP = [[], [(0, 38, 15, 57), (37, 48, 63, 63)], [(0, 26, 15, 48)], [(36, 22, 63, 37), (0, 46, 40, 63)],
         [(33, 44, 63, 63), (0, 46, 22, 63)]]
WRIST = [[], [(0, 40, 16, 56)], [(8, 33, 15, 39)], [(43, 26, 53, 33)], [(35, 45, 44, 53)]]


def nbrs(G, x, y, eight=False):
    d = [(1, 0), (-1, 0), (0, 1), (0, -1)] + ([(1, 1), (1, -1), (-1, 1), (-1, -1)] if eight else [])
    return [G[y + j][x + i] for i, j in d if 0 <= x + i < 64 and 0 <= y + j < 64]


def frame(f):
    from PIL import ImageDraw
    dx, dy = OFF[f]
    L = [r[:] for r in LF[f]]
    for x, y in hatmask:
        X, Y = x + dx, y + dy
        if 0 <= X < 64 and 0 <= Y < 64 and L[Y][X] in HAT:
            L[Y][X] = '.'
    for (x0, y0, x1, y1) in WRIST[f]:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if L[y][x] == '8':
                    L[y][x] = '7'
    m = Image.new('L', (64, 64), 0)
    ImageDraw.Draw(m).polygon(HAIRPOLY[f], fill=1, outline=1)
    inside = lambda x, y: m.getpixel((x, y)) == 1
    ontop = lambda x, y: any(a <= x <= c and b <= y <= d for a, b, c, d in ONTOP[f])
    g = [['.'] * 64 for _ in range(64)]
    # 1. Leaf's body: tank, shorts, skin; her hair-coloured pixels next to skin are arm outlines
    for y in range(64):
        for x in range(64):
            c = L[y][x]
            if c == '.': continue
            X0, Y0, X1, Y1 = NECK
            if X0 + dx <= x <= X1 + dx and Y0 + dy <= y <= Y1 + dy and c in '6712':
                L[y][x] = '.'
                continue
            if c in '193':
                if any(n in '67' for n in nbrs(L, x, y, True)):
                    g[y][x] = 'o'
                elif not inside(x, y):
                    g[y][x] = 'K' if any(n in '6728cbd' for n in nbrs(L, x, y)) else '.'
            elif c == '6': g[y][x] = 'S'
            elif c == '7': g[y][x] = 'O'
            elif c == '8':
                g[y][x] = 'O' if any(n in '67' for n in nbrs(L, x, y)) and not any(n in 'cbd' for n in nbrs(L, x, y)) else 'T'
            elif c in 'cbd': g[y][x] = 'T'
            elif c in 'ae': g[y][x] = 'D'
            elif c in '45': g[y][x] = 'W'
            else: g[y][x] = 'o' if any(n in '67' for n in nbrs(L, x, y)) else 'K'
    skin = [[g[y][x] in 'SOo' and L[y][x] in '6728193' and any(n in '67' for n in nbrs(L, x, y, True)) or g[y][x] in 'SO'
             for x in range(64)] for y in range(64)]
    # 2. hair mass (behind the arms), gradient + wavy lock lines, outlined
    for y in range(64):
        for x in range(64):
            if inside(x, y) and not (skin[y][x] and g[y][x] in 'SOo' and ontop(x, y)):
                c = hair(y - dy, False, False, x)
                yr = y - dy
                if (x - dx + int(round(1.8 * math.sin(yr / 3.2 + (x - dx) / 7.0)))) % 5 == 0 and yr > 31:
                    c = SH[c]
                g[y][x] = c
    edge = [(x, y) for y in range(63) for x in range(64)
            if g[y][x] in 'YghecD' and inside(x, y) and (any(n in '.TK' for n in nbrs(g, x, y)) or x in (0, 63))]
    for x, y in edge:
        g[y][x] = 'K'
    # 3. Lucoa's head
    for y, runs in HEAD.items():
        for x0, s in runs:
            for i, c in enumerate(s):
                X, Y = x0 + i + dx, y + dy
                if c != '.' and 0 <= X < 64 and 0 <= Y < 64:
                    g[Y][X] = '.' if c == '_' else c
    return [''.join(r) for r in g]


F = [frame(f) for f in range(5)]
open(S + '/frames/back.txt', 'w').write('\n'.join('\n'.join(fr) for fr in F) + '\n')
o = Image.new('RGBA', (5 * (64 * 4 + 8) + 8, 64 * 4 + 16), (120, 176, 96, 255))
for i, fr in enumerate(F):
    b = render(fr).resize((256, 256), Image.NEAREST); o.paste(b, (8 + i * 264, 8), b)
o.save(S + '/preview/back_zoom.png')
