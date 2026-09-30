# Lucoa's battle pictures: front.png (64x64 trainer pic) and back.png (5x 64x64 throw), recoloured
# region by region from Leaf's FRLG pictures, plus horns on the cap. oak.png is derived from front.png
# exactly like tools/rh/gen_player_graphics.py does.
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
LEAF = os.path.join(HERE, '..', '..', '..', 'graphics', 'rh_player', 'leaf')

PAL = [(0, 0, 0),
       (48, 36, 48),     # 1 outline
       (246, 163, 170),  # 2 cap pink
       (222, 110, 136),  # 3 cap shade / shoes / ball
       (240, 238, 245),  # 4 white (cap band, bill, shoe soles)
       (253, 231, 213),  # 5 skin
       (234, 180, 158),  # 6 skin shade
       (196, 138, 118),  # 7 skin dark / horn bands
       (238, 241, 133),  # 8 hair yellow
       (196, 200, 88),   # 9 hair shade
       (179, 236, 120),  # 10 hair green
       (139, 204, 247),  # 11 hair blue
       (66, 60, 78),     # 12 top / thigh-highs
       (104, 96, 120),   # 13 top highlight
       (80, 84, 168),    # 14 jean shorts
       (230, 178, 138)]  # 15 horn
K, P, p, W, S, s, sd, Y, g, h, c, T, t, D, O = range(1, 16)


def hair(light, row, top, bottom):
    f = (row - top) / max(1, bottom - top)
    if f < 0.45:
        return Y if light else g
    if f < 0.75:
        return h if light else g
    return c if light else h


def front():
    im = Image.open(os.path.join(LEAF, 'front.png'))
    src = [[im.getpixel((x, y)) for x in range(64)] for y in range(64)]
    out = [[0] * 64 for _ in range(64)]
    for y in range(64):
        for x in range(64):
            v = src[y][x]
            if v == 0:
                continue
            if v in (2, 0xc) and not (19 <= y <= 35 and 26 <= x <= 36):
                o = K
            elif y <= 17 and 22 <= x <= 42:                       # cap
                o = {7: P, 6: p, 8: W, 3: p, 1: g, 9: K, 0xd: K, 0xb: K, 0xc: K, 4: S, 5: s}.get(v, K)
            elif y >= 58:                                          # shoes
                o = {8: p, 7: P, 6: W, 1: K}.get(v, K)
            elif 52 <= y:                                          # thigh-high socks
                o = {0xd: t, 9: T, 0xb: T, 5: s, 3: sd, 1: K, 6: t}.get(v, T)
            elif 36 <= y <= 41 and v in (8, 0xa, 1) and x >= 24:  # skirt -> jean shorts
                o = D if v == 0xa else (K if v == 1 else D)
            elif 30 <= y <= 37 and x <= 20:                        # hand + Poke Ball
                o = {8: p, 0xa: p, 7: W, 6: W, 0xb: K, 1: K, 3: sd, 4: S, 5: s, 0xc: K}.get(v, K)
            elif v in (1, 3) and 13 <= y <= 41 and (x >= 33 or y <= 17):   # long hair
                o = hair(v == 3, y, 13, 41)
            elif v == 1:
                o = K
            elif v == 0xe:                                         # bag -> gone into the shorts / top
                o = D if y >= 35 else T
            else:
                o = {3: sd, 4: S, 5: s, 9: T, 0xd: t, 0xb: T, 8: D, 0xa: D, 7: W, 6: t}.get(v, K)
            out[y][x] = o
    # horns rising from both sides of the cap
    horn = [".KK.", "KOOK", "KsdK", "KOOK", "KOOK", "KsdK", "KOOK", ".KOOK", "..KOOK"]
    mp = {'K': K, 'O': O, 's': sd, 'd': sd, '.': 0}
    for j, r in enumerate(horn):
        for i, ch in enumerate(r):
            if ch != '.':
                out[4 + j][22 + i] = mp[ch]
    for j, r in enumerate(horn):
        for i, ch in enumerate(r[::-1].rjust(6, '.')):
            if ch != '.':
                out[3 + j][36 + i] = mp[ch]
    return out


def back():
    im = Image.open(os.path.join(LEAF, 'back.png'))
    frames = []
    for f in range(5):
        src = [[im.getpixel((x, 64 * f + y)) for x in range(64)] for y in range(64)]
        hat = [(y, x) for y in range(64) for x in range(64) if src[y][x] in (4, 5)]
        top = min(y for y, x in hat)
        hx0 = min(x for y, x in hat); hx1 = max(x for y, x in hat)
        hairpx = [(y, x) for y in range(64) for x in range(64) if src[y][x] in (1, 3, 9, 0xb)]
        hb = max(y for y, x in hairpx)
        out = [[0] * 64 for _ in range(64)]
        for y in range(64):
            for x in range(64):
                v = src[y][x]
                if v == 0:
                    continue
                o = {2: K, 5: P, 4: p, 0xa: W, 6: S, 7: s, 8: T, 0xc: t, 0xd: D, 0xe: p}.get(v)
                if v in (1, 3, 9, 0xb):
                    o = hair(v in (1, 9, 0xb), y, top + 8, hb)
                out[y][x] = o
        # horns: one at each end of the cap's crown, straight up
        horn = ["KK", "KOK", "KsK", "KOK", "KsK", "KOK"]
        mp = {'K': K, 'O': O, 's': sd}
        for side, x0 in ((0, hx0 + 4), (1, hx1 - 6)):
            base = min(y for y in range(64) if out[y][x0 + 1] != 0)    # cap surface under the horn
            for j, r in enumerate(horn):
                for i, ch in enumerate(r):
                    y = base - 5 + j + 1                               # horn root sits in the cap
                    if 0 <= y < 64:
                        out[y][x0 + i] = mp[ch]
        frames.append(out)
    return frames


def save(grid_list, path, vertical=False):
    h = 64 * len(grid_list) if vertical else 64
    w = 64 if vertical else 64 * len(grid_list)
    im = Image.new('P', (w, h), 0)
    flat = [v for c in PAL for v in c] + [0] * (768 - 48)
    im.putpalette(flat)
    for n, g in enumerate(grid_list):
        for y in range(64):
            for x in range(64):
                im.putpixel((x, y + 64 * n) if vertical else (x + 64 * n, y), g[y][x])
    im.save(path)


def oak(front_path, out_path):
    fr = Image.open(front_path)
    o = Image.new('P', (64, 96), 0)
    pal = [0] * 768
    fp = fr.getpalette()
    for i in range(16):
        pal[(64 + i) * 3:(64 + i) * 3 + 3] = fp[i * 3:i * 3 + 3]
    o.putpalette(pal)
    for y in range(64):
        for x in range(64):
            v = fr.getpixel((x, y))
            o.putpixel((x, y + 32), 0 if v == 0 else 64 + v)
    o.save(out_path)


if __name__ == '__main__':
    save([front()], os.path.join(HERE, 'lucoa_front.png'))
    save(back(), os.path.join(HERE, 'lucoa_back.png'), vertical=True)
    oak(os.path.join(HERE, 'lucoa_front.png'), os.path.join(HERE, 'lucoa_oak.png'))
