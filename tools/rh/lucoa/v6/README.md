# Round 6: back picture from the 3D model

Made with Blender (`pip install bpy`) from the owner's rigged Lucoa model, which is not stored in the repo.
Put it at `model/01- Quetzalcoatl Lucoa/` and run from the folder above `m3d/`, with these scripts in `m3d/`:

    for m in flat lit; do YAW=215 SHX=-0.12 SUNZ=160 TW=15,22,-28,-30 CURL=55 python3 m3d/pose.py $m; done
    FIST=red REDCFG='{"1": [[7,48], "flipv", [0,46,8,58]], "2": [[11,17], "none", [3,5,17,17]]}' python3 m3d/convert.py

- `pose.py` poses the Mixamo skeleton in Red's five throw poses (left arm throws), enlarges the horns,
  thickens the arms a little and renders a flat-colour pass and a lit pass from behind (512px).
- `convert.py` turns each render into 64x64: material by majority vote, tone from the lit pass, then
  outlines, horn rings, wavy hair colour bands and the cap's snapback opening.
- `backpal.py` is the palette, based on the owner's normal.png.
- `red_hands.py` puts Red's FR/LG hands (cut at his wristband, recoloured to Lucoa's skin) on frames 2 and 3,
  with a few joining pixels at the wrist. (`hands_manual.py`/`fist.py` = earlier attempts, unused.)

`back_sheet.png` = the 5 frames stacked (64x320). Not in the game yet.
