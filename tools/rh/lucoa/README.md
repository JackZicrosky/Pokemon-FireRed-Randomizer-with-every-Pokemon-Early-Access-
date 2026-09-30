# Lucoa player-graphics pack (work in progress)

Not wired into the game yet. `pack/` has the same files and format as `graphics/rh_player/<pack>/`:

| File | Content |
|---|---|
| `normal.png` | walk/run, 20 frames of 16x32 (owner's hand-edited sheet) |
| `surf.png`, `item.png` | 16x32 frames |
| `bike.png`, `fish.png`, `itembike.png` | 32x32 frames |
| `front.png` | 64x64 trainer pic |
| `back.png` | 5 x 64x64 battle throw |
| `oak.png` | Oak's intro pic, derived from `front.png` |

The overworld sheets share one 16-colour palette (index 0 transparent); front/back/oak have their own.

Sources:
- `lucoa_frames.txt` – master copy of the walk/run sheet, one letter per pixel (owner's edit, `owner_edit.png`).
- `lucoa22.py` – renders `lucoa_frames.txt` (writes `lucoa_normal.png` + walk/run GIFs next to it).
- `lucoa_pack.py` – surf/bike/fish from Leaf's frames with Lucoa's heads; item/itembike hand-drawn on Lucoa's frames.
- `lucoa_battle.py` – front/back/oak recoloured from Leaf's battle pics, horns added.

`preview/` has labelled overviews. Scripts write their PNGs next to themselves; copy the results into `pack/`
(overworld sheets converted to indexed colour with the palette order of `PAL` in `lucoa22.py`).
