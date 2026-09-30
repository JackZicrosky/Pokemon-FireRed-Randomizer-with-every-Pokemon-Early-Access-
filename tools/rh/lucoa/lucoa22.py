# Lucoa overworld sprite (FRLG player format: normal.png = 20 frames of 16x32, 16 colours).
# Master copy: lucoa_frames.txt (the owner's hand-edited sheet, one letter per pixel).
# This script only renders it: the game sheet, a preview and the walk/run GIFs.
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))

PAL = {
    '.': (0, 0, 0, 0),
    'K': (48, 36, 48),      # outline
    'T': (66, 60, 78),      # charcoal top
    't': (104, 96, 120),    # top highlight / shadow under the bill
    'Y': (238, 241, 133),   # hair yellow
    'g': (196, 200, 88),    # hair shade
    'h': (179, 236, 120),   # hair green
    'c': (139, 204, 247),   # hair blue tips
    'S': (253, 231, 213),   # skin
    's': (234, 180, 158),   # skin shade
    'p': (222, 110, 136),   # cap shade / blush / shoes
    'P': (246, 163, 170),   # cap pink / shoe highlight
    'W': (226, 224, 230),   # cap bill / band
    'O': (230, 178, 138),   # horn
    'o': (168, 110, 78),    # horn bands
    'D': (80, 84, 168),     # jean shorts / purple eye
}


def load_frames():
    frames, cur = [], None
    for line in open(os.path.join(HERE, 'lucoa_frames.txt')):
        line = line.rstrip('\n')
        if line.startswith('# frame'):
            cur = []
            frames.append(cur)
        elif line and not line.startswith('#'):
            assert len(line) == 16 and all(c in PAL for c in line), line
            cur.append(list(line))
    assert len(frames) == 20 and all(len(f) == 32 for f in frames)
    return frames


def render(frame):
    im = Image.new('RGBA', (16, 32))
    for y, row in enumerate(frame):
        for x, ch in enumerate(row):
            c = PAL[ch]
            im.putpixel((x, y), c if len(c) == 4 else c + (255,))
    return im


def main():
    bg = (120, 176, 96, 255)
    frames = load_frames()
    used = {ch for fr in frames for row in fr for ch in row}
    print('colours used (incl. transparent):', len(used))
    sheet = Image.new('RGBA', (16 * 20, 32))
    for i, fr in enumerate(frames):
        sheet.paste(render(fr), (16 * i, 0))
    sheet.save(os.path.join(HERE, 'lucoa_normal.png'))

    def anim(name, cycles, dur):
        imgs = []
        for step in range(4):
            canvas = Image.new('RGBA', (3 * 88 + 8, 32 * 5 + 16), bg)
            for d, cyc in enumerate(cycles):
                big = render(frames[cyc[step]]).resize((80, 160), Image.NEAREST)
                canvas.paste(big, (8 + d * 88, 8), big)
            imgs.append(canvas.convert('RGB'))
        imgs[0].save(os.path.join(HERE, name), save_all=True, append_images=imgs[1:], duration=dur, loop=0)
    anim('lucoa_walk.gif', [(3, 0, 4, 0), (5, 1, 6, 1), (7, 2, 8, 2)], [133] * 4)
    anim('lucoa_run.gif', [(10, 9, 11, 9), (13, 12, 14, 12), (16, 15, 17, 15)], [83, 50, 83, 50])


if __name__ == '__main__':
    main()
