# bike.png (9 x 32x32) and itembike.png (6 x 32x32): Lucoa riding Leaf's FRLG bicycle, recoloured pink.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lc
from PIL import Image
S = os.path.dirname(os.path.abspath(__file__))
N = lc.load(S + '/frames/normal.txt')
CH = '.123456789abcdef'
im = Image.open(lc.REPO + '/graphics/rh_player/leaf/bike.png')
LB = [[[CH[im.getpixel((i * 32 + x, y))] for x in range(32)] for y in range(32)] for i in range(9)]
# facing (0 front, 1 back, 2 side), dx, dy of Leaf's head in each bike frame (found by auto_pack.py)
HEAD = [(0, 8, -1), (1, 8, -2), (2, 7, -2), (0, 7, -1), (0, 9, -1), (1, 7, -2), (1, 9, -2), (2, 6, -2), (2, 8, -2)]
# pink frame, black tyres, white rims, grey spokes/hubs; Leaf's bag becomes Lucoa's shorts + thigh-high
BIKE = {'1': 'K', '2': 'K', '3': 'V', '4': 'W', '5': 'V', '6': 'p', '7': 'O', '8': 'S', '9': 'S', 'a': 'T',
        'b': 'D', 'c': 'p', 'd': 'K', 'e': 'D', 'f': 'V'}
BIKE_FROM = {0: 26, 1: 27, 2: 25}

RIDER = {
    0: [  # rows after the chin: shoulders, arms down to the grips, hands on the grips
        "KhhcoSTOOTSochhK",
        ".KcKSOTVVTOSKcK.",
        ".KKSSKKppKKSSKK.",
    ],
    2: [  # local x 0..15 starts at dx; grey handlebar post at local 2-4, hand on the grip
        "..KVKSTThhhhhhhK",
        "..KSSSTThhhhhccK",
        "..KVKKTDDccccKK.",
    ],
}


def frame(i):
    d, dx, dy = HEAD[i]
    g = [['.'] * 32 for _ in range(32)]
    y0 = BIKE_FROM[d]
    for y in range(y0, 32):
        for x in range(32):
            c = LB[i][y][x]
            if c != '.':
                g[y][x] = 'P' if (d == 0 and y in (26, 27) and c == '4') else BIKE[c]
    last = 28 if d == 1 else 23            # back: her hair hangs down over the seat
    for y in range(8, last + 1):
        for x in range(16):
            c = N[d][y][x]
            if c != '.' and 0 <= y + dy < 32:
                g[y + dy][x + dx] = c
    if d in RIDER:
        for j, row in enumerate(RIDER[d]):
            for x, c in enumerate(row):
                if c != '.':
                    g[24 + dy + j][x + dx] = c
    if d == 2:                              # sheen on the top of her thigh-high (Leaf's leg is solid black)
        for x in range(dx + 6, dx + 9):
            if g[25][x] in 'KT':
                g[25][x] = 'V'
    return g


B = [frame(i) for i in range(9)]
lc.save(B, S + '/frames/bike.txt', '# bike\n')


# ---------------------------------------------------------------- itembike: VS Seeker on the bike
SEEK = ["KKKK", "KWpK", "KWWK", "KVVK", "KKKK"]
SEEK_A = ["KKKK", "KcWK", "KWWK", "KVVK", "KKKK"]
SEEK_B = ["KKKK", "KWcK", "KcWK", "KVVK", "KKKK"]


def put(g, y0, x0, rows):
    for j, r in enumerate(rows):
        for i, c in enumerate(r):
            if c != ' ':
                g[y0 + j][x0 + i] = c


def cp(g):
    return [r[:] for r in g]


X, Y = 8, -1                     # frame 0 offsets: local (x, y) -> global (x + 8, y - 1)


def left_arm_up(g, hand):
    """her right arm (image left) off the grip and raised; forearm from local row `hand` to the shoulder"""
    put(g, 24 + Y, X, [" KcKKT", " KKKKK"])
    put(g, hand + Y, X, ["KSSK"])
    for y in range(hand + 1, 24):
        put(g, y + Y, X, ["KSOK"])


ib = [cp(B[0]) for _ in range(6)]
put(ib[1], 24 + Y, X, ["          KSKcK", "          KKKKK"])   # 1: left hand leaves the grip
put(ib[2], 21 + Y, X + 11, SEEK); put(ib[2], 24 + Y, X, ["          KSKcK", "          KKKKK"])
left_arm_up(ib[3], 22); put(ib[3], 17 + Y, X, SEEK)
left_arm_up(ib[4], 19); put(ib[4], 14 + Y, X, SEEK_A)
left_arm_up(ib[5], 19); put(ib[5], 14 + Y, X, SEEK_B)
lc.save(ib, S + '/frames/itembike.txt', '# itembike\n')
