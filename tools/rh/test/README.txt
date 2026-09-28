Romhack test tools (from the previous Claude session). Suggested home in the repo: tools/rh/test/
- harness.c   headless mGBA runner. Build: apt install libmgba-dev libpng-dev; gcc -O2 harness.c -o harness -lmgba -lpng
              Usage: ./harness ROM script.txt [save.sav]   (commands listed at the top of the file / in CLAUDE.md §5b)
- grid.py     contact sheet: python3 grid.py out.png a.png b.png ...
- run_st.sh   runs the in-ROM self-test and prints gRhSelfTestLog (edit ELF path; uses boot_rt1.txt first 7 lines)
- boot_rt1.txt  boot sequence; its first 7 lines reach the field in a quickstart (RH_TEST_MAP) build
