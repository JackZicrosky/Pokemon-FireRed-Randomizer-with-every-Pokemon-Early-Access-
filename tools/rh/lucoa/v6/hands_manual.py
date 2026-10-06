# Hand-drawn throwing hands for frames 2 and 3 (index 1, 2). O = outline, L = skin, M = skin shadow,
# '.' = clear, ' ' = leave the pixel as rendered. (x0, y0) = top-left of the patch; ERASE clears the old hand first.
import backpal
HANDS = {
    1: {'erase': (0, 48, 7, 58), 'at': (0, 48), 'rows': [
        "..OOLLLLMMOO",
        ".OLLLLLLMO  ",
        "OLLLLLLMMO  ",
        "OLMLLLLMMO  ",
        "OLLLLLMMMO  ",
        "OLMLLMMMMO  ",
        ".OLMMMMMO   ",
        "..OOOOOO    ",
    ]},
    2: {'erase': (3, 7, 17, 16), 'at': (7, 7), 'rows': [
        "          ",
        "..OOOOOO..",
        ".OLLLLLLO.",
        "OLLMLLMLMO",
        "OLLLLLLMMO",
        "OLLLLLLMMO",
        "OMLLLLMMMO",
        ".OMLLMMMO.",
        "..OLMMMO..",
    ]},
}
def apply(img, i):
    if i not in HANDS: return
    h = HANDS[i]; x0, y0, x1, y1 = h['erase']
    for y in range(y0, y1):
        for x in range(x0, x1):
            if img[y][x] is not None and img[y][x] in (backpal.O,) + tuple(backpal.PAL['skin']): img[y][x] = None
    ax, ay = h['at']; sk = backpal.PAL['skin']
    for dy, row in enumerate(h['rows']):
        for dx, c in enumerate(row):
            if c == ' ': continue
            img[ay + dy][ax + dx] = {'.': None, 'O': backpal.O, 'L': sk[0], 'M': sk[2]}[c]
