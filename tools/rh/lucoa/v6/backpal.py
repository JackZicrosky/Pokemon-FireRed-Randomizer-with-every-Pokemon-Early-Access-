# Back-pic palette, based on the owner's normal.png (walk sprite) colours: (light, mid, shadow, outline).
# Colours marked * are extra light/shadow tones added next to normal.png's own colour.
O = (48, 36, 48)                                                  # normal.png's outline, used everywhere
PAL = {
    'skin':   [(253, 231, 213), (253, 231, 213), (206, 188, 179), O],
    'cap':    [(252, 196, 200), (246, 163, 170), (222, 110, 136), O],     # light*
    'horn':   [(232, 170, 132), (232, 170, 132), (150, 88, 70), O],
    'white':  [(226, 224, 230), (226, 224, 230), (181, 177, 184), O],
    'tank':   [(86, 80, 102), (66, 60, 78), (56, 48, 64), O],             # light*, shadow*
    'shorts': [(117, 105, 158), (80, 84, 168), (62, 64, 136), O],         # shadow*
    'hair0':  [(248, 250, 190), (238, 241, 133), (196, 200, 88), O],      # yellow, light*
    'hair2':  [(208, 244, 162), (179, 236, 120), (132, 196, 100), O],     # green, light*, shadow*
    'hair4':  [(184, 224, 250), (139, 204, 247), (100, 160, 222), O],     # blue tips, light*, shadow*
}
def hair_band(rgb, x=0, y=0):
    import colorsys
    h, l, s = colorsys.rgb_to_hls(*[v / 255 for v in rgb])
    import math
    deg = h * 360 + 9 * math.sin(x * 1.1 + y * 0.15)      # wavy borders between the colour bands
    return 'hair0' if deg < 76 else 'hair2' if deg < 150 else 'hair4'
