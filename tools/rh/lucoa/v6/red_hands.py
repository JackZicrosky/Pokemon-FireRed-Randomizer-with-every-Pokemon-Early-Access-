# Red's FR/LG throwing hands (frames 2 and 3), cut at the wristband and recoloured to Lucoa's skin,
# placed on Lucoa's wrist. Red's colours: light, mid, inner line, outline, wristband.
from PIL import Image
import backpal
RED = '/home/user/Pokemon-FireRed-Randomizer-with-every-Pokemon-Early-Access-/graphics/trainers/back_pics/red.png'
SK = backpal.PAL['skin']
MID = (236, 214, 202)                 # between normal.png's two skin tones (Red's hands have three)
LINE = (170, 138, 140)                # Red's brown finger lines, in Lucoa's skin
MAP = {(255, 197, 148): SK[0], (222, 148, 115): MID, (123, 65, 65): LINE, (106, 41, 41): LINE, (0, 0, 0): backpal.O}
BAND = (57, 57, 123)
# (Red frame, crop box in that frame, wrist pixel in the crop, transform)
SRC = {1: (1, (0, 41, 14, 55), (6, 13)), 2: (2, (0, 27, 14, 38), (8, 10))}
def hand(i, tf):
    im = Image.open(RED).convert('RGBA')
    f, (x0, y0, x1, y1), wrist = SRC[i]
    c = im.crop((x0, 64 * f + y0, x1, 64 * f + y1))
    W, H = c.size
    g = {}
    for y in range(H):
        for x in range(W):
            p = c.getpixel((x, y))
            if not p[3]: continue
            rgb = p[:3]
            if rgb == BAND: rgb = (222, 148, 115)      # bare wrist where Red has his wristband
            if rgb not in MAP: continue
            g[(x, y)] = MAP[rgb]
    def T(x, y):
        if tf == 'none': return x, y
        if tf == 'flipv': return x, H - 1 - y
        if tf == 'rot_ccw': return y, W - 1 - x
        if tf == 'rot_cw': return H - 1 - y, x
        if tf == 'flipv_flipd': return y, x
    out = {T(x, y): v for (x, y), v in g.items()}
    return out, T(*wrist)
def apply(img, i, at, tf, erase):
    x0, y0, x1, y1 = erase
    for y in range(y0, y1):
        for x in range(x0, x1):
            if img[y][x] is not None and img[y][x] in (backpal.O,) + tuple(SK): img[y][x] = None
    g, (wx, wy) = hand(i, tf)
    for (x, y), v in g.items():
        X, Y = at[0] + x - wx, at[1] + y - wy
        if 0 <= X < 64 and 0 <= Y < 64: img[Y][X] = v

# pixel fixes where Red's hand meets Lucoa's arm: {frame: {(x, y): letter}}
FIX = {
    1: {(6, 46): 'O', (7, 46): 'L', (5, 47): 'O', (6, 47): 'L', (7, 47): 'L'},
    2: {(6, 17): 'O', (7, 17): 'O', (8, 17): 'O', (9, 17): 'O', (10, 17): 'm', (11, 17): 'm', (12, 17): 'm',
        (13, 17): 'O', (14, 17): 'O', (15, 17): 'O', (16, 17): '.'},
}
def fix(img, i):
    C = {'O': backpal.O, 'L': SK[0], 'M': SK[2], 'm': MID, 'n': LINE, '.': None}
    for (x, y), c in FIX.get(i, {}).items(): img[y][x] = C[c]
