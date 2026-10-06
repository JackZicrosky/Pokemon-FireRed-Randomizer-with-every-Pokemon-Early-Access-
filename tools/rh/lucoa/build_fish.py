# fish.png (12 x 32x32): Lucoa's own walk-sheet body at Leaf's position in each frame, Leaf's rod.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lc
from PIL import Image
S = os.path.dirname(os.path.abspath(__file__))
N = lc.load(S + '/frames/normal.txt')
CH = '.123456789abcdef'
im = Image.open(lc.REPO + '/graphics/rh_player/leaf/fish.png')
LF = [[[CH[im.getpixel((i * 32 + x, y))] for x in range(32)] for y in range(32)] for i in range(12)]
ROD = {'1': 'K', '2': 'V', 'b': 'V', '6': 'p', 'e': 'W', 'd': 'K', '4': 'V'}
POS = [(2, 10, 1), (2, 16, 0), (2, 13, 1), (2, 16, 0), (1, 7, -4), (1, 7, 1), (1, 8, -1), (1, 8, -1),
       (0, 9, -2), (0, 9, -8), (0, 9, -6), (0, 8, -9)]
# rod pixels copied from Leaf: (x0, y0, x1, y1, shift_x, in_front) inclusive boxes. Front-facing rods
# move 2px out because Lucoa's horns make her head wider than Leaf's.
RODS = [
    [(22, 4, 31, 13, 0, False)],
    [(25, 4, 31, 12, 0, False)],
    [(3, 10, 14, 21, 0, True)],
    [(2, 20, 17, 24, 0, True)],
    [(19, 21, 22, 31, 0, False)],
    [(21, 17, 25, 30, 0, False)],
    [(20, 2, 25, 12, 0, False)],
    [(14, 3, 17, 10, 0, False)],
    [(10, 3, 11, 12, -2, True)],
    [(7, 3, 9, 13, -1, True)],
    [(8, 22, 11, 29, -1, True)],
    [(14, 22, 17, 31, 0, True)],
]
# hand-drawn arms/hands on Lucoa's frame (local 16-wide coords, ' ' = keep)
SIDE_ARMS = {           # side frames: arms forward, hands together in front of her chest
    24: " KSSKVTT",
    25: " KSOKTTT",
    26: "  KK KTTK",
    27: "    KDDDK",
}
FRONT_HOLD = {          # front frames: both hands together in front of her, holding the rod
    25: ".KccKOTVVTOKccK.",
    26: ".KcKKOSSSSOKKcK.",
    27: "..KKKDDDDDDKKK..",
}
ARMS = {
    0: SIDE_ARMS, 1: SIDE_ARMS, 2: SIDE_ARMS, 3: SIDE_ARMS,
    8: FRONT_HOLD, 9: FRONT_HOLD, 10: FRONT_HOLD, 11: FRONT_HOLD,
}


def frame(i):
    d, dx, dy = POS[i]
    g = [['.'] * 32 for _ in range(32)]

    def rod(front):
        for x0, y0, x1, y1, sx, fr in RODS[i]:
            if fr != front:
                continue
            for y in range(y0, y1 + 1):
                for x in range(x0, x1 + 1):
                    c = LF[i][y][x]
                    if c in ROD and 0 <= x + sx < 32:
                        g[y][x + sx] = ROD[c]
    rod(False)
    body = [r[:] for r in N[d]]
    for y, row in ARMS.get(i, {}).items():
        for x, c in enumerate(row):
            if c != ' ':
                body[y][x] = c
    for y in range(32):
        for x in range(16):
            c = body[y][x]
            if c != '.' and 0 <= y + dy < 32 and 0 <= x + dx < 32:
                g[y + dy][x + dx] = c
    rod(True)
    return g


F = [frame(i) for i in range(12)]
lc.save(F, S + '/frames/fish.txt', '# fish\n')
