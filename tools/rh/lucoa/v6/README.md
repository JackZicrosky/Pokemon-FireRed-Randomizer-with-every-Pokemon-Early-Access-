# Round 6: back picture from the 3D model

Made with Blender (`pip install bpy`) from the owner's rigged Lucoa model, which is not stored in the repo.
Put it at `model/01- Quetzalcoatl Lucoa/` and run from the folder above `m3d/`, with these scripts in `m3d/`:

    for m in flat lit; do python3 m3d/pose.py $m; done
    python3 m3d/convert.py

- `pose.py` poses the Mixamo skeleton in Red's five throw poses (left arm throws, right arm like Red's, leaning forward), enlarges the horns,
  thickens the arms a little and renders a flat-colour pass and a lit pass from behind (512px).
- `convert.py` turns each render into 64x64: material by majority vote, tone from the lit pass, then
  outlines, horn rings, wavy hair colour bands and the cap's snapback opening.
- `bust.py` redraws the bust in frames 4-5 as a round shaded shape (the low-poly model renders it as a cone)
  and fills any enclosed holes.
- `backpal.py` is the palette, based on the owner's normal.png.
- `red_hands_small.py` = Red's FR/LG hands from his frames 2 and 3, redrawn at ~70% to fit Lucoa's arms
  (frames 2, 3 and 4). `red_hands.py` holds the shared colours.
- (older) `red_hands.py` put Red's FR/LG hands (cut at his wristband, recoloured to Lucoa's skin) on frames 2 and 3,
  with a few joining pixels at the wrist. (`hands_manual.py`/`fist.py` = earlier attempts, unused.)

`back_sheet.png` = the 5 frames stacked (64x320). Not in the game yet.
