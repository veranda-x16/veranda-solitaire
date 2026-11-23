#!/bin/bash
# run.sh CVT DIR SECONDS: the CVT as AUTOSTART.cvt in DIR (a host folder),
# headless and warped, at most SECONDS; console.txt and dump.bin (VRAM) there
# (EMU, ROM, SDK from the environment; EXTRA: more files to put there)
set -e
cvt=$1; d=$2; secs=$3
mkdir -p "$d"
cp "$cvt" "$d/AUTOSTART.cvt"
cp "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$d/"
for f in $EXTRA; do cp "$f" "$d/"; done
(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT "$secs" "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
	-warp -sound none -echo -dump V > console.txt 2>&1) || true
