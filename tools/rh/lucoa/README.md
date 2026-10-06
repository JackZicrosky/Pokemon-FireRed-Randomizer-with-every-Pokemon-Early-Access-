# Lucoa player-graphics pack

The 14th choice of **Custom Player Graphics → Player Character** ("Lucoa"). `pack/` has the same files
and format as `graphics/rh_player/<pack>/`, and `tools/rh/gen_player_graphics.py` copies it to
`graphics/rh_player/lucoa/` (it is listed in `LOCAL_PACKS` there).

| File | Content |
|---|---|
| `normal.png` | walk/run, 20 frames of 16x32 |
| `surf.png`, `item.png` | 16x32 frames (item: field move 0-4, VS Seeker 5-8) |
| `bike.png`, `fish.png`, `itembike.png` | 32x32 frames |
| `front.png` | 64x64 trainer pic (trainer card, Oak's intro) |
| `back.png` | 5 x 64x64 battle throw frames |
| `oak.png` | Oak's intro pic: `front.png` on a 64x96 8bpp canvas, colours at 64+i |

The overworld sheets share one 16-colour palette (index 0 transparent); front/back/oak share another.

## Sources

Every sheet is kept as text in `frames/`, one letter per pixel (palettes: `NEW` in `lc.py` for the
overworld, `BPAL` in `bpal.py` for the battle pics). `python3 make_lucoa.py` rebuilds everything:

| Script | Makes |
|---|---|
| `build_normal.py` | `frames/normal.txt` from the owner's sheet `frames/owner_normal.txt`: rounder cap, fuller hair, brown outlines on the face/chest, longer side hair |
| `build_surf_item.py` | surf and item frames, drawn on the walk frames |
| `build_bike.py` | bike + itembike: Lucoa on Leaf's FR/LG bicycle, recoloured pink |
| `build_fish.py` | fishing: Lucoa's own walk-sheet body at Leaf's positions, with Leaf's rod |
| `export_overworld.py` | `pack/` overworld PNGs |
| `build_front.py` | the front pic, hand-placed (left half mirrored) |
| `build_back.py` | the back pic: Leaf's throw poses with Lucoa's cap, horns, hair and bare arms |
| `export_battle.py` | `pack/front.png`, `back.png`, `oak.png` |

`preview/` has overview pictures.
