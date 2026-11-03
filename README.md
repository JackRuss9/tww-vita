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

## Builds

| | Command | Status |
|---|---|---|
| PC test version (WSL, 32-bit Linux) | `TWW_ISO=/path/GZLE01.iso ./build-pc.sh`, then `./run-pc.sh` | builds, starts; bring-up |
| PS Vita | `./build.sh` | next |

See the top of `build-pc.sh` for the one-time setup.

## Layout

- `tools/fetch.sh` downloads tww and sms-port at pinned commits and applies
  `patches/tww/*.patch` and `patches/sms-port/*.patch` (each with a `Reason:` line).
- `cmake/tww_game.cmake` lists the game's sources (`tools/gen_sources.py`, from
  tww's `configure.py`) and how they are compiled.
- `port/` is this port's own code: the compiler stand-ins (`port/include`) and the
  SDK functions the Sunshine platform layer did not have (`port/src`).
- `check/` is the fast compile check: `cmake -S check -B build-check -G Ninja && ninja -C build-check -k 0`.
- `NOTES.md` records the state of the port, measurements and decisions.
