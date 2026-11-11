#!/usr/bin/env bash
# Runs the PC test version (./build-pc.sh first) with your disc image, and
# writes everything it prints to tww-pc.log (send me that file). Under gdb,
# when it is installed, so that a crash ends the log with where it happened.
#
#   ./run-pc.sh                 the image the first build used
#   TWW_ISO=/path/x.iso ./run-pc.sh
#   TWW_NO_GDB=1 ./run-pc.sh    without gdb
set -uo pipefail
cd "$(dirname "$0")"
ISO=${TWW_ISO:-$(ls tww/orig/GZLE01/* 2>/dev/null | head -1)}
[[ -n "$ISO" && -e "$ISO" ]] || { echo "ERROR: no disc image: TWW_ISO=/path/to/GZLE01.iso ./run-pc.sh"; exit 1; }
[[ -x build-pc/tww ]] || { echo "ERROR: build it first: ./build-pc.sh"; exit 1; }
ISO=$(realpath "$ISO")
mkdir -p run-pc
cd run-pc
echo "== tww-vita $(git -C .. rev-parse --short HEAD 2>/dev/null), disc $ISO" > ../tww-pc.log
if command -v gdb >/dev/null && [[ -z "${TWW_NO_GDB:-}" ]]; then
  SMS_DISC_IMAGE="$ISO" gdb -q -batch -x ../tools/crash.gdb \
    --args ../build-pc/tww 2>&1 | grep -vE "^\[(New Thread|Thread .* exited)" | tee -a ../tww-pc.log
else
  SMS_DISC_IMAGE="$ISO" ../build-pc/tww 2>&1 | tee -a ../tww-pc.log
fi
echo "== the log is in tww-pc.log"
