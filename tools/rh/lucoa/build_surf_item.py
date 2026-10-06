# surf.png (3 x 16x32) and item.png (9 x 16x32) drawn on the new walk frames.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lc
S = os.path.dirname(os.path.abspath(__file__))
N = lc.load(S + '/frames/normal.txt')


def cp(fr):
    return [r[:] for r in fr]


def put(fr, y0, rows, x0=0):
    for j, r in enumerate(rows):
        for i, c in enumerate(r):
            if c != ' ':
                fr[y0 + j][x0 + i] = c


def clear(fr, y0, y1):
    for y in range(y0, y1 + 1):
        fr[y] = ['.'] * len(fr[y])


# ------------------------------------------------------------------ surf
surf0 = cp(N[0]); clear(surf0, 25, 31)
put(surf0, 25, [
    ".KccOTVTTVTOccK.",
    ".KcSOTTTTTTOScK.",
    "..KSODDDDDDOSK..",
    "..KTTDDKKDDTTK..",          # sitting: knees apart in her thigh-highs
    "...KKKK..KKKK...",
])
surf1 = cp(N[1]); clear(surf1, 29, 31)   # back: her hair covers her, feet hidden
surf2 = cp(N[2]); clear(surf2, 24, 31)
put(surf2, 24, [
    "...KVTThhhhhhhK.",
    "....KTTShhhhhhK.",
    "....KTTSSKhcccK.",          # hand resting on her thigh
    "..KKDDDOOKKccK..",
    ".KVTTTTTTK.KK...",          # thigh forward in her thigh-high, knee at the front
    "..KKKKKKK.......",
])
SURF = [surf0, surf1, surf2]

# ------------------------------------------------------------------ item (field move 0-4, VS Seeker 5-8)
BALL = [".KKK.", "KPPpK", "KKWKK", "KWWWK", ".KKK."]           # pink/white ball, button in the band
SEEK = ["KKKK", "KWpK", "KWWK", "KVVK", "KKKK"]                  # VS Seeker, light off
SEEK_A = ["KKKK", "KcWK", "KWWK", "KVVK", "KKKK"]                # flashing
SEEK_B = ["KKKK", "KWcK", "KcWK", "KVVK", "KKKK"]
base = cp(N[0])


def arm_up(fr, hand):
    """her right arm (image left) raised: gone from her side, 2px forearm from row `hand` down to
    the shoulder, elbow out like Leaf's."""
    put(fr, 25, ["  KTV", "  KTT", "  KKD"])                      # side arm removed
    put(fr, hand, ["KSSK"])
    for y in range(hand + 1, 23):
        put(fr, y, ["KSOK"])
    put(fr, 23, [" KSOK"]); put(fr, 24, [" KKSOK"[:5]])


item = [cp(base) for _ in range(9)]
# 0-1: hand goes to her shorts pocket (image right hand moves in), 1 = deeper
put(item[0], 26, [".KcSOTTTTTKSK...", "..KSODDDDDSOK..."])
put(item[1], 26, [".KcSOTTTTTTKK...", "..KSODDDDSOKK..."])
# 2: Poke Ball in her hand in front of her shoulder
put(item[2], 22, BALL, 11); put(item[2], 27, ["KSSK"], 11)
# 3: ball raised to shoulder height, 4: held up high
arm_up(item[3], 21); put(item[3], 16, BALL, 0)
arm_up(item[4], 18); put(item[4], 13, BALL, 0)
# 5: VS Seeker taken out (low, at her hip), 6: chest high, 7/8: raised and flashing
put(item[5], 24, SEEK, 12)
arm_up(item[6], 21); put(item[6], 16, SEEK, 0)
arm_up(item[7], 19); put(item[7], 14, SEEK_A, 0)
arm_up(item[8], 19); put(item[8], 14, SEEK_B, 0)

lc.save(SURF, S + '/frames/surf.txt', '# surf\n')
lc.save(item, S + '/frames/item.txt', '# item\n')
