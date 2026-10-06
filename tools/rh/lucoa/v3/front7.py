# Lucoa front pic v3: hand-placed part map -> outline + rim shading -> hand details
import sys, math
sys.path.insert(0, sys.argv[1])
from pal3 import *
OUT = sys.argv[1]
G = [['.'] * 64 for _ in range(64)]
def put(ch, spans, mirror=None):
    for y, rs in spans.items():
        if isinstance(rs, tuple): rs = [rs]
        for a, b in rs:
            for x in range(a, b + 1):
                G[y][x] = ch
                if mirror is not None: G[y][mirror - x] = ch
HAIR = {10: (21, 41), 11: (20, 42), 12: (18, 43), 13: (18, 43), 14: (17, 44), 15: (17, 44), 16: (17, 44),
        17: (17, 45), 18: (17, 45), 19: (17, 45), 20: (17, 45), 21: (17, 45), 22: (17, 46),
        23: (18, 46), 24: [(18, 23), (37, 46)], 25: [(19, 22), (37, 46)], 26: (38, 47), 27: (38, 47),
        28: (39, 47), 29: (39, 47), 30: (39, 48), 31: (40, 48), 32: (40, 48), 33: (40, 48), 34: (40, 49), 35: (41, 49),
        36: (41, 49), 37: [(41, 44), (45, 49)], 38: [(42, 44), (46, 48)], 39: [(43, 43), (47, 47)]}
put('H', HAIR)
HORN = {0: (22, 23), 1: (20, 23), 2: (19, 22), 3: (18, 22), 4: (18, 22), 5: (18, 23), 6: (19, 24), 7: (20, 25),
        8: (21, 25), 9: (22, 25)}
put('R', HORN, mirror=62)
put('P', {5: (27, 35), 6: (25, 37), 7: (24, 38), 8: (23, 39), 9: (23, 39), 10: (22, 40), 11: (22, 40)})
put('W', {12: (19, 40), 13: (20, 39)})
put('S', {22: (27, 31), 15: (25, 35), 16: (25, 35), 17: (25, 35), 18: (25, 35), 19: (25, 34), 20: (25, 33), 21: (26, 32), 22: (27, 30)})
put('N', {23: (29, 32), 24: (29, 32)})
put('S', {24: (24, 37), 25: (23, 38), 26: (30, 31)})
put('T', {24: [(25, 27), (34, 36)], 25: [(24, 28), (33, 37)], 26: [(23, 29), (32, 38)], 27: (22, 39), 28: (22, 39),
          29: (22, 39), 30: (22, 39), 31: (23, 38), 32: (24, 37), 33: (24, 37), 34: (24, 37)})
put('D', {35: (23, 38), 36: (23, 38), 37: [(23, 30), (32, 38)]})
put('C', {38: [(22, 30), (32, 39)], 39: [(22, 30), (32, 39)]})
put('S', {40: [(23, 30), (32, 39)], 41: [(23, 30), (33, 39)], 42: [(24, 30), (33, 39)]})
LL = {43: (24, 30), 44: (24, 30), 45: (24, 29), 46: (23, 29), 47: (23, 29), 48: (23, 28), 49: (23, 28), 50: (23, 28),
      51: (22, 28), 52: (22, 27), 53: (22, 27), 54: (22, 27), 55: (22, 27), 56: (22, 27)}
RL = {43: (33, 39), 44: (33, 39), 45: (34, 40), 46: (34, 40), 47: (35, 40), 48: (35, 41), 49: (36, 41), 50: (36, 41),
      51: (36, 42), 52: (37, 42), 53: (37, 42), 54: (37, 42), 55: (37, 42), 56: (37, 42)}
put('L', LL); put('L', RL)
put('Q', {57: [(21, 28), (36, 43)], 58: [(20, 28), (36, 44)], 59: [(19, 28), (36, 45)], 60: [(19, 28), (36, 45)]})
put('A', {25: (20, 23), 26: (19, 23), 27: (19, 22), 28: (18, 22), 29: (17, 21), 30: (16, 21), 31: (13, 20),
          32: (11, 19), 33: (10, 17), 34: (10, 14)})
put('A', {25: (38, 41), 26: (38, 42), 27: (39, 43), 28: (39, 43), 29: (40, 44), 30: (40, 44), 31: (40, 44),
          32: (39, 43), 33: (38, 42), 34: (36, 41), 35: (36, 40), 36: (37, 39)})
put('Z', {29: (7, 11), 30: (6, 12), 31: (6, 11), 32: (6, 10), 33: (7, 9)})
put('B', {14: (21, 39), 15: [(21, 25), (27, 30), (33, 39)], 16: [(21, 24), (28, 29), (34, 39)], 17: [(21, 24), (35, 38)],
          18: [(21, 24), (35, 38)], 19: [(21, 24), (35, 38)], 20: [(21, 24), (35, 38)], 21: [(21, 25), (34, 37)],
          22: [(22, 25), (35, 36)], 23: (22, 24), 24: (23, 24)})
L = G
open(OUT + '/front7_labels.txt', 'w').write('\n'.join(''.join(r) for r in L) + '\n')

# ---------------------------------------------------------------- paint
Zo = {'.': -1, 'H': 0, 'R': 1, 'P': 2, 'W': 3, 'S': 4, 'N': 4, 'T': 5, 'D': 6, 'C': 7, 'L': 6, 'Q': 7, 'A': 8, 'Z': 9, 'B': 10}
def at(x, y): return L[y][x] if 0 <= x < 64 and 0 <= y < 64 else '.'
def zone(y):
    return 0 if y < 27 else 1 if y < 35 else 2
RAMP = {'S': ('S', 'S', 'O'), 'A': ('S', 'S', 'O'), 'N': ('O', 'S', 'S'), 'P': ('B', 'P', 'p'), 'W': ('W', 'W', 'w'),
        'T': ('V', 'T', 'T'), 'L': ('V', 'T', 'T'), 'D': ('L', 'D', 'd'), 'C': ('L', 'L', 'c'), 'Q': ('B', 'P', 'p'),
        'Z': ('r', 'r', 'r')}
HR = [('y', 'Y', 'g'), ('h', 'h', 'e'), ('c', 'c', 'D')]
NOLINE = [('S', 'N'), ('N', 'S'), ('S', 'P'), ('S', 'W'), ('A', 'S'), ('A', 'N'), ('W', 'P'), ('C', 'D'),
          ('B', 'H')]
outline = [[False] * 64 for _ in range(64)]
for y in range(64):
    for x in range(64):
        p = L[y][x]
        if p == '.': continue
        for q in (at(x + 1, y), at(x - 1, y), at(x, y + 1), at(x, y - 1)):
            if q == '.' or (Zo[q] < Zo[p] and (p, q) not in NOLINE):
                outline[y][x] = True
def inside(x, y, p): return at(x, y) == p and not outline[y][x]
img = [['.'] * 64 for _ in range(64)]
for y in range(64):
    for x in range(64):
        p = L[y][x]
        if p == '.': continue
        if outline[y][x]:
            q = [at(x + 1, y), at(x - 1, y), at(x, y + 1), at(x, y - 1)]
            img[y][x] = 'o' if p in 'SAN' else ('g' if p == 'B' and any(c in 'SN' for c in q) else 'K')
            continue
        lit = not inside(x - 1, y, p) or not inside(x, y - 1, p)
        dark = not inside(x + 1, y, p) or not inside(x, y + 1, p)
        k = 0 if lit and not dark else 2 if dark and not lit else 1
        if p in 'HB':
            ramp = HR[zone(y)]
            w = int(round(1.4 * math.sin(y / 2.6 + x / 9)))
            if p == 'H' and (x + w) % 4 == 0 and y > 16: k = 2
            if p == 'H' and x > 36: k = max(k, 1)
            c = ramp[k]
            # dithered band change
            if y in (26, 34) and (x + y) % 2 == 0: c = HR[zone(y) + 1][1]
            img[y][x] = c
        elif p == 'R':
            img[y][x] = 'R' if y % 2 == 0 else 'H'
        else:
            img[y][x] = RAMP[p][k]
# details
DET = [(27, 17, 'K'), (26, 18, 'K'), (28, 18, 'K'),          # closed eye (far)
       (32, 17, 'K'), (33, 17, 'K'), (31, 18, 'K'), (34, 18, 'K'),   # closed eye (near)
       (26, 19, 'B'), (27, 19, 'B'), (32, 19, 'B'), (33, 19, 'B'),   # blush
       (28, 20, 'o'), (29, 21, 'o'), (30, 20, 'o'),                    # smile
       (30, 26, 'o'), (30, 27, 'K'),                                   # cleavage
       (8, 31, 'K'), (9, 31, 'K'), (10, 31, 'K'), (7, 31, 'K'), (9, 32, 'W'), (8, 32, 'W'), (7, 32, 'W'), (8, 33, 'W'),
       (31, 37, 'K'), (31, 36, 'd')]
for x, y, c in DET: img[y][x] = c
FACE = {  # hand-drawn face block, x 19-40
    14: "YYgggggggggggggggggggY",
    15: "YgyyYYgYYYgYYYgYYyyYYY",
    16: "YgyYYgSgYYgSSgYYgYyYgY",
    17: "ggyYgoSSKSSSSKKSogyYgY",
    18: "ggyYgoSKSKSSKSSKogyYgY",
    19: "YgyYgoSBBSSSSBBSogyYgY",
    20: "YgyYgoSSSSSSSSSSoYyYgY",
    21: "YgYYYgoSSSoSoSSoYYgYYg",
    22: "YYgyYYgoSSSoSSoYYgYYYg",
    23: "yYgYggggooSSSooYgYggYg",
    24: "YggYKKKKKoOOOOoKKKoYgg",
}
for y, r in FACE.items():
    assert len(r) == 22, (y, len(r))
    for i, c in enumerate(r): img[y][19 + i] = c
for x in range(19, 29): img[60][x] = 'W' if img[60][x] not in 'K' else 'K'
for x in range(36, 46): img[60][x] = 'W' if img[60][x] not in 'K' else 'K'
rows = [''.join(r) for r in img]
open(OUT + '/front7.txt', 'w').write('\n'.join(rows) + '\n')
zoom(rows, OUT + '/front7.png', 10)
