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
    2: {'at': (7, 6), 'rows': [         # the same hand as frame 2 (approved), held up; wrist narrowed to her forearm
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
}
def erase(img, i):
    if i == 1:      # the rendered hand: everything past the wrist, up and to the left
        for y in range(18, 33):
            for x in range(0, 15):
                if (x - 10) * -0.8 + (y - 31) * -0.6 > 0.5 and img[y][x] in (backpal.O,) + tuple(SK): img[y][x] = None
    if i == 2:
        for y in range(6, 16):
            for x in range(5, 18):
                if img[y][x] in (backpal.O,) + tuple(SK): img[y][x] = None
def apply(img, i):
    if i not in HANDS: return
    erase(img, i)
    C = {'O': backpal.O, 'L': SK[0], 'M': SK[2], 'm': MID, 'n': LINE, '.': None}
    ax, ay = HANDS[i]['at']
    for dy, row in enumerate(HANDS[i]['rows']):
        for dx, c in enumerate(row):
            if c != ' ': img[ay + dy][ax + dx] = C[c]
