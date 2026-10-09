#!/usr/bin/env python3
"""Custom Player Graphics: converts UPR FVX Gen 3 graphics packs (GPL-3.0; the art belongs to the credited creators)
into FireRed-format sprites.

Output: graphics/rh_player/<pack>/*.png (16-color indexed) and src/data/rh_player_graphics.h
FVX layout rules follow romio/graphics/packs/{Gen3,FRLG,RSE}PlayerCharacterGraphics.java."""
import configparser, os, shutil
from PIL import Image

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')) + '/'
FVX = '/home/claude/work/fvx/random/src/main/resources/data/custom_player_graphics/'
OUT = R + 'graphics/rh_player/'

# (folder, display name). FRLG-format packs are what UPR FVX offers for FireRed; the RSE ones are converted too.
PACKS = [
    ('ethan_frlg', 'Ethan'),
    ('kris_frlg', 'Kris'),
    ('red_fr', 'Red (FR/LG)'),
    ('leaf', 'Leaf'),
    ('brendan_e', 'Brendan (E)'),
    ('may_e', 'May (E)'),
    ('brendan_rs', 'Brendan (R/S)'),
    ('may_rs', 'May (R/S)'),
    ('wally', 'Wally'),
    ('prof_birch', 'Prof. Birch'),
    ('cynthia', 'Cynthia'),
    ('ghost', 'Ghost (Snakewood)'),
    ('wraith', 'Wraith (Snakewood)'),
]

# Packs made for this romhack: already in FireRed format in graphics/rh_player/<folder>/ (not converted).
LOCAL_PACKS = [
    ('lucoa', 'Lucoa', "From=Miss Kobayashi's Dragon Maid (fan art)\n"
                       "Creator=JackZicrosky (owner) and Claude (overworld: owner's walk/run sheet, other sheets built off Leaf's FR/LG frames)\n"
                       "Battle front / Oak intro picture: based on the fan pixel art \"Lucoa Pixel Art 1\" (artist unknown)\n"
                       "Battle back picture: rendered from a fan-made rigged 3D model of Lucoa (artist unknown)\n"),
]

WALK_ORDER = [(0, 0), (0, 1), (0, 2), (1, 0), (2, 0), (1, 1), (2, 1), (1, 2), (2, 2)]
GEN3_SHEET = {
    'FrontImage': (11, 53, 64, 64, 0, [(0, 0)]),
    'WalkSprite': (11, 139, 16, 32, 1, WALK_ORDER),
    'RunSprite': (66, 139, 16, 32, 1, WALK_ORDER),
    'BikeSprite': (121, 139, 32, 32, 1, WALK_ORDER),
    'FishSprite': (224, 139, 32, 32, 1, [(0, 2), (1, 2), (2, 2), (3, 2), (0, 1), (1, 1), (2, 1), (3, 1),
                                         (0, 0), (1, 0), (2, 0), (3, 0)]),
}
FRLG_SHEET = {
    'BackImage': (90, 53, 64, 64, 1, [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0)]),
    'SitSprite': (11, 327, 16, 32, 1, [(0, 0), (0, 1), (0, 2)]),
    'ItemSprite': (11, 250, 16, 32, 1, [(0, 0), (1, 0), (2, 1), (3, 1), (4, 1), (2, 0), (3, 0), (4, 0), (5, 0)]),
    'ItemBikeSprite': (117, 250, 32, 32, 1, [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0)]),
}
RSE_SHEET = {
    'BackImage': (90, 53, 64, 64, 1, [(1, 0), (2, 0), (3, 0), (0, 0)]),
    'SitSprite': (81, 360, 32, 32, 1, [(0, 0), (0, 1), (0, 2)]),
}


def rgba_from_file(path):
    im = Image.open(path)
    if im.mode == 'P':
        pal = im.getpalette()
        tr = tuple(pal[0:3])
        rgba = im.convert('RGBA')
        px = rgba.load()
        src = im.load()
        for y in range(im.height):
            for x in range(im.width):
                if src[x, y] == 0:
                    px[x, y] = (0, 0, 0, 0)
        return rgba
    rgba = im.convert('RGBA')
    bg = rgba.getpixel((0, 0))
    return transparent(rgba, bg)


def transparent(img, bg):
    img = img.copy()
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            if px[x, y][:3] == bg[:3]:
                px[x, y] = (0, 0, 0, 0)
    return img


def frames_of(img, fw, fh):
    return [img.crop((i * fw, 0, (i + 1) * fw, fh)) for i in range(img.width // fw)] if img.height == fh else \
           [img.crop((0, i * fh, fw, (i + 1) * fh)) for i in range(img.height // fh)]


def sheet_frames(sheet, desc):
    x0, y0, fw, fh, margin, frames = desc
    out = []
    for fx, fy in frames:
        x = x0 + fx * (fw + margin)
        y = y0 + fy * (fh + margin)
        crop = sheet.crop((x, y, x + fw, y + fh))
        out.append(transparent(crop, crop.getpixel((0, 0))))
    return out


def center16(frame32):
    return frame32.crop((8, 0, 24, 32))


def load_pack(folder):
    ini = configparser.ConfigParser(strict=False)
    ini.optionxform = str
    ini.read(FVX + folder + '/info.ini', encoding='utf-8')
    sec = ini[ini.sections()[0]]
    romtype = sec.get('RomType')
    g = {}
    if sec.get('Sheet'):
        sheet = Image.open(FVX + folder + '/' + sec.get('Sheet').split('//')[0].strip()).convert('RGBA')
        descs = dict(GEN3_SHEET)
        descs.update(FRLG_SHEET if romtype == 'FRLG' else RSE_SHEET)
        for k, d in descs.items():
            g[k] = sheet_frames(sheet, d)
        run_mode = 'rse'                                    # sheets always use the RSE run layout (FVX)
    else:
        def f(key):
            v = sec.get(key)
            return FVX + folder + '/' + v.split('//')[0].strip() if v else None
        sizes = {'FrontImage': (64, 64), 'WalkSprite': (16, 32), 'RunSprite': (16, 32), 'BikeSprite': (32, 32),
                 'FishSprite': (32, 32), 'ItemSprite': (16, 32), 'ItemBikeSprite': (32, 32), 'BackImage': (64, 64)}
        for k, (fw, fh) in sizes.items():
            p = f(k)
            if p and os.path.exists(p):
                g[k] = frames_of(rgba_from_file(p), fw, fh)
        sit_frlg = FVX + folder + '/sit_frlg.png'
        if os.path.exists(sit_frlg):
            g['SitSprite'] = frames_of(rgba_from_file(sit_frlg), 16, 32)
        elif f('SitSprite'):
            g['SitSprite'] = frames_of(rgba_from_file(f('SitSprite')), 32, 32)
        run_mode = (sec.get('RunSpriteMode') or 'rse').lower()
    # normalize
    walk = g['WalkSprite'][:9]
    run = g.get('RunSprite', walk)[:9]
    if run_mode == 'rse':
        run = [run[i] for i in (0, 3, 4, 1, 5, 6, 2, 7, 8)]
    sit = [s if s.width == 16 else center16(s) for s in g.get('SitSprite', [walk[0], walk[1], walk[2]])][:3]
    bike = g['BikeSprite'][:9]
    fish = g['FishSprite'][:12]
    if 'ItemSprite' in g:
        item = g['ItemSprite'][:9]
    else:
        item = [walk[0], walk[0], walk[0], walk[0], walk[0], walk[0], walk[0], walk[0], walk[0]]
    if 'ItemBikeSprite' in g:
        itembike = g['ItemBikeSprite'][:6]
    else:
        itembike = [bike[0]] * 6
    back = g['BackImage']
    if len(back) == 4:
        # RSE packs: [raise, throw, stand, stand] -> FireRed's 5 frames [stand, stand, raise, throw, throw]
        back = [back[3], back[2], back[0], back[1], back[1]]
    return dict(front=g['FrontImage'][0], back=back[:5], normal=walk + run + [walk[0], walk[0]],
                surf=sit, bike=bike, fish=fish, item=item, itembike=itembike)


def build_palette(images):
    colors = {}
    for im in images:
        for c in im.getdata():
            if c[3] >= 128:
                colors[c[:3]] = colors.get(c[:3], 0) + 1
    cols = sorted(colors, key=lambda c: -colors[c])
    if len(cols) > 15:
        strip = Image.new('RGB', (len(colors), 1))
        strip.putdata(list(colors))
        q = strip.quantize(15)
        p = q.getpalette()[:45]
        cols = [tuple(p[i:i + 3]) for i in range(0, 45, 3)]
    return [(0, 0, 0)] + cols[:15]


def nearest(pal, c):
    best, bd = 1, 1 << 30
    for i in range(1, len(pal)):
        p = pal[i]
        d = (p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2
        if d < bd:
            best, bd = i, d
    return best


def save_indexed(frames, pal, path, horizontal=True):
    fw, fh = frames[0].width, frames[0].height
    W, H = (fw * len(frames), fh) if horizontal else (fw, fh * len(frames))
    out = Image.new('P', (W, H), 0)
    flat = []
    for c in pal:
        flat += list(c)
    flat += [0] * (768 - len(flat))
    out.putpalette(flat)
    cache = {}
    px = out.load()
    for i, fr in enumerate(frames):
        ox, oy = (i * fw, 0) if horizontal else (0, i * fh)
        data = fr.load()
        for y in range(fh):
            for x in range(fw):
                c = data[x, y]
                if c[3] < 128:
                    continue
                k = c[:3]
                if k not in cache:
                    cache[k] = nearest(pal, k)
                px[ox + x, oy + y] = cache[k]
    out.save(path)


def save_oak_pic(front_path, out_path):
    # Oak's intro draws the player on an 8bpp BG (64x96, palette slot 4 = colours 64..79):
    # the 64x64 front pic sits at the bottom, colour i -> index 64+i, transparent stays 0.
    front = Image.open(front_path)
    src = front.load()
    oak = Image.new('P', (64, 96), 0)
    pal = [0] * 768
    fp = front.getpalette()
    for i in range(16):
        pal[(64 + i) * 3:(64 + i) * 3 + 3] = fp[i * 3:i * 3 + 3]
    oak.putpalette(pal)
    dst = oak.load()
    for y in range(64):
        for x in range(64):
            c = src[x, y]
            dst[x, y + 32] = 0 if c == 0 else 64 + c
    oak.save(out_path)


def old_credits():
    """Credits of already-converted packs, from the existing CREDITS.txt (used when the FVX source is absent)."""
    out, cur = {}, None
    path = OUT + 'CREDITS.txt'
    if os.path.exists(path):
        for line in open(path, encoding='utf-8').read().splitlines():
            if line.startswith('[') and line.endswith(']'):
                cur = line[1:-1]
                out[cur] = ''
            elif cur is not None and line.strip() and not line.startswith(('Custom player graphics', 'All art', '(plus packs')):
                out[cur] += line + '\n'
    return out


def main():
    have_fvx = os.path.isdir(FVX)
    prev_credits = old_credits()
    h = '// AUTO-GENERATED by tools/rh/gen_player_graphics.py - do not edit by hand\n'
    h += '// Player graphics converted from UPR FVX graphics packs (see graphics/rh_player/CREDITS.txt).\n'
    credits = 'Custom player graphics come from Universal Pokemon Randomizer FVX graphics packs\n' \
              '(plus packs made for this romhack). All art belongs to the creators listed below.\n\n'
    names = []
    all_packs = [(f, n, None) for f, n in PACKS] + list(LOCAL_PACKS)
    for idx, (folder, name, local_credit) in enumerate(all_packs):
        d = OUT + folder + '/'
        if local_credit is None and have_fvx:
            # Convert from the UPR FVX source.
            pk = load_pack(folder)
            if os.path.exists(d):
                shutil.rmtree(d)
            os.makedirs(d, exist_ok=True)
            ow = pk['normal'] + pk['surf'] + pk['bike'] + pk['fish'] + pk['item'] + pk['itembike']
            pal = build_palette(ow)
            for k in ['normal', 'surf', 'bike', 'fish', 'item', 'itembike']:
                save_indexed(pk[k], pal, d + k + '.png')
            save_indexed([pk['front']], build_palette([pk['front']]), d + 'front.png')
            save_indexed(pk['back'], build_palette(pk['back']), d + 'back.png', horizontal=False)
            save_oak_pic(d + 'front.png', d + 'oak.png')
            ini = open(FVX + folder + '/info.ini', encoding='utf-8').read()
            credit = ''.join(l + '\n' for l in ini.splitlines() if l.split('=')[0] in ('Creator', 'Adapter', 'From'))
        else:
            # Already converted (no FVX source on this machine) or a romhack-made pack: use the files as they are.
            assert os.path.exists(d + 'normal.png'), 'missing pack ' + d
            credit = local_credit if local_credit is not None else prev_credits.get(name, '')
        credits += f'[{name}]\n' + credit + '\n'
        p = f'"graphics/rh_player/{folder}/'
        n = f'sRhPg{idx}'
        h += f'static const u16 {n}_Normal[] = INCGFX_U16({p}normal.png", ".4bpp", "-mwidth 2 -mheight 4");\n'
        h += f'static const u16 {n}_Surf[] = INCGFX_U16({p}surf.png", ".4bpp", "-mwidth 2 -mheight 4");\n'
        h += f'static const u16 {n}_Bike[] = INCGFX_U16({p}bike.png", ".4bpp", "-mwidth 4 -mheight 4");\n'
        h += f'static const u16 {n}_Fish[] = INCGFX_U16({p}fish.png", ".4bpp", "-mwidth 4 -mheight 4");\n'
        h += f'static const u16 {n}_Item[] = INCGFX_U16({p}item.png", ".4bpp", "-mwidth 2 -mheight 4");\n'
        h += f'static const u16 {n}_ItemBike[] = INCGFX_U16({p}itembike.png", ".4bpp", "-mwidth 4 -mheight 4");\n'
        h += f'const u16 gRhPlayerPal{idx}[] = INCGFX_U16({p}normal.png", ".gbapal");\n'
        h += f'static const u32 {n}_Front[] = INCGFX_U32({p}front.png", ".4bpp.smol");\n'
        h += f'static const u16 {n}_FrontPal[] = INCGFX_U16({p}front.png", ".gbapal");\n'
        h += f'static const u32 {n}_Oak[] = INCGFX_U32({p}oak.png", ".8bpp.smol");\n'
        h += f'static const u8 {n}_Back[] = INCGFX_U8({p}back.png", ".4bpp");\n'
        h += f'static const u16 {n}_BackPal[] = INCGFX_U16({p}back.png", ".gbapal");\n'
        # pic tables in FireRed's frame layouts
        h += f'static const struct SpriteFrameImage {n}_PicNormal[] = {{\n'
        for i in range(20):
            h += f'    overworld_frame({n}_Normal, 2, 4, {i}),\n'
        h += '};\n'
        h += f'static const struct SpriteFrameImage {n}_PicSurf[] = {{\n'
        for i in [0, 1, 2, 0, 0, 1, 1, 2, 2, 0, 1, 2]:
            h += f'    overworld_frame({n}_Surf, 2, 4, {i}),\n'
        h += '};\n'
        h += f'static const struct SpriteFrameImage {n}_PicBike[] = {{ overworld_ascending_frames({n}_Bike, 4, 4) }};\n'
        h += f'static const struct SpriteFrameImage {n}_PicFish[] = {{ overworld_ascending_frames({n}_Fish, 4, 4) }};\n'
        h += f'static const struct SpriteFrameImage {n}_PicItem[] = {{ overworld_ascending_frames({n}_Item, 2, 4) }};\n'
        h += f'static const struct SpriteFrameImage {n}_PicItemBike[] = {{ overworld_ascending_frames({n}_ItemBike, 4, 4) }};\n\n'
        names.append((n, name))
    h += 'static const struct RhPlayerPack sRhPlayerPacks[] = {\n'
    for i, (n, name) in enumerate(names):
        h += (f'    {{ .normal = {n}_PicNormal, .surf = {n}_PicSurf, .bike = {n}_PicBike, .fish = {n}_PicFish, '
              f'.item = {n}_PicItem, .itemBike = {n}_PicItemBike, .front = {n}_Front, .frontPal = {n}_FrontPal, .oak = {n}_Oak, '
              f'.back = {n}_Back, .backPal = {n}_BackPal }},\n')
    h += '};\n'
    h += 'const u8 *const gRhPlayerGraphicsNames[] = {\n    COMPOUND_STRING("Default"),\n'
    for n, name in names:
        h += f'    COMPOUND_STRING("{name}"),\n'
    h += '};\nconst u8 gRhPlayerGraphicsCount = ARRAY_COUNT(gRhPlayerGraphicsNames);\n'
    open(R + 'src/data/rh_player_graphics.h', 'w').write(h)
    # palette table entries for event_object_movement.c
    pe = '// AUTO-GENERATED by tools/rh/gen_player_graphics.py\n'
    for i in range(len(names)):
        pe += f'    {{gRhPlayerPal{i}, OBJ_EVENT_PAL_TAG_RH_PLAYER + {i}}},\n'
    open(R + 'src/data/rh_player_palettes.h', 'w').write(pe)
    ext = '// AUTO-GENERATED by tools/rh/gen_player_graphics.py\n#define RH_PLAYER_PACK_COUNT %d\n' % len(names)
    for i in range(len(names)):
        ext += f'extern const u16 gRhPlayerPal{i}[];\n'
    open(R + 'include/rh_player_palettes.h', 'w').write(ext)
    open(OUT + 'CREDITS.txt', 'w').write(credits)
    print('packs', len(names))


main()
