#!/usr/bin/env python3
"""Preview pictures of pack/: everything at 4x (preview/lucoa_everything.png), Lucoa next to Leaf and Cynthia
(preview/lucoa_vs_leaf_cynthia.png) and animated walk/run/bike/throw GIFs."""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
PACKS = os.path.join(HERE, '..', '..', '..', 'graphics', 'rh_player')
BG = (120, 176, 96, 255)


def rgba(path):
    s = Image.open(path)
    im = s.convert('RGBA')
    px = im.load()
    for y in range(s.height):
        for x in range(s.width):
            if s.getpixel((x, y)) == 0:
                px[x, y] = (0, 0, 0, 0)
    return im


def sheet(folder, out, z=4):
    sections = [('normal.png', 16, 32), ('surf.png', 16, 32), ('item.png', 16, 32), ('bike.png', 32, 32),
                ('itembike.png', 32, 32), ('fish.png', 32, 32)]
    W = 12 * (32 * z + 4) + 20
    H = sum(h * z + 30 for _, _, h in sections) + 64 * 3 + 60
    o = Image.new('RGBA', (W, H), BG)
    d = ImageDraw.Draw(o)
    y = 8
    for name, fw, fh in sections:
        d.text((10, y), name, fill=(20, 20, 20, 255)); y += 14
        im = rgba(os.path.join(folder, name))
        for i in range(im.width // fw):
            f = im.crop((i * fw, 0, i * fw + fw, fh)).resize((fw * z, fh * z), Image.NEAREST)
            o.paste(f, (10 + i * (fw * z + 4), y), f)
            d.text((10 + i * (fw * z + 4), y + fh * z + 1), str(i), fill=(20, 20, 20, 255))
        y += fh * z + 16
    d.text((10, y), 'front / back x5 / oak', fill=(20, 20, 20, 255)); y += 14
    zz = 3
    f = rgba(os.path.join(folder, 'front.png')).resize((64 * zz, 64 * zz), Image.NEAREST)
    o.paste(f, (10, y), f)
    b = rgba(os.path.join(folder, 'back.png'))
    for i in range(5):
        f = b.crop((0, 64 * i, 64, 64 * i + 64)).resize((64 * zz, 64 * zz), Image.NEAREST)
        o.paste(f, (10 + (i + 1) * (64 * zz + 8), y), f)
    ok = rgba(os.path.join(folder, 'oak.png')).resize((128, 192), Image.NEAREST)
    o.paste(ok, (10 + 6 * (64 * zz + 8), y), ok)
    o.crop((0, 0, o.width, y + 200)).save(out)


def versus(out):
    rows = [('Lucoa', os.path.join(HERE, 'pack')), ('Leaf', os.path.join(PACKS, 'leaf')),
            ('Cynthia', os.path.join(PACKS, 'cynthia'))]
    z = 4
    o = Image.new('RGBA', (20 * (16 * z + 4) + 20, len(rows) * (32 * z + 20) + 10 + len(rows) * (64 * 3 + 20)), BG)
    d = ImageDraw.Draw(o)
    y = 6
    for name, folder in rows:
        d.text((10, y), name, fill=(20, 20, 20, 255))
        im = rgba(os.path.join(folder, 'normal.png'))
        for i in range(20):
            f = im.crop((i * 16, 0, i * 16 + 16, 32)).resize((16 * z, 32 * z), Image.NEAREST)
            o.paste(f, (10 + i * (16 * z + 4), y + 14), f)
        y += 32 * z + 20
    for name, folder in rows:
        f = rgba(os.path.join(folder, 'front.png')).resize((192, 192), Image.NEAREST)
        o.paste(f, (10, y), f)
        b = rgba(os.path.join(folder, 'back.png'))
        for i in range(5):
            f = b.crop((0, 64 * i, 64, 64 * i + 64)).resize((192, 192), Image.NEAREST)
            o.paste(f, (10 + (i + 1) * 200, y), f)
        d.text((10, y), name, fill=(20, 20, 20, 255))
        y += 64 * 3 + 20
    o.save(out)


def gif(out, sheetname, fw, cycles, durations, z=5):
    im = rgba(os.path.join(HERE, 'pack', sheetname))
    frames = []
    for step in range(len(cycles[0])):
        c = Image.new('RGBA', (len(cycles) * (fw * z + 8) + 8, 32 * z + 16), BG)
        for k, cyc in enumerate(cycles):
            f = im.crop((cyc[step] * fw, 0, cyc[step] * fw + fw, 32)).resize((fw * z, 32 * z), Image.NEAREST)
            c.paste(f, (8 + k * (fw * z + 8), 8), f)
        frames.append(c.convert('RGB'))
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=durations, loop=0)


def back_gif(out, z=3):
    b = rgba(os.path.join(HERE, 'pack', 'back.png'))
    frames = []
    for i in (0, 0, 1, 2, 3, 4, 4):
        c = Image.new('RGBA', (64 * z + 16, 64 * z + 16), BG)
        f = b.crop((0, 64 * i, 64, 64 * i + 64)).resize((64 * z, 64 * z), Image.NEAREST)
        c.paste(f, (8, 8), f)
        frames.append(c.convert('RGB'))
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=[400, 400, 150, 150, 150, 400, 400], loop=0)


if __name__ == '__main__':
    P = os.path.join(HERE, 'preview')
    os.makedirs(P, exist_ok=True)
    sheet(os.path.join(HERE, 'pack'), os.path.join(P, 'lucoa_everything.png'))
    versus(os.path.join(P, 'lucoa_vs_leaf_cynthia.png'))
    gif(os.path.join(P, 'walk.gif'), 'normal.png', 16, [(3, 0, 4, 0), (5, 1, 6, 1), (7, 2, 8, 2)], [133] * 4)
    gif(os.path.join(P, 'run.gif'), 'normal.png', 16, [(10, 9, 11, 9), (13, 12, 14, 12), (16, 15, 17, 15)],
        [83, 50, 83, 50])
    gif(os.path.join(P, 'bike.gif'), 'bike.png', 32, [(3, 0, 4, 0), (5, 1, 6, 1), (7, 2, 8, 2)], [133] * 4)
    back_gif(os.path.join(P, 'throw.gif'))
