# Battle-pic palette (front/oak share it; back has the same colours) + render helpers.
from PIL import Image
BPAL = {
    '.': (0, 0, 0, 0),
    'K': (41, 33, 45),      # outline: hair, cap, clothes
    'o': (150, 88, 70),     # skin outline, horn rings, mouth
    'S': (253, 231, 213),   # skin
    'O': (234, 176, 140),   # skin shade, horn
    'Y': (240, 242, 138),   # hair yellow
    'g': (196, 200, 88),    # hair yellow shade
    'h': (168, 226, 112),   # hair green
    'e': (98, 176, 104),    # hair green shade
    'c': (139, 204, 247),   # hair blue, shorts cuffs
    'D': (72, 98, 178),     # denim, hair blue shade
    'P': (246, 163, 170),   # cap pink, shoes
    'p': (214, 102, 128),   # cap shade, blush, shoes shade
    'W': (248, 246, 244),   # cap brim, shoe soles
    'T': (62, 58, 74),      # tank top, thigh-highs
    'V': (112, 104, 142),   # tank / thigh-high sheen
}
ORDER = '.KoSOYghecDPpWTV'


def render(rows):
    h = len(rows); w = len(rows[0])
    im = Image.new('RGBA', (w, h))
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            im.putpixel((x, y), BPAL[ch])
    return im


def save_indexed(rows, path):
    h = len(rows); w = len(rows[0])
    im = Image.new('P', (w, h))
    pal = []
    for ch in ORDER:
        pal += list(BPAL[ch][:3]) if ch != '.' else [115, 197, 164]
    im.putpalette(pal + [0] * (768 - len(pal)))
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            im.putpixel((x, y), ORDER.index(ch))
    im.save(path, transparency=0)


def load_rows(path):
    return [l.rstrip('\n') for l in open(path) if l.strip() and not l.startswith('#')]
