#!/usr/bin/env bash
# Builds the PC test version of the Wind Waker port: build-pc/tww, a 32-bit
# Linux program (it runs in WSL) with the same game code, platform layer and
# patches as the Vita build. Start-up bugs are much quicker to find here than
# on the Vita; ./run-pc.sh runs it.
#
#   TWW_ISO=/mnt/c/Users/Mikel/Downloads/GZLE01.iso ./build-pc.sh
#       the first time: your disc image, from which the small textures and
#       display lists the game's code embeds are extracted (only once; the
#       image is linked, not copied, into tww/orig/GZLE01/)
#   ./build-pc.sh          later builds (only what changed)
#   ./build-pc.sh --clean  delete build-pc/ first
#
# One-time setup in WSL (32-bit compiler and SDL2):
#   sudo dpkg --add-architecture i386 && sudo apt update && sudo apt install -y gcc-multilib g++-multilib libsdl2-dev:i386 libgl-dev:i386 libegl-dev:i386 cmake ninja-build python3-yaml gdb
set -euo pipefail
cd "$(dirname "$0")"
source tools/fetch.sh

B=build-pc
if [[ "${1:-}" == "--clean" ]]; then
  rm -rf "$B"
fi
for tool in gcc g++ cmake ninja python3 git; do
  command -v "$tool" >/dev/null || { echo "ERROR: $tool is not installed (see the setup line at the top of build-pc.sh)."; exit 1; }
done
echo 'int main(){return 0;}' > /tmp/tww-m32-test.c
gcc -m32 /tmp/tww-m32-test.c -o /tmp/tww-m32-test 2>/dev/null || {
  echo "ERROR: gcc cannot build 32-bit programs: install gcc-multilib g++-multilib (see the top of build-pc.sh)."; exit 1; }
[[ -f /usr/lib/i386-linux-gnu/cmake/SDL2/sdl2-config.cmake || -f /usr/lib/i386-linux-gnu/cmake/SDL2/SDL2Config.cmake ]] || {
  echo "ERROR: 32-bit SDL2 is missing: sudo apt install libsdl2-dev:i386 (see the top of build-pc.sh)."; exit 1; }

fetch_sources
extract_assets

cmake -S . -B "$B" -G Ninja -DTWW_PC=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSDL2_DIR=/usr/lib/i386-linux-gnu/cmake/SDL2 -DCMAKE_LIBRARY_PATH=/usr/lib/i386-linux-gnu \
  -DCMAKE_IGNORE_PATH=/usr/lib/x86_64-linux-gnu \
  -DTWW_PLACEHOLDER_ASSETS="${TWW_PLACEHOLDER_ASSETS:-OFF}" \
  -DTWW_VITA_COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
echo "== Building with ${JOBS:-$(nproc)} jobs (the first build compiles about 1,100 files)"
if ninja -C "$B" -j "${JOBS:-$(nproc)}" 2>&1 | tee "$B/last-build.log"; then
  :
else
  echo "ERROR: the build failed (see $B/last-build.log)."
  exit 1
fi
echo "SUCCESS: $B/tww (run it with ./run-pc.sh)"
