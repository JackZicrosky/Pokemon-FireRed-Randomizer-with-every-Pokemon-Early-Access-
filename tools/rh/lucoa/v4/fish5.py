# Lucoa front fishing frames (fish.png 8-11), hand-drawn on Leaf's timing/positions: crouched with the rod
# upright, wind-up over her shoulder, cast down to the side, rod held out in front.
import sys
sys.path.insert(0, sys.argv[1])
from owpal import *
S = sys.argv[1]
U = load(S + '/owner_normal2.txt')
HEAD = [r[:] for r in U[0][10:25]]
def blank(): return [['.'] * 32 for _ in range(32)]
def put(g, y0, x0, rows):
    for j, r in enumerate(rows):
        for i, c in enumerate(r):
            if c != '.' and 0 <= y0 + j < 32 and 0 <= x0 + i < 32: g[y0 + j][x0 + i] = c
def line(a, b):
    (x0, y0), (x1, y1) = a, b
    n = max(abs(x1 - x0), abs(y1 - y0))
    return [(round(x0 + (x1 - x0) * t / n), round(y0 + (y1 - y0) * t / n)) for t in range(n + 1)]
def rod(g, a, b, behind=False):
    pts = line(a, b)
    for x, y in pts:                       # dark outline on both sides, solid light shaft
        for X in (x - 1, x + 1):
            if 0 <= X < 32 and g[y][X] == '.': g[y][X] = 'K'
    for x, y in pts:
        if behind and g[y][x] not in '.K': continue
        g[y][x] = 'V'
    x, y = pts[0]
    g[y][x] = 'p'
    if y > 0 and g[y - 1][x] == '.': g[y - 1][x] = 'K'
F = []
# 8: crouched, rod upright at her side, both hands on it
g = blank(); ox, t0 = 9, 8
rod(g, (7, 1), (7, 25))
put(g, t0, ox, HEAD)
put(g, t0 + 15, ox - 3, [
    "KSSKKccOTVTTVTOccK",
    "KSOSKcSOTTTTTTOScK",
    ".KKK.KKDDDDDDDDKK.",
    ".....KTTKKKKKKTTK.",
    ".....KpPK....KPpK.",
    "......KK......KK..",
])
F.append(g)
# 9: wind-up, rod swung back over her shoulder, hands raised beside her head
g = blank(); ox, t0 = 9, 2
rod(g, (4, 0), (10, 15), behind=True)
put(g, t0, ox, HEAD)
put(g, 13, 7, ["KSSK", "KSOSK", ".KKK"])
put(g, t0 + 15, ox, [
    ".KccKTVTTVTOccK.",
    ".KcKTTTTTTTOScK.",
    "..KKDDDDDDDOSK..",
    "..KTTDDKKDDTTK..",
    "..KpPK....KPpK..",
    "...KK......KK...",
])
F.append(g)
# 10: the cast, rod swung down to the side, hands together at her chest, crouched
g = blank(); ox, t0 = 9, 4
put(g, t0, ox, HEAD)
put(g, t0 + 15, ox, [
    ".KccKSSOOSSKccK.",
    ".KcKKOSSSSOKKcK.",
    "..KKDDDDDDDDKK..",
    "..KTTKKKKKKTTK..",
    "..KpPK....KPpK..",
    "...KK......KK...",
])
rod(g, (4, 30), (13, 20))
put(g, 19, 12, ["KSSK"])
F.append(g)
# 11: rod held out in front of her (pointing at the water below), hands together
g = blank(); ox, t0 = 8, 1
put(g, t0, ox, HEAD)
put(g, t0 + 15, ox, [
    ".KccKSSOOSSKccK.",
    ".KcKKOSSSSOKKcK.",
    "..KKDDDDDDDDKK..",
    "..KTTKKKKKKTTK..",
    "..KpPK....KPpK..",
    "...KK......KK...",
])
rod(g, (15, 31), (15, 18), behind=True)
put(g, 16, 14, ["KSSK", ".KK."])
F.append(g)
save(F, S + '/fish_front5.txt', '# fish 8-11\n')
strip([F], S + '/fz.png', 9)
