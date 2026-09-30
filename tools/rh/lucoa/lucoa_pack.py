# Lucoa's remaining overworld sheets (surf, bike, fish, item, itembike), built from Leaf's FRLG
# frames: Leaf's head is located in every frame and replaced with Lucoa's head (same facing, from
# lucoa_frames.txt), and Leaf's body is recoloured into Lucoa's palette.
import os
from PIL import Image
import lucoa22 as L

HERE = os.path.dirname(os.path.abspath(__file__))
LEAF = os.path.join(HERE, '..', '..', '..', 'graphics', 'rh_player', 'leaf')
CH = '.123456789abcdef'


def leaf_grid(name, fw, fh):
    im = Image.open(os.path.join(LEAF, name))
    n = im.width // fw
    return [[[CH[im.getpixel((i * fw + x, y))] for x in range(fw)] for y in range(fh)] for i in range(n)]


LEAF_NORMAL = leaf_grid('normal.png', 16, 32)
LUCOA = L.load_frames()
HEAD_ROWS = range(12, 21)        # Leaf's hat rows (used to find her head)


def find_head(fr):
    """best (score, facing, dx, dy): Leaf's hat from normal frame 0/1/2 placed at (dx, dy) in fr."""
    H, W = len(fr), len(fr[0])
    best = (0, 0, 0, 0)
    for d in (0, 1, 2):
        tpl = [(y, x, LEAF_NORMAL[d][y][x]) for y in HEAD_ROWS for x in range(16) if LEAF_NORMAL[d][y][x] != '.']
        for dy in range(-12, H - 12):
            for dx in range(-4, W - 12):
                s = 0
                for y, x, c in tpl:
                    ty, tx = y + dy, x + dx
                    if 0 <= ty < H and 0 <= tx < W and fr[ty][tx] == c:
                        s += 1
                if s > best[0]:
                    best = (s, d, dx, dy)
    return best


def hair(dark, row, chin):
    d = row - chin
    if d <= 0:
        return 'g' if dark else 'Y'
    if d <= 2:
        return 'g' if dark else 'h'
    return 'h' if dark else 'c'


def convert(fr):
    H, W = len(fr), len(fr[0])
    score, d, dx, dy = find_head(fr)
    chin = 23 + dy
    out = [['.'] * W for _ in range(H)]
    # 1. recolour Leaf's body
    for y in range(H):
        for x in range(W):
            c = fr[y][x]
            if c == '.':
                continue
            if c == '1':
                o = 'K'
            elif c in '58d' and y <= chin + 3:
                o = hair(c in 'd8', y, chin)
            else:
                o = {'2': 'T', '3': 'W', '4': 'P', '5': 'S', '6': 'D' if y <= chin + 5 else 'p', '7': 's',
                     '8': 'S', '9': 'S', 'a': 'D', 'b': 'T', 'c': 'p', 'd': 'D', 'e': 'Y', 'f': 't'}[c]
            out[y][x] = o
    # 2. remove Leaf's head (her whole head template, hat to chin)
    for y in range(12, 24):
        for x in range(16):
            if LEAF_NORMAL[d][y][x] != '.':
                ty, tx = y + dy, x + dx
                if 0 <= ty < H and 0 <= tx < W:
                    out[ty][tx] = '.'
    # 3. Lucoa's head (horns to chin) in the same place
    for y in range(8, 24):
        for x in range(16):
            c = LUCOA[d][y][x]
            ty, tx = y + dy, x + dx
            if c != '.' and 0 <= ty < H and 0 <= tx < W:
                out[ty][tx] = c
    # 4. anything Leaf holds in front of her head (VS Seeker, field-move item) goes back on top:
    #    pixels inside her head area that are not part of her head and are the item's colours
    if score < 100:
        for y in range(12, 24):
            for x in range(16):
                ty, tx = y + dy, x + dx
                if 0 <= ty < H and 0 <= tx < W and LEAF_NORMAL[d][y][x] != fr[ty][tx] and fr[ty][tx] in '1236f9':
                    c = fr[ty][tx]
                    out[ty][tx] = {'1': 'K', '2': 'K', '3': 'W', '6': 'p', 'f': 't', '9': 'S'}[c]
    return out, (score, d, dx, dy)


SHEETS = [('surf.png', 16, 32), ('bike.png', 32, 32), ('fish.png', 32, 32), ('item.png', 16, 32),
          ('itembike.png', 32, 32)]


def render(g):
    im = Image.new('RGBA', (len(g[0]), len(g)))
    for y, row in enumerate(g):
        for x, ch in enumerate(row):
            c = L.PAL[ch]
            im.putpixel((x, y), c if len(c) == 4 else c + (255,))
    return im


# Item frames (field move = 0-4, VS Seeker = 5-8), hand-drawn on Lucoa's standing frame after
# Leaf's poses: Poke Ball (pink top, white bottom) or VS Seeker (white box with a light).
BALL = [".KK.", "KppK", "KWWK", ".KK."]
SEEKER = ["KKKK", "KWpK", "KWWK", "KWWK", "KKKK"]
SEEKER_ON = ["KKKK", "KDpK", "KDDK", "KWWK", "KKKK"]
ARM_UP_BODY = {                # left arm raised: its lower half is gone from her side
    25: ".KcKTTtTTtTsScK.",
    26: ".KcKTTTTTTTsScK.",
    27: "..KKKDDDDDDssK..",
}


def stamp(g, rows, y0, x0):
    for j, r in enumerate(rows):
        for i, c in enumerate(r):
            if c != '.' and 0 <= y0 + j < len(g) and 0 <= x0 + i < len(g[0]):
                g[y0 + j][x0 + i] = c


def raised_arm(g, y_top, y_bottom, x, dy=0, dx=0):
    """forearm (skin, outlined) from the held item down to her shoulder, in column x."""
    for y in range(y_top, y_bottom + 1):
        g[y + dy][x + dx] = 'S'
        g[y + dy][x + dx - 1] = 'K'
        g[y + dy][x + dx + 1] = 'K'


def item_frame(i, base, dy=0, dx=0, arm_rows=True):
    g = [row[:] for row in base]
    if i in (3, 4, 7, 8) and arm_rows:
        for y, row in ARM_UP_BODY.items():
            for x, c in enumerate(row):
                g[y + dy][x + dx] = c
    if i == 2:
        stamp(g, BALL, 23 + dy, 11 + dx)        # ball in her right hand at the chest
    elif i == 3:
        stamp(g, BALL, 19 + dy, 0 + dx)         # ball raised to her left shoulder
        raised_arm(g, 23, 24, 2, dy, dx)
    elif i == 4:
        stamp(g, BALL, 16 + dy, 0 + dx)         # ball held up high
        raised_arm(g, 20, 24, 2, dy, dx)
    elif i == 5:
        stamp(g, SEEKER, 24 + dy, 12 + dx)      # VS Seeker held low at her right hip
    elif i == 6:
        stamp(g, SEEKER, 21 + dy, 0 + dx)       # VS Seeker at her left side, chest high
    elif i == 7:
        stamp(g, SEEKER, 16 + dy, 0 + dx)       # VS Seeker raised beside her head
        raised_arm(g, 21, 24, 2, dy, dx)
    elif i == 8:
        stamp(g, SEEKER_ON, 16 + dy, 0 + dx)    # ...and lit up
        raised_arm(g, 21, 24, 2, dy, dx)
    return g


def item_sheets():
    """item.png (9x 16x32) and itembike.png (6x 32x32: plain, plain, VS Seeker low/chest/up/lit)."""
    base = [row[:] for row in LUCOA[0]]
    item = [item_frame(i, base) for i in range(9)]
    bike_frames = leaf_grid('bike.png', 32, 32)
    lucoa_bike0, (s, d, dx, dy) = convert(bike_frames[0])
    itembike = [item_frame(i, lucoa_bike0, dy, dx, arm_rows=False) if i >= 5 else [r[:] for r in lucoa_bike0]
                for i in (0, 0, 5, 6, 7, 8)]
    return {'item.png': item, 'itembike.png': itembike}


def main():
    special = item_sheets()
    for name, fw, fh in SHEETS:
        frames = leaf_grid(name, fw, fh)
        out = Image.new('RGBA', (fw * len(frames), fh))
        txt = []
        for i, fr in enumerate(frames):
            if name in special:
                g, info = special[name][i], 'from Lucoa frame + Leaf item'
            else:
                g, info = convert(fr)
            print(name, i, 'head match', info)
            out.paste(render(g), (i * fw, 0))
            txt.append('# %s frame %d\n' % (name, i) + '\n'.join(''.join(r) for r in g) + '\n')
        out.save(os.path.join(HERE, 'lucoa_' + name))
        open(os.path.join(HERE, 'lucoa_' + name.replace('.png', '_frames.txt')), 'w').write(''.join(txt))


if __name__ == '__main__':
    main()
