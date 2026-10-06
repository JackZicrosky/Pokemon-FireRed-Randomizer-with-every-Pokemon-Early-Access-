# Round 5: front + Oak picture

`pixel_art_1_hq.png` is the owner's high-quality "Lucoa Pixel Art 1" (1440x1440, 12px cells).
`hq_native.png` holds its real 120x120 pixels. `front10.py` shrinks them to exactly half, picking
each pixel's colour from the reference's own palette (`hqpal.py`), with no blending. It then puts back
the green eye, which the reduction blurred, and writes `front10.png` (64x64), `oak10.png` (64x96) and
`front10.txt`. These are 26 colours for now; they get cut to 16 before going in the game.
