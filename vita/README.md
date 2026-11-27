# SwitchWaker on Vita

The 01.15 startup probe runs SwitchWaker's `pc::loadAssets()` against the Vita
CISO reader. It checks the DOL and RELs against the USA revision 0 hashes,
loads the 163 arrays into memory and fills an array registered by the probe.
After Aurora's OSInit it enters SwitchWaker's `main01()`. The early Vita
build uses that function's `mDoMch_Create()` path to configure JFW and
initialize the command, archive, game and Zelda heaps. It records
`vita-awaiting-gx-backend` and returns before graphics initialization.
It uses SwitchWaker's OS SDK to run a worker thread
that allocates from the shared JKR heap, post messages from the worker
and a timer callback, and join the worker. It exercises the game's JKRDvdFile
with both path and FST entry opens for the title and ocean archives.
The Vita DVD adapter completes those reads inline and delivers the result
through JKRDvdFile's existing message queue. For compressed archives it
compares the full output of JKRDecomp with the reference asset loader.
The ARAM check allocates a 16 MiB native memory block as auxiliary RAM and
verifies an upload and readback through Aurora's ARQ path.
It also runs the game's JUTGamePad input reader on the Vita's first pad.
Cross, Circle, Square and Triangle map to A, B, X and Y respectively.
Select maps to Z, and the shoulder buttons map to L and R.
The suite also expands the title and ocean RARC archives through the upstream
loader, checks a GXM texture render and framebuffer copy, and displays the
logo texture from the title actor's archive through GXM. If that texture
cannot be rendered, the probe uses the disc banner when available.

## Build and run the hardware probe

Use Ubuntu with VitaSDK, CMake, Ninja and Python with PyYAML. Check out
[SwitchWaker `1080514`](https://github.com/centollOS/SwitchWaker/tree/108051434f2f67437947e78015b3dd409be6e1c6)
and [Aurora `3227d76`](https://github.com/encounter/aurora/tree/3227d76c60e1e782ca576610bce61c9e7744d8be).
Set `SW` and `AURORA` to their absolute paths and `STUBS` to an absolute
directory for the generated headers. From the tww-vita repository root:

```sh
git -C "$SW" apply "$PWD/vita/patches/switchwaker-vita-libc.patch" "$PWD/vita/patches/switchwaker-arm32.patch" "$PWD/vita/patches/switchwaker-vita-suite.patch"
git -C "$AURORA" apply "$PWD/vita/patches/aurora-vita-boot.patch"
(cd "$SW" && bash native/tools/gen_assets.sh --stubs --version GZLE01 --out "$STUBS")
cmake -S vita/probe -B build-vita-probe -G Ninja -DCMAKE_BUILD_TYPE=Release -DTWW_SWITCHWAKER_DIR="$SW" -DTWW_AURORA_DIR="$AURORA" -DTWW_SWITCHWAKER_ASSETS="$STUBS" && cmake --build build-vita-probe
```

Set `VITASDK` to the SDK installation directory. Install
`build-vita-probe/tww-vita-probe.vpk` in VitaShell and open **TWW Vita Probe**.
Expect the title logo on black with a green status strip, or a red strip
if a check reports an error. Move both sticks and try the buttons before
pressing START. The app logs the observed stick ranges and button mask,
joins the OS alarm thread, then exits.
Collect `ux0:/data/tww/probe.log` before running the probe again.

The graphics test embeds the MIT-licensed texture shaders from
[libvita2d](https://github.com/xerpi/libvita2d/tree/a8f15ab09d5233f0a4e4ad0e8f6ade0da888cbed/libvita2d/shader).

Keep your USA revision 0 GameCube CISO at `ux0:/data/tww/WindWaker.ciso`.
The log identifies each check with BEGIN, PASS, FAIL or SKIP. The summary
is `check: suite PASS` when all checks succeed. A crash may leave a BEGIN
without a result. The probe covers the machine initialization for heaps, file access and input.

## Cross-compile the game startup modules

Configure the same build with `-DTWW_BUILD_GAME_OBJECTS=ON`. Then run:

```sh
cmake --build build-vita-probe --target switchwaker_boot_link_check
```

This compiles the upstream `m_Do`, `framework` and `JSystem-core`
modules and partially links them as `switchwaker-boot-arm.o`. The
undefined symbol list includes calls to other game modules and platform
interfaces such as GX and VI. The full game sources are available as
the `cos_modules` target. The diagnostic VPK remains a separate target.

For the complete ARM object set, apply `switchwaker-vita-gcc.patch`
to SwitchWaker and use the `switchwaker_all_link_check` target. Set
`TWW_GXM_AURORA_DIR` to the GXM fork when checking that backend. This
selects its GX texture and palette layouts for the game objects.

## Runtime DVD service

Apply `vita/patches/aurora-vita-dvd.patch` to the pinned Aurora checkout
and configure with `-DTWW_BUILD_RUNTIME_DVD=ON`. Build the
`tww_vita_dvd` target. This archive uses Aurora's worker, directory
traversal, and DVD callbacks over the Vita CISO stream. The diagnostic
executable uses its own inline adapter. The game executable should link
the runtime service in its place.

The game links `tww_vita_card` to expose a persistent virtual Slot A through
the normal CARD API. Slot B is empty.
The `tww_vita_sdk` target compiles the SwitchWaker SDK with a muted audio adapter.

## GXM renderer integration

Apply `mh3-aurora-vita-gxm.patch` at the root of the Monster Hunter Tri port
checkout. Use its `aurora-main/` directory for the renderer build and
`TWW_GXM_AURORA_DIR`. For a GCC Vita build, configure the renderer with
`-DCMAKE_C_FLAGS=-fno-short-enums -DCMAKE_CXX_FLAGS=-fno-short-enums`.
Aurora, SDL and the game callers must agree on enum width.

`TWW_GXM_RUNTIME_SHADERS=ON` selects runtime compilation with vitaShaRK.
`TWW_VITASHARK_PREFIX` points to the dependency prefix with the shader
compiler libraries and headers. This configuration supplies the Aurora
utility shaders and caches generated programs in `ux0:/data/tww/shaders`.
It uses the Vita's shader compiler module at runtime. The diagnostic VPK
build does not use this renderer.

## Build the game VPK

With the probe's three SwitchWaker patches already applied, apply the remaining
Vita patches. Set `GXM` to the absolute path of the patched Monster Hunter Tri
checkout's `aurora-main` directory and `SHARK` to a prefix with VitaShaRK,
SceShaccCgExt and taiHEN stubs. Use a VitaSDK with GNU ld support for
`__sce_headroom`, such as the October 2026 VitaSDK.

```sh
git -C "$SW" apply "$PWD/vita/patches/switchwaker-vita-gcc.patch" "$PWD/vita/patches/switchwaker-vita-gx-ownership.patch" "$PWD/vita/patches/switchwaker-vita-title-log.patch" "$PWD/vita/patches/switchwaker-vita-first-painter.patch" "$PWD/vita/patches/switchwaker-vita-frame-tail.patch" "$PWD/vita/patches/switchwaker-vita-muted-opening.patch" "$PWD/vita/patches/switchwaker-vita-muted-waves.patch" "$PWD/vita/patches/switchwaker-vita-muted-listener.patch" "$PWD/vita/patches/switchwaker-vita-muted-ambience.patch" "$PWD/vita/patches/switchwaker-vita-muted-playback.patch" "$PWD/vita/patches/switchwaker-vita-muted-animation.patch" "$PWD/vita/patches/switchwaker-vita-title-sparkle.patch" "$PWD/vita/patches/switchwaker-vita-name-heap-trace.patch" "$PWD/vita/patches/switchwaker-vita-card-commit.patch" "$PWD/vita/patches/switchwaker-vita-card-poll.patch" "$PWD/vita/patches/switchwaker-vita-minimap-alpha.patch" "$PWD/vita/patches/switchwaker-vita-icon-trace.patch" "$PWD/vita/patches/switchwaker-vita-fast-grass.patch" "$PWD/vita/patches/switchwaker-vita-gx-sync.patch" "$PWD/vita/patches/switchwaker-vita-audio-option.patch" "$PWD/vita/patches/switchwaker-vita-title-sparkle-option.patch" "$PWD/vita/patches/switchwaker-vita-icon-draw-trace.patch" "$PWD/vita/patches/switchwaker-vita-draw-distance.patch"
git -C "$AURORA" apply "$PWD/vita/patches/aurora-vita-dvd.patch"
git -C "$GXM/.." apply "$PWD/vita/patches/mh3-aurora-vita-gxm.patch" "$PWD/vita/patches/mh3-aurora-public-gx.patch" "$PWD/vita/patches/mh3-aurora-public-gx-headers.patch" "$PWD/vita/patches/mh3-aurora-vita-fatal-log.patch" "$PWD/vita/patches/mh3-aurora-vita-recorded-gx.patch" "$PWD/vita/patches/mh3-aurora-vita-packed-shader-fetch.patch" "$PWD/vita/patches/mh3-aurora-vita-host-alloc.patch" "$PWD/vita/patches/mh3-aurora-vita-present-probe.patch" "$PWD/vita/patches/mh3-aurora-vita-present-efb.patch" "$PWD/vita/patches/mh3-aurora-vita-title-compiler.patch" "$PWD/vita/patches/mh3-aurora-vita-copy-host-alloc.patch" "$PWD/vita/patches/mh3-aurora-vita-outset-compiler.patch" "$PWD/vita/patches/mh3-aurora-vita-fractional-dst-alpha.patch" "$PWD/vita/patches/mh3-aurora-vita-fps-and-swim-compiler.patch" "$PWD/vita/patches/mh3-aurora-vita-palette-and-efb.patch" "$PWD/vita/patches/mh3-aurora-vita-toon-float-tev-and-scale.patch" "$PWD/vita/patches/mh3-aurora-vita-settings-menu.patch" "$PWD/vita/patches/mh3-aurora-vita-safe-vertex-compiler.patch" "$PWD/vita/patches/mh3-aurora-vita-param-cache-and-icon-trace.patch" "$PWD/vita/patches/mh3-aurora-vita-icon-palette-trace.patch" "$PWD/vita/patches/mh3-aurora-vita-gx-thread.patch"
cmake -S "$GXM" -B build-vita-gxm -G Ninja -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-fno-short-enums -DCMAKE_CXX_FLAGS=-fno-short-enums -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_BZIP2=ON -DAURORA_ENABLE_CARD=OFF -DAURORA_ENABLE_DVD=OFF -DAURORA_SDL3_PROVIDER=vendor -DAURORA_SDL3_LINKAGE=static -DTWW_GXM_RUNTIME_SHADERS=ON -DTWW_VITASHARK_PREFIX="$SHARK"
cmake --build build-vita-gxm --target aurora_gx aurora_gd aurora_os aurora_mtx aurora_vi aurora_ms
cmake -S vita/probe -B build-vita-game -G Ninja -DCMAKE_BUILD_TYPE=Release -DTWW_SWITCHWAKER_DIR="$SW" -DTWW_AURORA_DIR="$AURORA" -DTWW_SWITCHWAKER_ASSETS="$STUBS" -DTWW_BUILD_GAME_OBJECTS=ON -DTWW_BUILD_RUNTIME_DVD=ON -DTWW_GXM_AURORA_DIR="$GXM" -DTWW_GXM_BUILD_DIR="$PWD/build-vita-gxm" -DTWW_VITASHARK_PREFIX="$SHARK"
cmake --build build-vita-game --target switchwaker-vita.vpk
```

Install `build-vita-game/switchwaker-vita.vpk` with VitaShell. It uses the
probe's title ID, so it updates that LiveArea bubble. Keep the original
USA revision 0 CISO at `ux0:/data/tww/WindWaker.ciso` and have
`libshacccg.suprx` available for shader compilation. The VPK reads the game's
resources from the CISO.

The Vita executable starts SwitchWaker's `aurora_main` with runtime DVD and
GXM. It writes `ux0:/data/tww/boot.log` and flushes the boot checkpoints.
The log records GXM initialization, admission and presentation of the first game frame, the logo and
opening scenes, the title draw and the file-select screen if reached.
The 01.41 build runs muted, skips the opening legend and provides a persistent
virtual card in Slot A. The GXM readback path for the Picto Box still needs
implementation.
On real hardware, 01.16 initialized GXM, compiled the initial utility
shaders, loaded the 163 disc asset arrays and entered `main01`. The crash
dump records `Aurora GX PANIC: GXSetDrawDoneCallback` during JFW startup,
before the first game frame. The `shaders` directory is the runtime
compiler cache.

The Monster Hunter Tri revision used for 01.16 intentionally aborts on
the public GX/FIFO interface. Build 01.17 retains that GXM renderer and
restores the public GX entry points and FIFO decoding from
[Grarak/mh3-wii `31301ed`](https://github.com/Grarak/mh3-wii/tree/31301edbebc4b629fce35b2303fda279554320b6/aurora-main).
It also restores texture copies from
[Grarak/mh3-wii `37c57f5`](https://github.com/Grarak/mh3-wii/tree/37c57f5a32556e089d513aee8f3766a86bf6c8a6/aurora-main)
and drains pending GX commands before sealing each frame. The complete
01.17 hardware run reached `GXSetTexCoordGen`, where an inline
compatibility wrapper still aborted. Build 01.18 restores that wrapper,
uses the public vertex entry points whenever compiling for Vita, and
writes the VPK version to the boot log.

Hardware build 01.20 completed graphics and scene initialization, followed by
J3D setup in the first painter pass. Its log ended as the 2D pass began.
Build 01.21 traces the 2D pass through draw lists, fader, FIFO flush, and
first-frame submission. Direct GXM failures now reach the boot log.

Hardware build 01.21 completed the 2D draw lists, then stopped when the Vita
compiler rejected vertex shader `1343e017f28d2c63_v` during the first fade.
Build 01.22 supplies the recorded GX vertex and array buffers to GXM.
It also logs the Cg compiler diagnostics and retries failed shader compilations
at lower optimization levels. Failed shader sources and origin files remain
under `ux0:/data/tww/shaders/`.

Hardware build 01.22 reproduced the same internal Cg compiler error at O3,
O2, and O1. Build 01.23 lowers the declared element counts in the two GX
word buffers by using four-word vectors. Buffer sizes and word order are
preserved. The normal shader cache checks the resulting source before reuse.

Hardware build 01.23 compiled the recorded GX shaders and presented the
first frame. The Vita core dump places the next fault in JAudio's
`talkOut`, called by the logo-to-opening scene transition while the
stream state was uninitialized. Build 01.24 keeps those scene and
playback requests out of the muted audio engine.

Hardware build 01.24 passed the opening-transition audio call. The next
fault was in the sound-bank loading check during play-scene creation.
Build 01.25 treats an absent audio bank as having no pending loads and
keeps the muted scene music and dynamic wave operations idle.

Hardware build 01.25 reached stage `sea_T`, room 44, then faulted while
registering its camera in JAudio. Build 01.26 skips listener updates if its
storage is absent and suppresses voice, enemy audio and reset paths in the
muted Vita build.

Hardware build 01.26 reached room 44 and loaded the title assets. The dump
locates the next crash in sea ambience playback. Build 01.27 disables
ambient playback in muted mode while preserving the environment actor's
wave updates. Sound object deletion also handles an absent registry.

Build 01.28 disables playback at the game-facing audio interface and guards
direct JAudio playback and movie mixer entry points. Muted status queries
return their idle values. Wind Waker rhythm and visual wave updates remain
active independently of sound.

Hardware build 01.28 advanced past the audio failures and hit a shader
translator allocation failure while decoding GX commands. Build 01.29 uses
the existing thread-local host heap scope for FIFO decoding and Aurora
frame work so renderer storage is independent of the active game heap.

Hardware build 01.29 reached title draw submission. The next core dump
identified animation audio for a seagull using an uninitialized JAudio
listener. Build 01.30 disables audio dispatch in the shared animation
sound path and its Zelda-specific playback entry point.

Hardware build 01.30 reached title draw submission but the tester saw a
black display. Build 01.31 compares nonblack pixel counts in the present
source, the EFB when distinct, and the display buffer. It logs queue and
presentation counts at startup and during the title so the next trace can
locate the failure.

Hardware build 01.31 measured 326205 nonblack pixels in the EFB at the
title while the GX display copy and Vita display buffer remained black.
Build 01.32 presents the EFB directly as a temporary workaround for
the black GX display copy.

Hardware build 01.32 showed the title and its 3D backdrop. The trace ended
at the start of vertex shader 2a4f5a80355c9dd3 compilation at O3. Build 01.33
starts that shader at O1 with fast math disabled and logs compiler return
and elapsed time. It also logs the START edge at the Vita pad backend.

Hardware build 01.33 spent around 87 seconds completing five new shader
compiles near the title effect before its trace ended at the next compile.
Build 01.34 skips the title sparkle emitter and overlay model on Vita for
the file-select test. The subtitle and the rest of the title are still drawn.

Hardware build 01.34 reached the file selector and name entry. The exit
after confirming the name came from the JKRExpHeap overlapping-block
check. Build 01.35 records the block bounds, the name scene transition
and candidate code addresses on the panic thread's stack.

## 01.36: frame lifetime during the first quest

The 01.35 trace reaches the end of the name scene destructor, then reports
overlapping free blocks in JKRExpHeap. The stack includes the final sealed GX
frame destructor. GXCopyDisp and GXCopyTex now run inside Aurora's host
allocation scope because their render passes and resources can outlive the
current scene heap. The Vita build also propagates that scope's build flag to
the GX FIFO and command decoder.

## 01.37: fast start and persistent saves

New quests use SwitchWaker's opening legend skip and enter the normal first
play scene. The file-select menu can create the initial save data, and the
pause menu can save the active quest. Keep both `ux0:/data/tww/gczelda.card`
and `ux0:/data/tww/gczelda.bak` when backing up saves. The backend alternates
between these snapshots and validates the previous snapshot if a write is
interrupted. The first play draw and periodic counts of submitted and
presented frames are written to the boot log.

## 01.38: card detection and Outset shader compilation

The Vita game loop polls the virtual card even when audio is muted. The
01.37 core dump occurred while the runtime compiler handled the Outset
vertex program `423e4a681fc7ae43`. This program starts at O1 without fast
math and falls back to O0. Compiler errors are reported after the compiler
returns. The existing shader cache remains usable.

## 01.39: fractional destination alpha at the start of play

The 01.38 hardware run created the virtual card and reached the end of the
Outset opening cutscene. The GXM backend reported an unsupported fractional
constant alpha and exited. For the GX destination-alpha override, the Vita
renderer now keeps the RGB draw and preserves the target's previous alpha
when the requested constant is fractional. This is a compatibility fallback
for GXM's fixed-function blender. The color and outline defects reported
during the opening cutscene still need investigation.

## 01.40: displayed FPS and minimap mask

The Vita display callback shows the actual presentation rate at the top of
the screen. The boot log also records mean time spent admitting a frame,
running the game frame and submitting it to Aurora. The minimap's alpha-only
mask writes its requested alpha directly through TEV with replace blending.
The vertex shader `aa6a00a9790047fa` uses the conservative runtime compiler
settings after a compile failure during the 01.39 play test.

## 01.41: palette lifetime and half-size EFB

The 01.40 Outset play test loaded its save and rendered the minimap. Inside
Link's house, the core dump points to the palette identity hash table in
GXInitTlutObj while building a dialogue icon. Its allocations now use the host
heap and palette identity lookups are serialized. The Vita test halves both
EFB dimensions while preserving logical GX coordinates and native scanout.
The boot log splits event polling, staging waits and GPU finish times so
future tests can identify the remaining bottleneck.

## 01.42: clocks, toon shading, float TEV and a resolution setting

- The executable now raises the clocks to 444 MHz CPU, 222 MHz bus, 222 MHz GPU and
  166 MHz GPU crossbar, as sms-vita does. Homebrew starts at 333 MHz CPU and
  111 MHz GPU, so earlier builds ran with half the GPU. The boot log records the
  clocks in `vita-clocks`.
- Toon shading: a `GX_TG_SRTG` texgen with a colour-channel source reads the
  lit channel output (Dolphin's `vertex_lighting_N`), not the raw vertex colour.
  The GXM fork read the absent vertex colour as white, so Aryll, Grandma and other
  NPCs sampled the brightest texel of their toon ramp. This is SwitchWaker's Aurora
  patch 0002 applied to the GXM fork. Its patch 0004 (`GXInitLightDistAttn` with a
  reference brightness of 0) is included too; 0001 and 0003 were already there.
- GX draws use float TEV combiners on the Vita, the combiners the native MH3
  materials use. The exact path emulated the 8-bit integer combiner per channel,
  which is very expensive on the SGX543. Shaders are recompiled once after
  updating (new sources, new cache entries).
- `ux0:data/tww/settings.txt` is created on first start:
  `resolution=` (25-100, percent of the GX frame in each dimension, default 50,
  the size 01.41 used) and `float_tev=` (1 default, 0 for the exact combiners).
- The boot log adds `vita-phase-0.1ms` every 10 s: game logic with the draw-list
  encode (`game`), the encode alone (`painter`), `mDoGph_AfterOfDraw`
  (`after_draw`), audio, pad, the GX to GXM translation (`fifo`) and the draw count
  of the last batch.
- `mh3-aurora-vita-recorded-gx.patch` no longer repeats the `aurora_core.cmake`
  change already in `mh3-aurora-vita-gxm.patch`, so the GXM patch list applies
  to a clean checkout.

## 01.43: settings menu

D-pad right + Square opens a settings menu, as in sms-vita. Up and down choose
a row, Cross or right steps its value forward, left steps it back, Circle closes.
The game sees no input while the menu is open. Values that differ from the
default are green.

- SHOW FPS: the counter at the top of the screen.
- RESOLUTION: 100, 87, 75, 62, 50 (default) or 37 percent of the GX frame in
  each dimension. It applies at once: the EFB render targets are rebuilt between
  two frames (the old ones are retired behind the GPU's fences) and the boot log
  records `vita-resolution`.
- FAST COLOUR MIXING (NEXT START): the float TEV combiners of 01.42.

The menu writes `ux0:data/tww/settings.txt` on every change and reads it at
start-up. It is drawn by the GXM display callback directly into each finished
display buffer, so it adds no draws to the game.

## 01.44: every vertex program at the safe compiler settings

The 01.43 hardware run reached the title (after several minutes of first-run
shader compilation) and crashed when the save was loaded. The core dump holds
`Cg compiler error: fatal internal error` on the game thread, during the O3
compile of vertex program `23c61c0e983692c9` for Outset. The toon texgen fix
changed the source of the Outset vertex program that 01.38 had listed as
`423e4a681fc7ae43`, so the list of programs compiled at the safe settings no
longer matched. The compiler's fatal error ends the process before a fallback
can run, so all GX vertex programs now start at O1 without fast math (O0 as the
fallback). Fragment programs keep O3.

## 01.45: resolution measurements, icon trace and a profiling build

The 01.44 resolution test (menu, Outset) showed the game is CPU bound: outside,
37% and 100% both run at about 4.5 fps. In the first house the GPU starts to
limit above 75% (100%: ~9 fps with 32 ms of staging waits; 62% and below:
~12 fps, no waits). Changing the resolution while playing worked at every step.

- Button icons in message text (A, L, the stick...) are drawn as gaps. 01.45
  logs, once per icon number, the textures its J2DPicture holds (`vita-icon`).
- `-DTWW_VITA_PROFILE=ON` builds the game with `-finstrument-functions` and the
  sampling profiler from sms-vita (`vita_prof.cpp`): every 20 s the boot log
  lists the busiest functions of each thread as addresses, with the sampler's
  own address for the load offset. Build the GXM renderer for it with
  `-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` and
  `-finstrument-functions -finstrument-functions-exclude-file-list=include/c++,/bits/,_deps,/extern/`
  added to `CMAKE_CXX_FLAGS`. The hooks slow the game down; read the shares,
  not the frame rate.

## 01.46: first profile results

The 01.45 profiling build (20 s windows of the game thread, instrumented, so
small functions are overstated):

| Outset outside | share | Grandma's house | share |
| --- | --- | --- | --- |
| `mDoGph_Painter` (draw) | 62% | `mDoGph_Painter` | 77% |
| GX translation (`fifo::process`) | 42% | GX translation | 48% |
| `GXCallDisplayList` | 40% | `GXCallDisplayList` | 34% |
| `dGrass_packet_c::draw` | 18% | HUD (`dDlst_2DMETER1_c`) | 13% |
| game logic (`fpcEx_Execute`) | 20% | `dKankyo_wave_Packet::draw` | 10% |
| `aurora_end_frame` | 10% | `GXEnd` (immediate 2D quads) | 19% |

Display lists and immediate primitives are translated on the game thread as
they are called. 01.46 removes two per-draw costs found there: the FIFO drain
timer (a clock read per drain; the log now counts `drains=` per frame) and the
uniform parameter lookups by name (`sceGxmProgramFindParameterByName` and a
`std::string` per sampler per draw), now cached per program. It also logs
24x24 palette textures bound to texmaps other than 0, for the message icons:
their pictures hold two C4 textures with RGB5A3 palettes, and only the ones
whose second texture is palettized are missing.

## 01.47: RESOLUTION AUTO

01.46 at 100% resolution (shaders cached): the game thread's frame went from
170-235 ms to 126-176 ms outside and from 72 to 54-57 ms in the houses, but
indoors the GPU now holds frames back (46-107 ms of admission wait a frame).

RESOLUTION AUTO, the new default, measures that wait over 1.5 s windows: above
12% of the frame time it steps the render size down (100, 87, 75, 62, 50%);
after three windows under 2% it steps back up. The window after a change is
not judged. The menu shows `AUTO` with the current size; a fixed percentage
still works as before.

The 01.46 icon trace shows the second palette texture of each message icon
resolves to a texture, so 01.47 logs which cache answers and how many opaque
texels a fresh conversion holds (`icon-palette`).

## 01.48: AUTO hysteresis

AUTO steps down after two busy windows (over 12% wait) or one over 30%, keeps
the size it left out of reach for 20 s, and steps up after four calm windows.

## 01.49: GX THREAD (experimental) and FAST GRASS

The 01.46 profile put 40-48% of the game thread in GX translation
(`fifo::process`), run inline at every `GXEnd` and display-list call.

- `GX THREAD (NEXT START)`, off by default: a worker on core 1 runs the
  command processor and owns the renderer while it does; the game thread
  (pinned to core 0) only appends commands and display-list copies, as
  upstream Aurora's threaded FIFO does. Every renderer access from the game
  thread (copies, frame end, texture objects, `GXDrawDone`) first waits for
  the worker, so the work runs in the same order as before. The log adds
  `vita-gx-thread-0.1ms worker=... game_wait=... publishes=...`.
- `FAST GRASS`, on by default and live: the outdoor grass (18% of the game
  thread outside) is one draw per room instead of one per tuft; the tufts'
  positions are transformed on the CPU. Tuft lists it cannot read fall back to
  the original path.

## 01.50: GX THREAD fixes

01.49 with GX THREAD stayed black at the logo: JFW's GP-hang alarm (one second
of `GXDrawDone`) called `GXSetDrawDone` from the OS timer thread while it held
the OS lock, and that thread then waited for the worker. Draw-done calls from
threads other than the game thread and the worker no longer touch the FIFO.
Thread roles are thread-local flags instead of stack ranges, the game thread
keeps its own core, and a wait for the worker longer than 3 s logs
`GX worker stall` with the worker's state.

FAST GRASS (01.49 log, Outset): about 700 fewer draws a frame and 25-30 ms
less game thread time (160-227 ms to 133-145 ms).

## 01.51: name and LiveArea

The bubble is titled "The Wind Waker" and released VPKs are named
`tww-vita-<version>.vpk`. LiveArea artwork stays out of git: run
`vita/tools/make_livearea.py ICON_ART BACKGROUND_ART vita/livearea` with your
own images and the build packs `vita/livearea/` when it exists.

01.50 log, GX THREAD on (per frame): Outset 75-128 ms game thread (sync 01.49:
133-160 ms), houses 42-62 ms (sync 54-57 ms). The worker is busy 50-97 ms and
the game thread waits for it 30-68 ms, at 4-6 sync points a frame.

## 01.52: fewer waits for the GX worker

- EFB copies (`GXCopyTex`, `GXCopyDisp`) are worker tasks in FIFO order
  instead of waits: the copy setters write a shadow of the copy state and each
  copy carries a snapshot of it. `GXPeekZ` reads the latest depth snapshot
  without waiting. RAM readbacks of copies (`aurora_flush_efb_copies_to_ram`)
  wait for everything first.
- `GX THREAD` has a third value, `FULL`: the worker also begins, seals,
  renders and presents frames, and `GXDrawDone` waits only for the previous
  frame, so the game thread runs one frame ahead of the worker.
- `JKRHeap::destroy`/`freeAll` wait for all queued GX work
  (`switchwaker-vita-gx-sync.patch`), as the worker may still read the heap.
- `GX THREAD` is on by default. AUTO resolution follows GPU staging waits
  instead of the frame admission wait, which with the worker also holds waits
  for the CPU.

## 01.53: GX tasks in host memory

01.52 corrupted a game heap when loading a save: a queued copy's task was
built on the game thread from the current JKR heap and freed by the worker
(`run_next_task` -> `JKRExpHeap::do_free` on the worker). Tasks and their
queue nodes are now allocated in host memory.

## 01.54-01.55

- 01.54: settings.txt keeps GX THREAD FULL (the loader read every non-percent
  option as on/off).
- 01.55: with SHOW FPS, the render size (and AUTO's current step, in yellow)
  is shown under the FPS counter.

GX THREAD FULL (01.54 log) is slower than ON for now: the worker then also
seals, renders and presents, and it was already the longer of the two threads
(Outset at 62%: FULL 97-120 ms a frame, ON 67-85 ms; houses 49-52 ms vs 42 ms).
ON stays the default until the worker is cheaper.

## 01.56-01.57: worker profile

01.56 profile (Outset, GX THREAD on, worker thread): command processing 88% of
the worker, of which draws 63% (unmerged draw setup 26%: sampled texture
resolution 11%, uniforms 5%, bind groups 4%; index pushes 15%; pipeline state
8%), BP 7%, XF 6%, CP 3%. The profile build now leaves tiny accessors
(`read_u32`, `bswap`, staging size helpers...) uninstrumented, as their probes
dwarfed their bodies. With GX THREAD the game thread is pinned to core 0.

01.57: the worker/game thread role checks use the threads' stack ranges
instead of `thread_local` (each access was a call to `__emutls_get_address`),
falling back to the thread id if the kernel gives no range.

## 01.58: worker round 1

- Matrix loads (position, normal, texture, post-transform) that rewrite the
  same bits no longer mark the draw state dirty, so J3D's repeated matrix loads
  between shapes stop ending merged draws.
- Palette textures (C4/C8/C14X2) use the per-frame texture identity cache; the
  identity now includes the TLUT loaded in the texture's slot.

## 01.58 results and 01.59: 16:9

01.58 log: houses 38 ms a frame (26 fps, was 42 ms), Outset 59-96 ms at 50-62%
(was 67-85 ms), the worker 37 ms and 52-87 ms.

01.59 adds `SCREEN (NEXT START)`, 16:9 by default: `COS_ASPECT` is set from
settings.txt in `pc_harness_init` (SwitchWaker's widescreen code, see its
docs/WIDESCREEN.md: wider camera, HUD at the edges, anamorphic 640x480 EFB), and
the first frame sets Aurora's `AURORA_VIEWPORT_16_9` policy, which renders into
a 16:9 target (960x540 at 100%) and presents it across the screen.

## 01.60: audio, title sparkle

- 01.59 log (16:9, 62%): Outset ~49 ms a frame, GPU-bound (42-45 ms waiting
  for a staging set), worker 47 ms; houses 39 ms with ~24 ms staging waits.
- `AUDIO (NEXT START)`, on by default: the muted Vita patches' early returns
  are run-time checks (`tww_vita_audio_muted`, switchwaker-vita-audio-option.patch),
  and the build links cos_sdk's AI/DSP/DTK with Dolphin's DSP HLE from
  RecompCore (`native/tools/fetch_recompcore.sh`; `vita_dsp_common_shim.cpp`
  replaces the shim's mmap). The DSP interrupt and AI DMA threads, and every
  game OS thread, run on core 2.
- `TITLE SPARKLE`: the logo's sparkle (particle and `mModel_kirari`), cut by an
  earlier build because it crashed, is back as a live option.

## 01.61 / 01.62: audio boot fixes

- 01.61: the SDK audio code read MEM1 size from aurora-up's `AuroraConfig`, whose
  layout differs from the mh3 Aurora the game links; `tww_vita_mem1_size()` now
  supplies 128 MiB.
- 01.62: the DSP interrupt and AI DMA threads, and every game thread started
  through the thread_start hook, may run on any user core
  (`SCE_KERNEL_CPU_MASK_USER_ALL`). Pinned together on one core, the DSPHLE boot
  hung with a black screen.
- 01.62: a watchdog thread logs `vita-watchdog no frame ended for N s` with the
  last milestone when no frame ends for 15 s, so a log tells a hang from a slow
  load.

## 01.63: background shader compiles

- A GX program that no cache holds is translated and compiled on a thread of
  its own (core 2, below the game, GX worker and audio). Until it is ready,
  draws that use it are skipped instead of freezing the game (a vertex program
  took 18.6 s at the first roll). The FPS line shows `SHADERS` meanwhile.
  Settings: BACKGROUND SHADERS (live); OFF compiles in place as before.
- `ux0:data/tww/shaders/glsl.index` maps each program's GLSL to its
  translated Cg's hash, so a program in `cache.pack` loads without the
  regex-based GLSL-to-Cg translation, which took seconds per program and
  caused the multi-second frames of the title screen. The first start of this
  version builds the index, so graphics may appear gradually that once.

## 01.64: no black band at the bottom

- Aurora keeps GX's 640x528 EFB (games use the rows under the picture as
  scratch), but TWW shows 480 of them. The Vita presented the whole EFB, so
  the bottom ~9% of the screen was black. The EFB target is now 528/480 taller
  and presentation pushes the scratch rows below the screen.

## 01.65: intro restored, button icon test

- The legend and the Outset prologue play again. SKIP INTRO (NEXT START), off
  by default, skips them as before (`COS_QOL_SKIP_INTRO`).
- Message button icons are J2DPictures with two C4 textures, blended so the
  second one gets all the weight; on the Vita they draw as gaps. BUTTON ICON
  FIX (live, on by default) draws such a picture with that texture alone on
  texmap 0, which looks the same (`switchwaker-vita-icon-single-texture.patch`).
  Whether it shows the icons tells if the fault is in sampling a palette
  texture on texmap 1 or in the texture itself.

## 01.66: icon trace

01.65's BUTTON ICON FIX did not bring the icons back. 01.66 applies it whenever
the colour blend gives the second texture all the weight, logs the first
pictures it applies to (`vita-icon-fix`), and logs the whole GX draw state
(TEV stages, konstant and register colours, blending, alpha test, textures)
of the first draws that sample a 24x24 C4 texture with two or more TEV
stages (`icon-draw`).

## 01.67: icon trace armed by the game

The 01.66 trace spent its reports on the title screen, where the fix did apply;
the message icons still sampled two textures. 01.67 logs each message icon
picture as it is drawn (`vita-icon-draw`: texture count, blend constants,
alpha, position) and only then arms the renderer's `icon-draw` trace.

## 01.68: button icons

The 01.67 trace showed the icon draws had the right textures and TEV state,
so the fault was in the vertices. `J2DPicture::draw` writes its texture
coordinates with `GXTexCoord2s16(0x8000, ...)` into a U16 attribute with 15
fraction bits: the FIFO takes the raw bits, 1.0. Aurora's immediate-mode
writer converted the value instead, clamping -32768 to 0, so every icon
sampled one transparent corner texel. Integer writes now keep their low bits
as the hardware does. The 01.65 single-texture workaround and its menu entry
are gone.

## 01.69: GPU pass counters and a profiling build

GX THREAD FULL (01.68 test) gained nothing: the worker's translation is the
limit outside (~100-140 ms a frame for ~800 draws) and close to it inside.
01.69 adds `vita-gpu-per-frame-x10` (GPU scenes, their pixels, depth
loads/stores, EFB-to-texture copies, all per frame times ten) to the 10 s
lines, and ships with a profiling build of the same version.

## 01.70: the worker's core, and where the game waits for it

01.69 measurements (house, Omasao): ~54 ms of CPU a frame whatever the
resolution (18.5 fps at 37-50%); above that the GPU sets the pace and scales
with pixels (14 fps at 62%, 10 at 75%, 6.3 at 100%), with 8 GPU scenes and 2
EFB copies a frame. Outside the worker alone takes ~107 ms. The profile shows
the game thread waiting in GXDrawDone for the worker for nearly half its time
in the house, and the audio thread busy enough to matter.

- Game threads (audio included) and the DSP/AI threads run on cores 0 and 2;
  core 1 is the GX worker's alone.
- `wait-stack` lines: the call stacks (6 return addresses, with the runtime
  address of `producer_acquire` to relocate them) where the game thread waits
  over 0.3 ms for the worker, summed per 10 s.

## 01.71: GX THREAD FULL without the frame-end drain

01.70 results (62%): with core 1 to itself the worker got faster (house
43 -> 33 ms, outside 107 -> 77 ms a frame). The house is now GPU-bound
(~26 ms staging waits, 13.8 fps) and outside the game thread is the limit
(~110 ms, 7.4 fps). In FULL the wait stacks put 48 ms a frame (house) in
`GXSetDrawDone` -> `drain()`: JUTVideo's frame-end draw-done still waited for
the whole FIFO. It now waits for the previous frame, like `GXDrawDone`.

## 01.72: CMPR AS DXT1

01.71 in FULL: the frame-end drain is gone, but FULL is still slower than ON
(house 11.5 vs 13.8 fps, outside ~7.9 vs ~7.4-8): in FULL the worker also
waits for the GPU (45 ms in the house), in series with its translation. ON
stays the default.

CMPR AS DXT1 (next start, on by default): GX CMPR textures (DXT1 in 8x8 tiles,
big-endian colours, reversed index bits) are reordered into GXM's swizzled
UBC1 instead of being expanded to RGBA8: an eighth of the memory and of the
GPU's texture traffic, and no expansion on the CPU. Power-of-two sizes (or one
level); levels under 4x4 are dropped. A transparent texel of a 3-colour block
is black in DXT1, the midpoint in CMPR, so filtered cut-out edges may darken a
little. Levels go in whole through `gl::write_texture_packed` /
`TextureStorage::upload_packed`.

## 01.73: GPU TIMING

01.72's CMPR AS DXT1 changed nothing measurable (house 13.5 fps, staging wait
27 ms as before; outside 7.4 fps): the GPU is not limited by texture reads.
With three staging slots the CPU runs ahead of the GPU, so the ~74 ms a frame
in the house (62%) is GPU time; it grows with pixels (~0.23 ms per thousand
EFB pixels, ~23 ms fixed). GPU TIMING (live, off by default, slow) finishes
every scene at its end and logs, per place in the frame,
`vita-gpu-scene N: ms avg, kpixels, draws`.

## 01.74: SHADER TEST

01.73's GPU TIMING (62%, each scene finished on its own): one scene takes
nearly all the GPU time, the main 3D pass, 91 ms with ~270 draws in the house
and 120 ms with ~700 draws outside; the other scenes (copies, palette
conversions, the 2D pass, the display blit) add ~15 ms together. SHADER TEST
(next start, diagnostic) tells how much of that is fragment shading: GREY
makes every GX fragment program output flat grey (the TEV, textures and alpha
test compile away), NO ALPHA skips only the alpha test (no discard, so the
GPU can reject hidden fragments before shading them).

## Integration points

- Use [SwitchWaker at `1080514`](https://github.com/centollOS/SwitchWaker/tree/108051434f2f67437947e78015b3dd409be6e1c6)
  as the first game-source baseline. Its native build requires Aurora's GX,
  OS, DVD, CARD and THP interfaces, among others. Start with the USA revision
  0 build so the existing PC test disc can serve as a reference.
- Apply [the ARM32 source patch](patches/switchwaker-arm32.patch) from the
  SwitchWaker checkout. It addresses the first compile errors in the logo,
  message and heap units. These units can be compiled against Aurora's
  headers and SwitchWaker's generated runtime-asset stubs before linking the
  rest of the engine.
- Evaluate [Grarak's Aurora GXM backend at `93f167c`](https://github.com/Grarak/mh3-wii/tree/93f167caadb46a7bd83351d65068b43ca0949f6e/aurora-main).
  Its Vita build uses GXM, with a shader bank generated for Monster Hunter Tri.
  SwitchWaker uses Aurora at `3227d76` with its own patch series.
  The first boot uses the GXM fork for graphics and the pinned SwitchWaker
  Aurora for DVD.
- Use `sms-vita` as the reference for file paths, Vita input, logging and
  runtime shader compilation. Use the native game build to identify the next
  graphics compatibility work as it reaches the actual boot path.

### 01.75

- SHADER TEST results (exterior, 62%): GREY cut the main 3D scene from ~120 ms
  to ~7.5 ms; NO ALPHA only to ~114 ms (house 91 -> 70 ms). Per-pixel shading is
  the GPU cost, not discard.
- FAST TEXCOORDS (NEXT START, default ON): direct GX texture samples no longer
  emulate the s24 fixed-point texture coordinate per pixel (integer work on the
  SGX). The vertex shader writes the normalised UV, so the fragment sample is a
  non-dependent read; the LOD bias moves to the GXM sampler (1/8 steps) and fog
  depth is computed in floats. Indirect stages keep the exact path.

### 01.76

- 01.75 results: house main scene 91 -> 44 ms, exterior ~120 -> ~94 ms (673
  draws). But every GX vertex program changed, and vertex programs compile at
  O1 without fast math (the O3 compiler faults on them): 10-37 s each, so
  SHADERS stayed on for a long time.
- FAST TEXCOORDS now has three values: OFF, PIXEL (default: the fragment shader
  scales the float UV, so vertex programs stay as before and keep their cache)
  and VERTEX (01.75's behaviour: non-dependent reads, long vertex recompiles).

### 01.77

- 01.76 PIXEL results: house main scene 55 ms (VERTEX 44, before 91), exterior
  91 ms (VERTEX 94, before 120). House ~16 fps; exterior ~8 fps with the game
  thread at ~110 ms. PIXEL stays the default.
- SHADER TEST gains NO TEXTURES (every sample is a constant, the TEV maths
  stays) and NO FOG, to split the remaining exterior cost.

### 01.78

- 01.77 results (main scene): NO TEXTURES exterior 92 ms (no change), house
  ~45-52 ms; NO FOG exterior 44 ms (from 91), house 13-16 ms (from 55). Fog is
  the GPU cost.
- SHADER TEST gains FOG NO RANGE (fog without the range adjustment) and FOG
  FIXED Z (fog maths kept, no fragment-position reads), to find which part.

### 01.79

- 01.78 results (exterior main scene): FOG FIXED Z ~46 ms, FOG NO RANGE ~51 ms,
  NO FOG ~44 ms, full fog ~91 ms. The range adjustment (per-pixel screen-x table
  lookup) is ~40 ms; the depth read itself ~5 ms.
- FOG (NEXT START): FULL (as GX), FAST (default, no range adjustment: fog is a
  little lighter at the screen edges), OFF.

### 01.80

- 01.79 FOG FAST: exterior main scene ~51 ms, house ~20 ms. Now the CPU side
  limits: exterior game thread ~110 ms (painter ~50 ms of it); house game thread
  ~36 ms plus ~36 ms in aurora_end_frame (drain of the GX worker + frame seal).
- DRAW DISTANCE (live, switchwaker-vita-draw-distance.patch): 100/75/50/35 % of
  the stage cull distance given to mDoLib_clipper.
- Test plan: GX THREAD FULL again now that the GPU is no longer the limit (it
  overlaps the worker's drain with the next game frame).
