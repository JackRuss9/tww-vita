#!/usr/bin/env bash
# Reproducible, bounded PC diagnosis; isolated saves, log and frame captures.
# TWW_PROBE_GDB=/path/commands.gdb enables a debugger script.
set -uo pipefail
probe_save_dir=
probe_audio=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-captures) export SMS_GX_DUMP_EVERY=0; shift ;;
    --audio) probe_audio=1; shift ;;
    --save-dir) probe_save_dir=$(realpath -m "$2"); shift 2 ;;
    --outset) export TWW_START_OUTSET=1; shift ;;
    --deterministic) export SMS_VI_DETERMINISTIC=1; shift ;;
    --headless) export SMS_HEADLESS=1; shift ;;
    --seconds) export TWW_PROBE_SECONDS="$2"; shift 2 ;;
    --gdb) export TWW_PROBE_GDB="$2"; shift 2 ;;
    --input) export SMS_AUTOPRESS="$2"; shift 2 ;;
    *) echo "Unknown option: $1"; exit 2 ;;
  esac
done
cd "$(dirname "$0")/.."
root=$PWD
iso=${TWW_ISO:-$root/tww/orig/GZLE01/GZLE01.ciso}
[[ -f "$iso" && -x build-pc/tww ]] || { echo 'Missing disc or PC executable'; exit 1; }
iso=$(realpath "$iso")
mkdir -p run-pc
out=$(mktemp -d "$root/run-pc/probe-XXXXXX")
echo "Probe artifacts: $out"
export SMS_DISC_IMAGE="$iso" SMS_SAVE_DIR="${probe_save_dir:-$out/card}" SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"
if [[ "$probe_audio" == 1 ]]; then
  # Use the real host device, overriding the diagnostic dummy default.
  unset SDL_AUDIODRIVER
  export SMS_AUDIO=1 SMS_AUDIO_OUT=sdl SMS_AUDIO_STATS=1 SMS_AUDIO_WAV="$out/audio.wav"
fi
export SMS_GX_DUMP_DIR="$out" SMS_GX_DUMP_EVERY="${SMS_GX_DUMP_EVERY:-300}"
cd "$root/run-pc"
if [[ -z "${SMS_BINDINGS:-}" && ! -f bindings.txt ]]; then
  export SMS_BINDINGS="$root/config/pc-bindings.txt"
fi
if [[ -n "${TWW_PROBE_GDB:-}" ]]; then
  timeout --signal=INT --kill-after=10s "${TWW_PROBE_SECONDS:-90}s" gdb -q -batch --return-child-result -x "$TWW_PROBE_GDB" --args "$root/build-pc/tww" > "$out/run.log" 2>&1
else
  timeout --kill-after=5s "${TWW_PROBE_SECONDS:-90}s" "$root/build-pc/tww" > "$out/run.log" 2>&1
fi
status=$?
echo "$status" > "$out/status"
echo "Probe ended with status $status (124 means the time limit). Log: $out/run.log"
exit "$status"
