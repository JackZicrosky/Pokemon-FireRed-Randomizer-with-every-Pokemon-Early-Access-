from PIL import Image
P = {'.': (0,0,0,0), 'K': (48,36,48), 'T': (66,60,78), 'V': (117,105,158), 'Y': (238,241,133), 'g': (196,200,88),
     'h': (179,236,120), 'c': (139,204,247), 'S': (253,231,213), 'O': (232,170,132), 'o': (150,88,70),
     'p': (222,110,136), 'P': (246,163,170), 'W': (226,224,230), 'D': (80,84,168), 'G': (27,135,98),
     'A': (181,177,184), 'B': (206,188,179), 'e': (98,176,104), 'd': (60,62,130), 'r': (210,60,60)}
def load(path):
    F = []; cur = None
    for l in open(path):
        l = l.rstrip('\n')
        if l.startswith('# frame'): cur = []; F.append(cur)
        elif l and not l.startswith('#'): cur.append(list(l))
    return F
def save(F, path, head='#\n'):
    with open(path, 'w') as f:
        f.write(head)
        for i, fr in enumerate(F): f.write('# frame %d\n' % i + '\n'.join(''.join(r) for r in fr) + '\n')
def render(fr):
    im = Image.new('RGBA', (len(fr[0]), len(fr)))
    for y, r in enumerate(fr):
        for x, c in enumerate(r):
            v = P[c]; im.putpixel((x, y), v if len(v) == 4 else v + (255,))
    return im
def strip(rows_of_frames, out, z=6, bg=(120, 176, 96, 255)):
    fw = len(rows_of_frames[0][0][0]); fh = len(rows_of_frames[0][0])
    n = max(len(r) for r in rows_of_frames)
    o = Image.new('RGBA', (n * (fw * z + 6) + 6, len(rows_of_frames) * (fh * z + 6) + 6), bg)
    for j, row in enumerate(rows_of_frames):
        for i, fr in enumerate(row):
            im = fr if isinstance(fr, Image.Image) else render(fr)
            b = im.resize((fw * z, fh * z), Image.NEAREST); o.paste(b, (6 + i * (fw * z + 6), 6 + j * (fh * z + 6)), b)
    o.save(out)
def gif(frames, out, durations, z=6, bg=(120, 176, 96, 255)):
    ims = []
    for fr in frames:
        im = render(fr); c = Image.new('RGBA', im.size, bg); c.alpha_composite(im)
        ims.append(c.resize((im.width * z, im.height * z), Image.NEAREST).convert('RGB'))
    ims[0].save(out, save_all=True, append_images=ims[1:], duration=durations, loop=0)
