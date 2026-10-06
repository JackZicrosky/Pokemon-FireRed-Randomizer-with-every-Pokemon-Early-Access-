# Lucoa trainer front pic, fully hand-placed. LH = left half (x 0-31) per row, mirrored to x 32-63.
# Then: hair lock texture pass, light-from-the-left shading pass, asymmetric touches.
import os, sys, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bpal import render
from PIL import Image
S = os.path.dirname(os.path.abspath(__file__))
LH = [
    "................KK..............",  # 0  horn tip
    "...............KOOK.............",  # 1
    "...............KooK.............",  # 2
    "..............KOOOK.............",  # 3
    "..............KoooK.........KKKK",  # 4  cap top
    "..............KOOOOK......KKPPPP",  # 5
    "...............KooooK....KPPPPPP",  # 6
    "...............KOOOOOK..KPPPPPPP",  # 7
    "................KoooooKKPPPPPPPP",  # 8
    ".................KOOOOOKPPPPPPPP",  # 9
    "..................KooooKPPPPPPPP",  # 10
    "................KKYKKKKPPPPPPPPP",  # 11
    "..............KKYYYYKWWWWWWWWWWW",  # 12 brim
    ".............KYYYYYYYKKKKKKKKKKK",  # 13
    "............KYYYYYYYYYgggggggggg",  # 14 shadow under the brim
    "...........KYYYYYYYYYYYYYYgYYYYY",  # 15 bangs
    "...........KYYYYYYYYYYYYYgYYgYYY",  # 16
    "..........KYYYYYYYYYYYYYYgSSgYYY",  # 17
    "..........KYYYYYYYYYYYYYgSSKKSgY",  # 18 closed eyes
    "..........KYYYYYYYYYYYYYgSKSSKSg",  # 19
    "...........KYYYYYYYYYYYYgSppSSSS",  # 20 blush
    "...........KYYYYYYYYYYYYgSSSSSoS",  # 21 smile
    "..........KYYYYYYYYYYYYYgoSSSSSo",  # 22
    "..........KYYYYYYYYYYYYYYYgoSSSS",  # 23
    "..........KYYYYYYYYYYYYYYYYYgoOO",  # 24 chin
    "...........KYYYYYYYYYYYYYYYYYoSS",  # 25 neck
    "...........KYYYYYYYYYYYYoSSTTSSS",  # 26 shoulders, straps
    "..........KYYYYYYYYYYYoSSSSTTSSS",  # 27
    ".........KYYYYYYYYYYYoSSOKTTTTSS",  # 28
    ".........KYYYYYYYYYYoSSOKTVVVTTS",  # 29 bust
    ".........KhhhhhhhhhhoSSOKVTTTVTT",  # 30
    "..........KhhhhhhhhhoSSOKTTTTTVT",  # 31
    "..........KhhhhhhhhhoSSOKTTTTTTT",  # 32
    "...........KhhhhhhhhoSSOKTTTTTTT",  # 33
    "...........KhhhhhhhhoSSOKKKKKKTT",  # 34 under the bust
    "..........KhhhhhhhhhoSOohKTTTTTT",  # 35 waist (hair shows between arm and waist)
    "..........KhhhhhhhhhoSOohKTTTTTT",  # 36
    ".........KcccccccccoSSOohKTTTTTT",  # 37
    ".........KcccccccDcoSSOoKKKKKKKK",  # 38 tank hem
    "........KcccDcccDcoSSSOoKDDDDDDD",  # 39 shorts, hands
    "........KccDcccDccoSSSOoKDDDDDDD",  # 40
    ".........KcDccKcDcKoSoSoKDDDDDDD",  # 41
    ".........KcDcK.KcDK.o.oKcccccccK",  # 42 cuffs
    "..........KcK...KcK....KDDDDDDDK",  # 43
    "...........K.....K.....KKKKKKKKK",  # 44
    "......................oSSSSSSSo.",  # 45 thighs
    "......................oSSSSSSOo.",  # 46
    "......................oSSSSSSOo.",  # 47
    "......................KTTTTTTTK.",  # 48 thigh-high tops
    "......................KVTTTTTTK.",  # 49
    ".......................KVTTTTTK.",  # 50
    ".......................KVTTTTTK.",  # 51
    ".......................KVTTTTK..",  # 52 knee
    "........................KVTTTK..",  # 53
    "........................KVTTTK..",  # 54
    "........................KVTTTK..",  # 55
    "........................KVTTTK..",  # 56
    "........................KVTTTK..",  # 57
    ".......................KKPPPPK..",  # 58 shoes
    "......................KPPPPPPK..",  # 59
    "......................KWPPPPpK..",  # 60
    "......................KWWWWWWK..",  # 61
    ".......................KKKKKK...",  # 62
    "................................",  # 63
]
assert len(LH) == 64, len(LH)
for i, r in enumerate(LH):
    assert len(r) == 32, (i, len(r), r)
G = [list(r) + list(r[::-1]) for r in LH]


def wave(y):
    return int(round(1.6 * math.sin(y / 3.0)))


SH = {'Y': 'g', 'h': 'e', 'c': 'D'}
# hair lock texture: wavy lock lines every 5px that break now and then, plus shade along the outer
# edge so the mass looks round (not in the bangs/face area)
for y in range(11, 47):
    for x in range(1, 63):
        c = G[y][x]
        if c not in SH:
            continue
        if 22 <= x <= 41 and y < 26:
            continue
        xs = x if x < 32 else 63 - x
        line = (xs + wave(y)) % 5 == 0 and (y // 3 + xs // 5) % 4 != 0
        edge = G[y][x - 1] == 'K' and x < 32 or G[y][x + 1] == 'K' and x >= 32
        if (line or edge) and G[y][x - 1] != '.' and G[y][x + 1] != '.':
            G[y][x] = SH[c]
# gradient dithering at the colour changes
for y, (a, b) in ((29, ('Y', 'h')), (36, ('h', 'c'))):
    for x in range(64):
        if G[y][x] == b and (x % 2 == 0):
            G[y][x] = a
        if G[y + 1][x] == b and (x % 2 == 0) and y + 1 == 30:
            pass

# light from the left: the right half's skin/cap edges get shade
for y in range(64):
    for x in range(36, 64):
        if G[y][x] == 'P' and G[y][x + 1] in 'K':
            G[y][x] = 'p'

for y in range(49, 58):
    r = G[y]
    xs = [x for x in range(32, 64) if r[x] == 'V']
    for x in xs:
        r[x] = 'T'
        k = x
        while r[k - 1] != 'K':
            k -= 1
        r[k] = 'V'
rows = [''.join(r) for r in G]
open(S + '/frames/front.txt', 'w').write('\n'.join(rows) + '\n')
im = render(rows)
o = Image.new('RGBA', (64 * 8 + 64 * 2 + 32, 64 * 8 + 16), (120, 176, 96, 255))
b = im.resize((512, 512), Image.NEAREST); o.paste(b, (8, 8), b)
b2 = im.resize((128, 128), Image.NEAREST); o.paste(b2, (536, 8), b2)
o.paste(im, (536, 150), im)
o.save(S + '/preview/front_zoom.png')
