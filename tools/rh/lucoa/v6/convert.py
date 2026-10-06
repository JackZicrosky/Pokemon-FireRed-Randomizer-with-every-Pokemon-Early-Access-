# Turn the 512px renders (flat colour + lit) into 64x64 pixel-art frames.
import colorsys, sys
sys.path.insert(0, 'm3d')
from PIL import Image
from collections import Counter
import backpal
N = 64; B = 8
import os
FIST_MODE = os.environ.get('FIST', 'auto')
def cls(p):
    r, g, b = p[:3]
    h, l, s = colorsys.rgb_to_hls(r / 255, g / 255, b / 255)
    if l > 0.93 and s < 0.5: return 'white'
    if r > 200 and g > 200 and b > 180 and r >= g >= b and r - b < 60: return 'skin'
    if r > 220 and r - g > 50 and b > 140: return 'cap'
    if r > 180 and r - g > 30 and b < 150 and g > 120: return 'horn'
    if l < 0.45 and s < 0.25: return 'tank'
    if b > r + 35 and g < b - 15 and r < 150: return 'shorts'
    return 'hair'
def tone_rgb(c, t, k):
    r, g, b = [v / 255 for v in c[:3]]
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    if t == 2:   l = min(0.97, l + 0.06)
    if t == 0:   l = l * 0.74; h = (h + (0.03 if k != 'hair' else -0.04)) % 1; s = min(1, s * 1.1)
    if t == -1:  l = l * 0.30; s = min(1, s * 1.15); h = (h + 0.03) % 1
    r, g, b = colorsys.hls_to_rgb(h, l, s)
    return (round(r * 255), round(g * 255), round(b * 255))
def frame(flat, lit, lo=0.62, hi=0.86):
    f = flat.load(); L = lit.load()
    K = [[None] * N for _ in range(N)]; C = [[None] * N for _ in range(N)]; T = [[1] * N for _ in range(N)]
    for Y in range(N):
        for X in range(N):
            ps = []; ls = []
            for y in range(Y * B, Y * B + B):
                for x in range(X * B, X * B + B):
                    p = f[x, y]
                    if p[3] > 128: ps.append(p); ls.append(L[x, y][0] / 255)
            if len(ps) < B * B * 0.45: continue
            kc = Counter(cls(p) for p in ps); k = kc.most_common(1)[0][0]
            sel = [p for p in ps if cls(p) == k]
            col = tuple(sum(p[i] for p in sel) // len(sel) for i in range(3))
            lum = sum(ls) / len(ls)
            K[Y][X] = k; C[Y][X] = col
            T[Y][X] = 2 if lum > hi else 1 if lum > lo else 0
    return K, C, T
def smooth(K, C, T):
    # horns: widen by one pixel toward the outside so the rings read
    hs = [(x, y) for y in range(N) for x in range(N) if K[y][x] == 'horn']
    for x, y in hs:
        cx = sum(p[0] for p in hs) / max(1, len(hs))
        for dx in ((-1,) if x < cx else (1,)):
            if 0 <= x + dx < N and K[y][x + dx] is None: K[y][x + dx] = 'horn'; C[y][x + dx] = C[y][x]; T[y][x + dx] = 1
    # tone: majority of the 3x3 same-material neighbourhood (removes speckle)
    T2 = [r[:] for r in T]
    for y in range(N):
        for x in range(N):
            if not K[y][x]: continue
            c = Counter(T[y + dy][x + dx] for dy in (-1, 0, 1) for dx in (-1, 0, 1)
                        if 0 <= x + dx < N and 0 <= y + dy < N and K[y + dy][x + dx] == K[y][x])
            T2[y][x] = c.most_common(1)[0][0]
    return K, C, T2
def hair_detail(K, C, T):
    import math
    # wavy lobes on the hair's left (back) edge
    for y in range(N):
        xs = [x for x in range(N) if K[y][x] == 'hair']
        if len(xs) < 6: continue
        l = min(xs); w = math.sin(y / 2.2)
        if w > 0.55 and l > 0 and K[y][l - 1] is None and y > 0 and K[y - 1][l] == 'hair':
            K[y][l - 1] = 'hair'; C[y][l - 1] = C[y][l]; T[y][l - 1] = T[y][l]
        elif w < -0.75 and K[y][l + 1] == 'hair':
            K[y][l] = None
    # curved clump lines inside the hair, placed relative to each row's hair span (so they move with her)
    lines = set()
    for y in range(N):
        xs = [x for x in range(N) if K[y][x] == 'hair']
        if len(xs) < 8: continue
        l, r = min(xs), max(xs); w = r - l
        for k, (f, y0, y1, ph) in enumerate(((0.22, 0, 99, 0.0), (0.45, 6, 99, 1.9), (0.68, 3, 99, 3.7))):
            top = min(yy for yy in range(N) if any(K[yy][x] == 'hair' for x in range(N)))
            if not (top + 5 + y0 <= y <= top + 5 + y0 + 22): continue
            x = round(l + f * w + 1.3 * math.sin((y - top) / 3.1 + ph))
            if K[y][x] == 'hair' and ((y - top + k * 3) % 11) < 8: lines.add((x, y))
    # sheen: a few short light streaks across the upper hair
    for x, y in lines:
        if y - min(yy for xx, yy in lines) < 7 and T[y][x] >= 1: T[y][x] = 2
    return K, C, T
def snapback(K, out):
    # the cap's snapback opening at the back of her head: a little arch with hair showing through
    cs = [(x, y) for y in range(N) for x in range(N) if K[y][x] == 'cap']
    if not cs: return
    l = min(x for x, y in cs); r = max(x for x, y in cs); cx = (l + r) / 2; hw = (r - l) / 2
    bx = round(cx - 0.57 * hw)
    col = [y for x, y in cs if x == bx]
    if not col: print('snapback: no column', bx, l, r); return
    print('snapback at', bx, max(col))
    b = max(col)
    O = backpal.O
    for dx in range(-2, 2):
        for dy in range(0, 3):
            x, y = bx + dx, b - dy
            if K[y][x] != 'cap': continue
            edge = dx in (-2, 1) or dy == 2
            out[y][x] = O if edge else backpal.PAL['hair0'][1 if dy == 0 else 2]
def finish(K, C, T):
    K, C, T = smooth(K, C, T)
    K, C, T = hair_detail(K, C, T)
    out = [[None] * N for _ in range(N)]
    def k(x, y): return K[y][x] if 0 <= x < N and 0 <= y < N else None
    for Y in range(N):
        for X in range(N):
            if not K[Y][X]: continue
            kk = K[Y][X]; t = T[Y][X]
            if kk == 'horn': t = 1 if (Y // 2) % 2 == 0 else 0      # rings
            nb = [k(X + 1, Y), k(X - 1, Y), k(X, Y + 1), k(X, Y - 1)]
            if any(n is None for n in nb): t = -1                       # silhouette outline
            elif kk in ('skin',) and any(n in ('hair', 'tank') for n in nb): t = -1
            elif kk in ('tank', 'shorts') and any(n in ('hair',) for n in nb): t = -1
            elif kk == 'cap' and any(n in ('hair', 'horn') for n in nb): t = -1
            elif kk == 'horn' and any(n == 'cap' for n in nb): t = -1
            key = backpal.hair_band(C[Y][X], X, Y) if kk == 'hair' else kk
            out[Y][X] = backpal.PAL[key][{2: 0, 1: 1, 0: 2, -1: 3}[t]]
    snapback(K, out)
    return out
def to_img(out, bg=(0, 0, 0, 0)):
    im = Image.new('RGBA', (N, N), bg)
    for y in range(N):
        for x in range(N):
            if out[y][x]: im.putpixel((x, y), out[y][x] + (255,))
    return im
if __name__ == '__main__':
    import json, fist
    hands = json.load(open('m3d/out/hands.json'))
    frames = []
    for i in range(5):
        K, C, T = frame(Image.open(f'm3d/out/flat_{i}.png').convert('RGBA'), Image.open(f'm3d/out/lit_{i}.png').convert('RGBA'))
        out = finish(K, C, T)
        if i in (1, 2) and FIST_MODE == 'auto': fist.place(out, K, hands[str(i)]['wrist'], hands[str(i)]['knuckle'])
        if FIST_MODE == 'red':
            import red_hands
            cfg = json.loads(os.environ.get('REDCFG', '{}'))
            if str(i) in cfg: red_hands.apply(out, i, *cfg[str(i)]); red_hands.fix(out, i)
        if FIST_MODE == 'manual':
            import hands_manual; hands_manual.apply(out, i)
        frames.append(to_img(out))
    sheet = Image.new('RGBA', (64, 320), (0, 0, 0, 0))
    for i, fr in enumerate(frames): sheet.alpha_composite(fr, (0, 64 * i))
    # one shared palette for all frames
    q = sheet; q.save('m3d/back_sheet.png')
    z = 6; strip = Image.new('RGBA', (5 * (64 * z + 8), 64 * z), (40, 40, 40, 255))
    for i in range(5):
        t = Image.new('RGBA', (64, 64), (120, 176, 96, 255)); t.alpha_composite(q.crop((0, 64 * i, 64, 64 * i + 64)))
        strip.paste(t.resize((64 * z, 64 * z), Image.NEAREST), (i * (64 * z + 8), 0))
    strip.save('m3d/back_strip.png')
    print(len(set(c for c in q.getdata() if c[3])))
