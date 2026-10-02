#!/bin/bash
# Run the in-ROM self-test and print its log.
# Needs a DEBUG build with include/config/rh_test.h set to RH_TEST_EXTRA RH_SelfTest() (+ RH_TEST_MAP etc.).
# Usage: ROM=path/pokefirered.gba ELF=path/pokefirered.elf bash run_st.sh [steps]   (steps of 600 frames, default 120)
ROM=${ROM:-../../../pokefirered.gba}
ELF=${ELF:-../../../pokefirered.elf}
HERE=$(cd "$(dirname "$0")" && pwd)
ADDR=$(arm-none-eabi-nm "$ELF" | awk '/ gRhSelfTestLog$/{print $1}')
DONE=$(arm-none-eabi-nm "$ELF" | awk '/ gRhSelfTestDone$/{print $1}')
N=${1:-120}
(head -7 "$HERE/boot_rt1.txt"; for i in $(seq 1 $N); do echo "wait 600"; echo "peek32 $DONE"; done; echo "dumpstr *$ADDR 5120") > /tmp/st.txt
"${HARNESS:-$HERE/harness}" "$ROM" /tmp/st.txt > /tmp/st.out
grep -m1 -n "=00c0ffee" /tmp/st.out | cut -d: -f1 | awk '{print "done after ~" ($1-0)*600 " frames"}'
grep -v "^0203\|^0200" /tmp/st.out
