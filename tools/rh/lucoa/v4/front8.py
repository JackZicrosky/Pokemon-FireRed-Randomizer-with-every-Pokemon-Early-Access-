# Lucoa front pic v4: pose after "Lucoa Pixel Art 1" (fist by her chin, other hand at her hip, legs crossed).
import sys, math
sys.path.insert(0, sys.argv[1])
from pal3 import *
OUT = sys.argv[1]
OX, OY = 14, 2
G = [['.'] * 64 for _ in range(64)]
def put(ch, spans):
    for y, rs in spans.items():
        if isinstance(rs, tuple): rs = [rs]
        for a, b in rs:
            for x in range(a, b + 1): G[y + OY][x + OX] = ch
put('H', {10: (11, 28), 11: (10, 28), 12: (10, 28), 13: (9, 28), 14: (9, 28), 15: (7, 28), 16: (6, 28), 17: (5, 27),
          18: (5, 26), 19: (5, 16), 20: (5, 15), 21: (5, 15), 22: (5, 15), 23: (5, 15), 24: (4, 16), 25: (4, 16),
          26: (4, 16), 27: (4, 15), 28: [(5, 9), (11, 15)], 29: [(5, 8), (12, 14)], 30: [(6, 7), (12, 13)]})
put('P', {3: (15, 24), 4: (14, 25), 5: (13, 26), 6: (13, 26), 7: (13, 26), 8: (13, 25)})
put('W', {9: (12, 26)})
put('U', {10: (14, 23)})
put('S', {11: (15, 25), 12: (14, 26), 13: (14, 26), 14: (14, 26), 15: (15, 26), 16: (15, 25), 17: (16, 24), 18: (17, 22)})
put('T', {20: (14, 28), 21: (14, 29), 22: (14, 29), 23: (14, 29), 24: (14, 29), 25: (15, 29), 26: (15, 28), 27: (16, 28),
          28: (17, 28)})
put('S', {19: (16, 24), 20: (17, 23), 21: (18, 22), 22: (19, 21), 23: (20, 20)})
put('D', {29: (17, 28), 30: (16, 29), 31: (15, 29), 32: (14, 30), 33: (14, 30), 34: (14, 30)})
put('C', {35: [(13, 20), (22, 31)], 36: [(13, 20), (22, 31)]})
put('S', {37: [(13, 20), (22, 30)], 38: [(14, 20), (22, 30)], 39: [(15, 20), (22, 29)]})
put('L', {40: (22, 28), 41: (22, 28), 42: (22, 27), 43: (22, 27), 44: (23, 27), 45: (23, 27), 46: (23, 27), 47: (23, 27),
          48: (23, 27), 49: (23, 26), 50: (23, 26), 51: (23, 26), 52: (23, 26), 53: (23, 26), 54: (23, 26), 55: (23, 26)})
put('M', {40: (15, 21), 41: (15, 21), 42: (16, 21), 43: (16, 21), 44: (17, 22), 45: (17, 22), 46: (18, 23), 47: (18, 23),
          48: (18, 23), 49: (17, 22), 50: (17, 21), 51: (16, 20), 52: (16, 20), 53: (15, 19), 54: (15, 19), 55: (15, 19)})

put('A', {16: (24, 28), 17: (24, 29), 18: (24, 29), 19: (25, 29), 20: (26, 30), 21: (27, 30), 22: (27, 30), 23: (27, 30),
          24: (27, 30), 25: (27, 30), 26: (27, 29)})
put('A', {33: (12, 15), 34: (11, 15), 35: (11, 14), 36: (12, 14)})
put('B', {10: (13, 26), 11: [(13, 15), (19, 20), (25, 27)], 12: [(13, 14), (26, 27)], 13: [(12, 14), (26, 27)],
          14: [(12, 14), (26, 27)], 15: [(12, 14), (26, 27)], 16: [(12, 14)], 17: (13, 15)})
L = G
open(OUT + '/front8_labels.txt', 'w').write('\n'.join(''.join(r) for r in L) + '\n')
Zo = {'.': -1, 'H': 0, 'R': 1, 'P': 2, 'W': 3, 'U': 3, 'S': 4, 'T': 5, 'D': 6, 'C': 7, 'L': 6, 'M': 8, 'Q': 9, 'E': 10, 'A': 9, 'B': 10}
def at(x, y): return L[y][x] if 0 <= x < 64 and 0 <= y < 64 else '.'
RAMP = {'S': ('S', 'S', 'O'), 'A': ('S', 'S', 'O'), 'P': ('B', 'P', 'p'), 'W': ('W', 'W', 'W'), 'U': ('U', 'U', 'U'),
        'T': ('V', 'T', 'T'), 'L': ('V', 'T', 'T'), 'M': ('V', 'T', 'T'), 'D': ('L', 'D', 'd'), 'C': ('L', 'L', 'c'),
        'Q': ('B', 'P', 'p'), 'E': ('W', 'W', 'w')}
HR = [('y', 'Y', 'g'), ('h', 'h', 'e'), ('c', 'c', 'D')]
def zone(y): y -= OY; return 0 if y < 19 else 1 if y < 25 else 2
NOLINE = {('S', 'P'), ('S', 'W'), ('S', 'U'), ('A', 'S'), ('W', 'P'), ('U', 'W'), ('C', 'D'), ('B', 'H'), ('E', 'Q'), ('U', 'P')}
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
            img[y][x] = 'o' if p in 'SA' else ('k' if p == 'R' else ('g' if p == 'B' and 'S' in q else 'K'))
            continue
        lit = not inside(x - 1, y, p) or not inside(x, y - 1, p)
        dark = not inside(x + 1, y, p) or not inside(x, y + 1, p)
        k = 0 if lit and not dark else 2 if dark and not lit else 1
        if p in 'HB':
            w = int(round(1.3 * math.sin(y / 2.4 + x / 7)))
            if p == 'H' and (x + w) % 4 == 0 and y > 14: k = 2
            img[y][x] = HR[zone(y)][k]
        elif p == 'R':
            # ringed horn: dark ring every other row, light edge on the lit (left/top) side, dark on the right
            c = 'H'
            if (y - OY) % 2 == 0: c = 'R'
            if lit and c == 'H': c = 'j'
            if dark and c == 'R': c = 'k'
            img[y][x] = c
        else:
            img[y][x] = RAMP[p][k]
def X(x): return x + OX
def Y(y): return y + OY
DET = [  # eyes: green (her right) and purple (her left), lashes; blush; open smile
    (16, 12, 'K'), (17, 12, 'K'), (16, 13, 'G'), (17, 13, 'K'), (16, 14, 'G'), (17, 14, 'G'),
    (22, 12, 'K'), (23, 12, 'K'), (22, 13, 'v'), (23, 13, 'K'), (22, 14, 'v'), (23, 14, 'v'),
    (15, 15, 'B'), (16, 15, 'B'), (23, 15, 'B'), (24, 15, 'B'),
    (19, 16, 'o'), (20, 16, 'o'), (19, 17, 'r'), (20, 17, 'o'),
    (20, 21, 'o'), (20, 22, 'o'),                                    # cleavage
    (25, 17, 'o'), (26, 16, 'o'), (27, 16, 'o'),                     # fingers of the fist
    (17, 26, 'K'), (18, 26, 'K'), (25, 26, 'K'), (26, 26, 'K'),      # under the bust
    (22, 32, 'K'), (22, 33, 'K'), (21, 34, 'K'),                     # shorts seam
    (16, 22, 'V'), (17, 21, 'V'), (18, 21, 'V'), (24, 21, 'V'), (25, 21, 'V'), (26, 22, 'V'),  # bust sheen
]
for x, y, c in DET: img[Y(y)][X(x)] = c
HORN = [            # left horn, x 7..15: a C-curve with the tip turning inward; rings, lit outer edge
    "..KKK....",
    ".KjHRK...",
    "KjHKK....",
    "KjRK.....",
    "KjHK.....",
    "KjRHK....",
    ".KjHRK...",
    ".KjRHRK..",
    "..KjHRHK.",
    "...KKHRK.",
]
for j, r in enumerate(HORN):
    for i, c in enumerate(r):
        if c == '.': continue
        for x, cc in ((7 + i, c), (39 - (7 + i), {'j': 'H', 'H': 'j'}.get(c, c) if False else c)):
            if L[Y(j)][X(x)] in 'PW': continue
            img[Y(j)][X(x)] = cc
SHOE = [".KPPPK..", "KBPPPPK.", "KBPPPWWK", "KpPPPWWK", "KWWWWWWK", ".KKKKKK."]
for j, r in enumerate(SHOE):
    for i, c in enumerate(r):
        if c == '.': continue
        img[Y(56 + j)][X(13 + i)] = c
        img[Y(56 + j)][X(21 + i)] = c
rows = [''.join(r) for r in img]
open(OUT + '/front8.txt', 'w').write('\n'.join(rows) + '\n')
zoom(rows, OUT + '/front8.png', 9)
