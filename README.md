# tww-vita

*The Legend of Zelda: The Wind Waker* (GameCube, USA, **GZLE01 Rev 0**) for the PS Vita:
the [zeldaret/tww](https://github.com/zeldaret/tww) decompilation, built on the
platform layer of the Super Mario Sunshine PC port
([chasem-dev/sms-pc-port](https://github.com/chasem-dev/sms-pc-port)) with the
changes made for [sms-vita](NOTES.md).

**Work in progress.** The decompilation is not finished: about 14% of the
game's code (mostly actors: some enemies, bosses and NPCs) is still empty
functions. The port follows it as it fills in (`TWW_COMMIT` in
`tools/fetch.sh`).

No game data is included. You need your own disc image.

See [PROGRESS.md](PROGRESS.md) for verified PC milestones, reproducible tests
and known blockers (Spanish). Normal startup through the complete opening to controlling Link is verified.
Manual play now reaches Tetra's rescue, Aryll's abduction, the beach conversation,
obtaining the shield and, in an automated replay from that earned state, the
complete pirate departure cinematic up to control on the pirate ship. The full
island from the menu in one run is not verified yet; see PROGRESS.md.
Full Outset story progression and uninterrupted audio are not verified yet.
Grandma's house loading and the Hero's Clothes scene also pass isolated PC
tests, including movement after the scene. Aryll's telescope delivery and
subsequent control return also pass an isolated test. Special conversation
cameras remain incomplete; see the progress log for limits.

## Builds

| | Command | Status |
|---|---|---|
| PC test version (Linux / WSL, 32-bit) | `TWW_ISO=/path/GZLE01.iso ./build-pc.sh`, then `./run-pc.sh` | normal startup, full opening and first Link movement verified; island story work continues |
| PS Vita | not implemented yet | deferred until PC works |

See the top of `build-pc.sh` for the one-time Ubuntu / WSL setup. A working
X11/WSLg display and 32-bit OpenGL driver are required to run the window.
Software rendering (llvmpipe) works for debugging but can be slow.

```sh
TWW_ISO=/absolute/path/GZLE01.ciso ./build-pc.sh
./run-pc.sh
```

The first build extracts the embedded assets from GZLE01 Rev 0. Subsequent
builds reuse them. `TWW_PLACEHOLDER_ASSETS=ON` is for compile/link checks;
it does not provide the assets needed for gameplay.

For PC bring-up, `./run-pc.sh --outset` starts a fresh in-memory game at
Outset Island's beach spawn (point 204), bypassing file selection and the story opening.
The entrance cutscene is omitted in this route; later story event orders
are allowed. This is a diagnostic start, not a saved-game load or a verified
complete story mode. Actors whose creation
code is still absent in the decompilation are omitted; they would otherwise
keep the whole room loading forever. Their profiles are generated at CMake
configure time by `tools/gen_missing_actors.py`.

The normal follow camera and L recentering are available. A PC C-stick orbit
camera supports rotation and tilt around Link, with zone-specific limits and
the existing collision handling. Retail vertical zoom/FOV coupling and special
combat/vehicle camera behavior are not fully reconstructed.

Keyboard defaults: Enter = Start, Space = A, Shift = B, WASD = left stick
and menu navigation, arrow keys (or IJKL) = C-stick camera, Q / E = L / R,
V / F = X / Y, Escape = quit. Hold left Ctrl with WASD for half stick
deflection (slower walking). Hold left Alt with WASD for fine tilt
(`SMS_FINE_TILT`, default 25%): the telescope then turns at about 17% of full
speed, which makes precise aiming possible from the keyboard. Under WSL2,
controllers reach the game only through `usbipd-win`; Xbox/XInput-mode
controllers are not usable there because the WSL kernel has no `xpad` driver. The runner loads `config/pc-bindings.txt` unless
`SMS_BINDINGS` or a local `run-pc/bindings.txt` supplies a custom layout.
Saves made through `run-pc.sh` are kept in `run-pc/card-a` (override with
`SMS_SAVE_DIR`). The runner writes `tww-pc.log`; `TWW_NO_GDB=1` disables gdb.

Local edits in `tww/` and `sms-port/` are preserved. If the patch set changes
and a dependency checkout has edits, the build stops without replacing it.
Save those edits and move the checkout aside before fetching a fresh copy;
keep `tww/orig/` and the extracted `tww/build/GZLE01/` assets to avoid
extracting them again. Port fixes must also live in `patches/`, because the
dependency directories are ignored by this repository.

This is still an incomplete port: some game functions and sound effects are
missing in the pinned decompilation. Reaching a menu does not establish that
a full playthrough, sound or saving works.

## Layout

- `tools/fetch.sh` downloads tww and sms-port at pinned commits and applies
  `patches/tww/*.patch` and `patches/sms-port/*.patch` (each with a `Reason:` line).
- `cmake/tww_game.cmake` lists the game's sources (`tools/gen_sources.py`, from
  tww's `configure.py`) and how they are compiled.
- `port/` is this port's own code: the compiler stand-ins (`port/include`) and the
  SDK functions the Sunshine platform layer did not have (`port/src`).
- `check/` is the fast compile check: `cmake -S check -B build-check -G Ninja && ninja -C build-check -k 0`.
- `NOTES.md` records the state of the port, measurements and decisions.

## PC diagnostics

`tools/probe-pc.sh` runs a bounded test with isolated saves and captures in
`run-pc/probe-*`. For example, from the repository root:

```sh
./tools/probe-pc.sh --outset --seconds 75 --gdb "$PWD/tools/movement.gdb" \
  --input stick_down@1200+120,l@1500+30,stick_left@1800+120
```

Probes use dummy audio by default. Add `--audio` for audible SDL output and
an `audio.wav` recording in the probe directory. For example:

```sh
./tools/probe-pc.sh --audio --seconds 360 --gdb "$PWD/tools/normal_flow.gdb"
```

`normal_flow.gdb` navigates the title, save selection and name entry using
virtual buttons, then lets the complete illustrated opening play. It logs
the island's opening event and checks whether control returns to Link.
`--deterministic` accelerates offline diagnostics and disables the audio
device; omit it for an audible test. `--save-dir PATH` reuses an explicit
test card instead of creating a new isolated one.

Add `--headless` for EGL without a window. Status 124 denotes the requested
timeout; inspect the log and captures for actual progress. `movement.gdb`
records player positions, ground height, movement state, input and demo state.
The numerical regression checks need no display:

```sh
python3 check/test_animation.py
python3 check/test_jstudio.py
g++ -std=c++11 -O2 -Iport/include check/test_timing.cpp -o /tmp/tww-timing-test
/tmp/tww-timing-test
```
