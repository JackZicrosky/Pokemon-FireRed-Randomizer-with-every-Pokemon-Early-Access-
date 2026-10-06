# Hand-drawn fist for the throwing hand (frames 2-3): back of the hand, knuckles toward the hand's direction.
import math
import backpal
# mask with knuckles at the top, wrist at the bottom ('#' = hand, 'k' = gap between knuckles)
FIST = [
    ".##.##.#.",
    "#########",
    "#########",
    "#########",
    "#########",
    ".#######.",
    "..#####..",
]
def place(img, K, wrist, knuckle):
    """img: 64x64 list of RGB/None; K: materials. Erase the rendered hand past the wrist, draw the fist."""
    wx, wy = wrist; dx, dy = knuckle[0] - wx, knuckle[1] - wy
    n = math.hypot(dx, dy) or 1; dx, dy = dx / n, dy / n
    # 1. erase the rendered hand (skin beyond the wrist)
    for y in range(64):
        for x in range(64):
            if K[y][x] == 'skin' or (img[y][x] == backpal.O and K[y][x] is not None and K[y][x] == 'skin'):
                pass
            vx, vy = x + 0.5 - wx, y + 0.5 - wy
            along = vx * dx + vy * dy; across = abs(-vx * dy + vy * dx)
            if along > 0.5 and along < 9 and across < 5 and K[y][x] == 'skin':
                img[y][x] = None; K[y][x] = None
    # 2. fist mask rotated so its top (knuckles) points along (dx, dy); its wrist end sits on the wrist
    h, w = len(FIST), len(FIST[0])
    cx, cy = wx + dx * (h / 2 - 0.5), wy + dy * (h / 2 - 0.5)
    ux, uy = dx, dy                     # 'up' in the mask = along the hand
    rx, ry = -uy, ux                    # 'right' in the mask
    if rx < 0: rx, ry = -rx, -ry        # keep the shading's light side to the upper left
    mask = set(); gaps = set()
    for y in range(64):
        for x in range(64):
            vx, vy = x + 0.5 - cx, y + 0.5 - cy
            mu = -(vx * ux + vy * uy) + h / 2      # row in mask (0 = knuckles)
            mr = vx * rx + vy * ry + w / 2
            r, c = int(math.floor(mu)), int(math.floor(mr))
            if 0 <= r < h and 0 <= c < w and FIST[r][c] == '#': mask.add((x, y))
    skin = backpal.PAL['skin']
    for x, y in mask:
        if not (0 <= x < 64 and 0 <= y < 64): continue
        nb = [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
        out = [p for p in nb if p not in mask]
        if out:
            # no outline where the fist meets the arm (wrist side)
            if all(0 <= p[0] < 64 and 0 <= p[1] < 64 and K[p[1]][p[0]] == 'skin' and img[p[1]][p[0]] not in (None, backpal.O) for p in out):
                img[y][x] = skin[1]
            else: img[y][x] = backpal.O
        elif (x + 1, y + 1) not in mask or (x + 1, y) not in mask or (x, y + 2) not in mask: img[y][x] = skin[2]
        else: img[y][x] = skin[0]
        K[y][x] = 'skin'
    # knuckle creases: short shade lines from the gaps
    return img
