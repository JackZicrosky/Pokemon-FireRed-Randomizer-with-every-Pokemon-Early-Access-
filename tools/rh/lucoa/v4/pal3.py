# v3 palette (no colour limit yet)
from PIL import Image, ImageDraw
P = {
 '.': (0,0,0,0),
 'K': (41,33,45), 'o': (150,88,70), 'S': (253,231,213), 'O': (234,176,140),
 'Y': (240,242,138), 'y': (255,255,205), 'g': (196,200,88), 'h': (168,226,112), 'e': (98,176,104),
 'c': (139,204,247), 'D': (72,98,178), 'd': (48,60,128), 'L': (170,200,240),
 'P': (246,163,170), 'q': (176,70,98), 'p': (214,102,128), 'W': (248,246,244), 'w': (200,198,206),
 'T': (62,58,74), 'V': (112,104,142),
 'H': (222,150,86), 'j': (246,196,128), 'k': (110,58,36), 'U': (176,168,196), 'v': (124,92,176), 'E': (255,255,255), 'R': (150,84,46), 'r': (220,60,60), 'G': (27,135,98), 'B': (255,200,200),
}
def load(path):
    return [l.rstrip('\n') for l in open(path) if l.strip() and not l.startswith('#')]
def render(rows):
    im = Image.new('RGBA', (len(rows[0]), len(rows)))
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            im.putpixel((x, y), P[ch] if len(P[ch]) == 4 else P[ch] + (255,))
    return im
def zoom(rows, out, z=10, grid=True):
    im = render(rows); w, h = im.size
    o = Image.new('RGBA', (w * z + 30, h * z + 30), (120, 176, 96, 255))
    b = im.resize((w * z, h * z), Image.NEAREST); o.paste(b, (30, 30), b)
    d = ImageDraw.Draw(o)
    if grid:
        for x in range(0, w, 4):
            d.text((30 + x * z + 2, 2), str(x), fill=(0, 0, 0, 255))
            d.line([(30 + x * z, 30), (30 + x * z, 30 + h * z)], fill=(0, 0, 0, 60))
        for y in range(0, h, 4):
            d.text((2, 30 + y * z + 2), str(y), fill=(0, 0, 0, 255))
            d.line([(30, 30 + y * z), (30 + w * z, 30 + y * z)], fill=(0, 0, 0, 60))
    o.save(out)
