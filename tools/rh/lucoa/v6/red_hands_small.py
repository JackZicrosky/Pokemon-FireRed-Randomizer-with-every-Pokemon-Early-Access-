# Red's FR/LG throwing hands (his frames 2 and 3) redrawn at ~70% size to fit Lucoa's slimmer arms,
# not mirrored. O = outline, n = Red's finger lines, m = skin, L = skin highlight, M = arm shadow,
# '.' = clear, ' ' = keep what's there. 'wrist' = where the patch's top-left goes.
import backpal
from red_hands import MID, LINE, SK
HANDS = {
    1: {'at': (5, 22), 'rows': [        # open hand, fingers up, thumb out to the right (Red's frame 2)
        "...nn.....",
        "..nmLn....",
        ".nmnLn..nn",
        ".nmnmmnnLn",
        "OmLmmmmmLO",
        "OmmmmmmmmO",
        "Ommmmmmmn.",
        ".Ommmmmmn.",
        "..OmmmmmO.",
        "...OOmmmMO",
    ]},
    2: {'at': (9, 6), 'rows': [         # the same hand as frame 2 (approved), held up; wrist narrowed to her forearm
        "...nn.....",
        "..nmLn....",
        ".nmnLn..nn",
        ".nmnmmnnLn",
        "OmLmmmmmLO",
        "OmmmmmmmmO",
        "Ommmmmmmn.",
        ".Ommmmmmn.",
        "..OmmmmmO.",
        "...OOmmO..",
    ]},
    3: {'at': (55, 21), 'rows': [        # the approved frame-2 hand turned 90 degrees: fingers toward the foe, thumb down
        "...OOO....",
        "..Ommmnn..",
        ".OmmmLmmn.",
        "Ommmmmnnmn",
        "OmmmmmmLLn",
        "mmmmmmmnn.",
        "mmmmmmn...",
        "mmmmmmn...",
        "OOnnmLLn..",
        "....OOnn..",
    ]},
}
# pixels that close the outline where a hand meets the arm: {frame: {(x, y): letter}}
JOIN = {3: {}}
# the wrist each patch was drawn for; patches and erase areas follow the real wrist from hands.json
REF = {1: (10, 31), 2: (13, 20), 3: (55, 27)}
# frame 3: widen the raised forearm under the hand to 2 pixels (rows below the wrist, relative to the wrist)
WIDEN = {2: True}
def erase(img, i, d=(0, 0)):
    ox, oy = d
    if i == 1:      # the rendered hand: everything past the wrist, up and to the left
        for y in range(18 + oy, 33 + oy):
            for x in range(0, 15 + ox):
                if 0 <= y < 64 and (x - 10 - ox) * -0.8 + (y - 31 - oy) * -0.6 > 0.5 and img[y][x] in (backpal.O,) + tuple(SK): img[y][x] = None
    if i == 3:
        for y in range(20 + oy, 34 + oy):
            for x in range((54 if y < 25 + oy else 55) + ox, 64):
                if img[y][x] in (backpal.O,) + tuple(SK): img[y][x] = None
    if i == 2:
        for y in range(6 + oy, 16 + oy):
            for x in range(max(0, 5 + ox), 18 + ox):
                if img[y][x] in (backpal.O,) + tuple(SK): img[y][x] = None
def apply(img, i, wrist=None):
    if i not in HANDS: return
    d = (0, 0)
    if wrist: d = (round(wrist[0]) - REF[i][0], round(wrist[1]) - REF[i][1])
    erase(img, i, d)
    C = {'O': backpal.O, 'L': SK[0], 'M': SK[2], 'm': MID, 'n': LINE, '.': None}
    ax, ay = HANDS[i]['at']; ax += d[0]; ay += d[1]
    for dy, row in enumerate(HANDS[i]['rows']):
        for dx, c in enumerate(row):
            if c != ' ' and 0 <= ax + dx < 64 and 0 <= ay + dy < 64: img[ay + dy][ax + dx] = C[c]
    for (x, y), c in JOIN.get(i, {}).items(): img[y][x] = C[c]
    if i in WIDEN:
        # frame 3: forearm and wrist 4 pixels wide (matching the hand), from the hand down until the arm is that wide
        hy = ay + len(HANDS[i]['rows']) - 1                     # the hand's wrist row
        def span(y):
            xs = [x for x in range(0, 32) if img[y][x] in (SK[0], SK[2], MID)]
            return (min(xs), max(xs)) if xs else None
        first = span(hy + 1)
        if first:
            l0 = first[0]
            for y in range(hy, hy + 16):
                sp = span(y) if y > hy else (l0, l0)
                if not sp: break
                l, r = sp
                if y == hy: l = l0
                if r - l + 1 >= 4 and y > hy: break
                col = SK[2]
                for x in range(l, l + 4): img[y][x] = img[y][x] if img[y][x] in (SK[0], SK[2], MID) and y > hy else (MID if y == hy else col)
                img[y][l - 1] = backpal.O; img[y][l + 4] = backpal.O
                if img[y][l + 5] == backpal.O and y > hy: img[y][l + 5] = None
