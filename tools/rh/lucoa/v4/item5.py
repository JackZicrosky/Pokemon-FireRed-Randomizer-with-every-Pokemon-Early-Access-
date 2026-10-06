# Lucoa item frames (field move 0-4, VS Seeker 5-8), after Leaf's motion: looks down and reaches for her
# pocket, bobs, brings the ball/Seeker up, raises it high with her head tilted up.
import sys
sys.path.insert(0, sys.argv[1])
from owpal import *
S = sys.argv[1]
U = load(S + '/owner_normal2.txt')
BASE = U[0]
def cp(g): return [r[:] for r in g]
def put(g, y0, x0, rows):
    for j, r in enumerate(rows):
        for i, c in enumerate(r):
            if c != ' ' and 0 <= y0 + j < 32 and 0 <= x0 + i < 16: g[y0 + j][x0 + i] = c
def blank(): return [['.'] * 16 for _ in range(32)]
HEAD = cp(BASE[10:25])
HEAD_DOWN = cp(HEAD)                     # looking down at her pocket: eyes closed, face turned a little
HEAD_DOWN[11] = list("KYgYYYYYYYYYYgYK")
HEAD_DOWN[12] = list("KgYopSKKSKKSoYgK")
HEAD_UP = cp(HEAD)                       # looking up at the raised item: brim shows more, chin lifted
HEAD_UP[11] = list("KYgYYKYYYYKYYgYK")
STAND = [r[:] for r in BASE[25:31]]
POCKET = [list(r) for r in [".KccOTVTTVTOKcK.", ".KcSOTTTTTTKSK..", "..KSODDDDDSOK...", "...KKDDKKDDKK...",
                             "....KpPKKPpK....", ".....KK..KK....."]]
LUP = [list(r) for r in [".KcKKTVTTVTOccK.", ".KcKTTTTTTTOScK.", "..KKDDDDDDDOSK..", "...KKDDKKDDKK...",
                          "....KpPKKPpK....", ".....KK..KK....."]]
BALL = [".KKK.", "KrrrK", "KKWKK", "KWWWK", ".KKK."]
SEEK = ["KKKK", "KWpK", "KWWK", "KAAK", "KKKK"]
SEEK_A = ["KKKK", "KcWK", "KWWK", "KAAK", "KKKK"]
SEEK_B = ["KKKK", "KWcK", "KcWK", "KAAK", "KKKK"]
def frame(head, body, dy=0, lift=0):
    g = blank()
    put(g, 10 + dy - lift, 0, head)
    if lift: put(g, 24 + dy, 0, [head[13]])      # longer neck when she looks up
    put(g, 25 + dy, 0, body)
    return g
def arm_up(g, top, x=1):
    """raised forearm (hand at row top) bending out at the elbow, upper arm back to the shoulder"""
    mid = (top + 24) // 2
    for y in range(top, mid + 1): put(g, y, 0, ["KSOK"])          # forearm, hand at the top
    put(g, mid + 1, 0, ["KSSOK"])                                   # elbow
    for y in range(mid + 2, 25): put(g, y, 1, ["KSOK"])            # upper arm to the shoulder
F = []
f = frame(HEAD_DOWN, POCKET); F.append(f)                         # 0 looks down, hand to her pocket
f = frame(HEAD_DOWN, POCKET[:4] + [list("....KpPKKPpK....")], dy=1)  # 1 bob down, hand in the pocket
F.append(f)
f = frame(HEAD, STAND); put(f, 22, 11, BALL); put(f, 27, 12, ["KSSK"]); F.append(f)   # 2 ball at her shoulder
f = frame(HEAD, LUP); arm_up(f, 21); put(f, 16, 0, BALL); F.append(f)                 # 3 ball raised
f = frame(HEAD_UP, LUP, lift=1); arm_up(f, 16); put(f, 11, 0, BALL); F.append(f)       # 4 held up high
f = frame(HEAD_DOWN, POCKET); put(f, 25, 12, SEEK); F.append(f)                        # 5 Seeker out
f = frame(HEAD, LUP); arm_up(f, 22); put(f, 17, 0, SEEK); F.append(f)                  # 6 chest high
f = frame(HEAD_UP, LUP, lift=1); arm_up(f, 16); put(f, 11, 0, SEEK_A); F.append(f)     # 7 raised, flashing
f = frame(HEAD_UP, LUP, lift=1); arm_up(f, 16); put(f, 11, 0, SEEK_B); F.append(f)     # 8
save(F, S + '/item5.txt', '# item\n')
from PIL import Image
im = Image.open('' + __import__('os').path.abspath(__import__('os').path.dirname(__file__) + '/../../../..') + '/graphics/rh_player/leaf/item.png')
rg = im.convert('RGBA'); px = rg.load()
for y in range(im.height):
    for x in range(im.width):
        if im.getpixel((x, y)) == 0: px[x, y] = (0, 0, 0, 0)
strip([F, [rg.crop((i * 16, 0, i * 16 + 16, 32)) for i in range(9)]], S + '/iz.png', 8)
