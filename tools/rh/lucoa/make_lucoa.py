#!/usr/bin/env python3
"""Rebuilds the whole Lucoa player-graphics pack: frames/*.txt (one letter per pixel) and pack/*.png.

Order: walk/run sheet (from the owner's sheet), surf + item, bike + itembike, fish, export of the overworld
sheets, then the battle front/back pics and Oak's intro pic. Copy pack/*.png to graphics/rh_player/lucoa/."""
import os, runpy

HERE = os.path.dirname(os.path.abspath(__file__))
for step in ('build_normal', 'build_surf_item', 'build_bike', 'build_fish', 'export_overworld',
             'build_front', 'build_back', 'export_battle'):
    print('--', step)
    runpy.run_path(os.path.join(HERE, step + '.py'), run_name='__main__')
