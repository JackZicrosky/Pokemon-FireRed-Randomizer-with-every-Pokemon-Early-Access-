# Write the overworld sheets as 16-colour indexed PNGs (index 0 transparent, shared palette).
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lc
from PIL import Image
S = os.path.dirname(os.path.abspath(__file__)); OUT = S + '/pack'
ORDER = '.KTVYghcSOopPWDG'
pal = []
for ch in ORDER:
    c = lc.NEW[ch][:3] if ch != '.' else (115, 197, 164)      # index 0: unused transparent colour
    pal += list(c)
SHEETS = [('normal', S + '/frames/normal.txt', 16), ('surf', S + '/frames/surf.txt', 16),
          ('item', S + '/frames/item.txt', 16), ('bike', S + '/frames/bike.txt', 32),
          ('itembike', S + '/frames/itembike.txt', 32), ('fish', S + '/frames/fish.txt', 32)]
COUNT = {'normal': 20, 'surf': 3, 'item': 9, 'bike': 9, 'itembike': 6, 'fish': 12}
for name, txt, fw in SHEETS:
    F = lc.load(txt)
    assert len(F) == COUNT[name], (name, len(F))
    im = Image.new('P', (fw * len(F), 32))
    im.putpalette(pal + [0] * (768 - len(pal)))
    for i, fr in enumerate(F):
        assert len(fr) == 32 and all(len(r) == fw for r in fr), (name, i)
        for y, row in enumerate(fr):
            for x, ch in enumerate(row):
                im.putpixel((i * fw + x, y), ORDER.index(ch))
    im.save(OUT + '/' + name + '.png', transparency=0)
    print(name, im.size)
