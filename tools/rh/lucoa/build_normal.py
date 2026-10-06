# New Lucoa walk/run sheet, built from the owner's latest normal.png (user_normal_frames.txt):
# rounder cap, fuller hair, brown skin outlines, 15-colour palette.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lc
S = os.path.dirname(os.path.abspath(__file__))
U = lc.load(S + '/frames/owner_normal.txt')
MERGE = {'t': 'V', 's': 'O'}                 # palette merges (near-identical colours)
F = [[[MERGE.get(c, c) for c in row] for row in fr] for fr in U]


def put(fr, y0, rows):
    for j, r in enumerate(rows):
        assert len(r) == 16, (len(r), r)
        fr[y0 + j] = list(r)


def blank():
    return [['.'] * 16 for _ in range(32)]


# ---------------------------------------------------------------- front (stand rows 10-30)
FRONT_HEAD = [                 # rows 10-24
    ".K............K.",
    "KOK..........KOK",
    "KoK..KKKKKK..KoK",        # rounded dome: 6, 8, 10, 12, 14 wide
    "KOK.KpPPPPpK.KOK",
    "KoOKPPPPPPPPKOoK",
    ".KOOPPPPPPPPOOK.",
    ".KpPPPPPPPPPPpK.",
    ".KppPPPPPPPPppK.",
    ".KYKWWppppWWKYK.",        # bill with white corners, hair under the rim
    "KYYKKWWWWWWKKYYK",
    "KYgYYVVVVVVYYgYK",        # bangs + shadow under the bill, hair full width
    "KYgYYYKYYKYYYgYK",        # eye tops under the bangs
    "KgYopSGSYVSpoYgK",        # green / purple eyes, brown face outline
    "KhYYYoOSSOoYYYhK",        # rounded 4px chin, brown outline, hair round it
    "KhhcoSTOOTSochhK",        # shoulders + neck shadow, brown outline against the hair
]
FRONT_BODY = [                 # rows 25-30 standing
    ".KccOTVTTVTOccK.",
    ".KcSOTTTTTTOScK.",
    "..KSODDDDDDOSK..",
    "...KKDDKKDDKK...",
    "....KpPKKPpK....",
    ".....KK..KK.....",
]
FRONT_WALK = [                 # rows 26-31 (bob 1): left arm forward, right arm back, right foot up
    ".KccOTVTTVTOccK.",
    ".KcSOTTTTTTOScK.",
    "..KSODDDDDDKKK..",
    "...KKDDKKPpK....",
    "....KpPKKKK.....",
    ".....KK.........",
]
for i in (0, 18, 19):
    fr = blank(); put(fr, 10, FRONT_HEAD); put(fr, 25, FRONT_BODY); F[i] = fr
F[3] = blank(); put(F[3], 11, FRONT_HEAD); put(F[3], 26, FRONT_WALK)
F[4] = blank(); put(F[4], 11, FRONT_HEAD); put(F[4], 26, [r[::-1] for r in FRONT_WALK])

# ---------------------------------------------------------------- front run (owner's layout)
RUN_HEAD = [                   # rows 0-12 relative to the hair top
    "...K.KK..KK.K...",
    "..KccchKKhcccK..",
    ".KKhchhcchhchKK.",
    "KOKYhYYhhYYhYKOK",
    "KoKYKKKKKKKKYKoK",
    "KOKKpPPPPPPpKKOK",
    "KoOKPPPPPPPPKOoK",
    ".KOOPPPPPPPPOOK.",
    ".KPPPPPPPPPPPPK.",        # tipped forward: more crown, rounded like the walk cap
    ".KpPPPPPPPPPPpK.",
    ".KppPPPPPPPPppK.",
    ".KYKWWppppWWKYK.",
    "..KYKWWWWWWKYK..",
]
f9 = blank(); put(f9, 7, RUN_HEAD); put(f9, 20, [
    "..KYYYKYYKYYYK..",        # yellow hair under the cap beside the eyes
    "...opSGSYVSpo...",
    "..KSOoSSSSoOSK..",        # fists beside the chin (black outline like Leaf's hands)
    "..KSOKTSSTKOSK..",
    "...KKVTTTTVKK...",
    "...KDDDDDDDDK...",
    "....KDDKKDDK....",
    "....KpPKKPpK....",
    ".....KK..KK.....",
]); F[9] = f9
f10 = blank(); put(f10, 8, RUN_HEAD); put(f10, 21, [
    "KSSKYYKYYKYYYK..",        # raised hand beside the cap (Leaf's placement)
    "KOSKpSGSYVSpo...",
    ".KK.ooSSSSoo....",
    "....oSTSSTSo....",
    "...KTVTTTVSKK...",
    "...KTTTTTKSSK...",
    "....KDDDDKOSK...",
    "....KDDKKKKK....",
    ".....KpPK.......",
    "......KK........",
]); f10[20] = list(".KK.KWWWWWWKYK..")
F[10] = f10
F[11] = [[{'G': 'V', 'V': 'G'}.get(c, c) if y == 22 else c for c in row[::-1]] for y, row in enumerate(f10)]

# ---------------------------------------------------------------- side: palette + brown face outline,
# longer/fuller hair behind her
SIDE_FACE_FIX = {(21, 2): 'o', (22, 3): 'o', (23, 4): 'o'}
for i, bob in ((2, 0), (7, 1), (8, 1), (15, 0), (16, 1), (17, 1)):
    for (y, x), c in SIDE_FACE_FIX.items():
        if F[i][y + bob][x] == 'K':
            F[i][y + bob][x] = c
# side stand/walk: hair one lock longer behind her back (tapered, like Cynthia's), same length in
# every frame so it doesn't jump while walking. Owner's 2x2 hands kept.
put(F[2], 24, [
    "...KVTThhhhhhhK.",
    "....KTTShhhhhhK.",
    "....KTTSSKhcccK.",
    "....KDDOOKKccK..",
    "....KDDKK..KK...",
    "....KPpK........",
    ".....KK.........",
])
put(F[7], 25, [
    "...KVThhhhhhhhK.",
    "....KTTShhhhhhK.",
    "....KDDSSKhcccK.",
    "...KDDKSOKPpccK.",
    "...KPpKKKKKKKK..",
    "....KK..........",
])
put(F[8], 26, [
    "....KTTKSSKhhcK.",
    "....KTTKOOKcccK.",
    "....KDDDKKKccK..",
    "......KPpK.KK...",
    ".......KK.......",
])
F[2][20] = list("..KKKKKKKKKKYYgK")
F[2][21] = list("..oSSKYYYYYgYYYK")
F[2][22] = list("...oSGYYggYYgYYK")
F[2][23] = list("....oSSYYgYYhYYK")

lc.save(F, S + '/frames/normal.txt', '# Lucoa walk/run frames (new palette)\n')
used = {c for fr in F for row in fr for c in row}
print('colours', len(used), sorted(used))
