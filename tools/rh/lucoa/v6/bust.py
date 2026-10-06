# Rounded bust for frames 4 and 5 (the low-poly model's bust renders as a cone). Drawn as a shaded round shape
# in the tank-top colours, behind her arms (skin pixels and their outlines are kept).
import math
import backpal
SK = set(backpal.PAL['skin'])
import red_hands; SK |= {red_hands.MID, red_hands.LINE}
TK = backpal.PAL['tank']
# frame: (centre x, centre y, radius x, radius y, clear box x0, y0, x1, y1)
BUST = {3: (48.5, 42.0, 6.2, 6.6, 43, 34, 60, 49), 4: (48.0, 42.5, 6.0, 6.4, 43, 34, 60, 49)}
def apply(img, i):
    if i not in BUST: return
    cx, cy, rx, ry, x0, y0, x1, y1 = BUST[i]
    def sk(x, y): return 0 <= x < 64 and 0 <= y < 64 and img[y][x] in SK
    def hairish(x, y): return 0 <= x < 64 and 0 <= y < 64 and img[y][x] is not None and img[y][x] not in SK and img[y][x] not in TK and img[y][x] != backpal.O
    # 1. clear the old cone: tank colours, and outline pixels that don't belong to an arm or the hair
    for y in range(y0, y1):
        for x in range(x0, x1):
            c = img[y][x]
            nb = [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
            if c in TK or (c == backpal.O and not any(sk(*p) or hairish(*p) for p in nb)): img[y][x] = None
    # 2. the round shape, shaded: light on the upper curve, mid, shadow underneath
    inside = set()
    for y in range(y0, y1 + 2):
        for x in range(x0, x1):
            in_bust = ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1
            in_chest = ((x + 0.5 - (cx - 3.5)) / 3.2) ** 2 + ((y + 0.5 - (cy - 5)) / 4.5) ** 2 <= 1   # joins it to her chest
            if (in_bust or in_chest) and not sk(x, y) and img[y][x] != backpal.O:
                inside.add((x, y))
    for x, y in inside:
        nx, ny = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
        d = nx * 0.35 + ny * -0.85                     # light from above, a little from the right
        img[y][x] = TK[0] if d > 0.35 else TK[1] if d > -0.35 else TK[2]
    # 3. outline where the shape meets empty space
    for x, y in inside:
        if any(not (0 <= p[0] < 64 and 0 <= p[1] < 64) or img[p[1]][p[0]] is None for p in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1))):
            img[y][x] = backpal.O
    # 4. close any gap left between the shape and the torso below
    for y in range(y0, y1 + 3):
        for x in range(x0, x1):
            if img[y][x] is None and sum(img[p[1]][p[0]] is not None for p in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)) if 0 <= p[0] < 64 and 0 <= p[1] < 64) >= 3:
                img[y][x] = TK[2]

def fill_enclosed(img, colour):
    """transparent pixels not connected to the picture's edge are holes: fill them"""
    seen = set(); stack = [(x, y) for x in range(64) for y in (0, 63)] + [(x, y) for y in range(64) for x in (0, 63)]
    while stack:
        x, y = stack.pop()
        if (x, y) in seen or not (0 <= x < 64 and 0 <= y < 64) or img[y][x] is not None: continue
        seen.add((x, y)); stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    for y in range(64):
        for x in range(64):
            if img[y][x] is None and (x, y) not in seen: img[y][x] = colour
