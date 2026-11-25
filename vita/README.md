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
git -C "$SW" apply "$PWD/vita/patches/switchwaker-vita-gcc.patch" "$PWD/vita/patches/switchwaker-vita-gx-ownership.patch" "$PWD/vita/patches/switchwaker-vita-title-log.patch" "$PWD/vita/patches/switchwaker-vita-first-painter.patch" "$PWD/vita/patches/switchwaker-vita-frame-tail.patch" "$PWD/vita/patches/switchwaker-vita-muted-opening.patch" "$PWD/vita/patches/switchwaker-vita-muted-waves.patch" "$PWD/vita/patches/switchwaker-vita-muted-listener.patch" "$PWD/vita/patches/switchwaker-vita-muted-ambience.patch" "$PWD/vita/patches/switchwaker-vita-muted-playback.patch" "$PWD/vita/patches/switchwaker-vita-muted-animation.patch" "$PWD/vita/patches/switchwaker-vita-title-sparkle.patch" "$PWD/vita/patches/switchwaker-vita-name-heap-trace.patch" "$PWD/vita/patches/switchwaker-vita-card-commit.patch" "$PWD/vita/patches/switchwaker-vita-card-poll.patch" "$PWD/vita/patches/switchwaker-vita-minimap-alpha.patch" "$PWD/vita/patches/switchwaker-vita-icon-trace.patch"
git -C "$AURORA" apply "$PWD/vita/patches/aurora-vita-dvd.patch"
git -C "$GXM/.." apply "$PWD/vita/patches/mh3-aurora-vita-gxm.patch" "$PWD/vita/patches/mh3-aurora-public-gx.patch" "$PWD/vita/patches/mh3-aurora-public-gx-headers.patch" "$PWD/vita/patches/mh3-aurora-vita-fatal-log.patch" "$PWD/vita/patches/mh3-aurora-vita-recorded-gx.patch" "$PWD/vita/patches/mh3-aurora-vita-packed-shader-fetch.patch" "$PWD/vita/patches/mh3-aurora-vita-host-alloc.patch" "$PWD/vita/patches/mh3-aurora-vita-present-probe.patch" "$PWD/vita/patches/mh3-aurora-vita-present-efb.patch" "$PWD/vita/patches/mh3-aurora-vita-title-compiler.patch" "$PWD/vita/patches/mh3-aurora-vita-copy-host-alloc.patch" "$PWD/vita/patches/mh3-aurora-vita-outset-compiler.patch" "$PWD/vita/patches/mh3-aurora-vita-fractional-dst-alpha.patch" "$PWD/vita/patches/mh3-aurora-vita-fps-and-swim-compiler.patch" "$PWD/vita/patches/mh3-aurora-vita-palette-and-efb.patch" "$PWD/vita/patches/mh3-aurora-vita-toon-float-tev-and-scale.patch" "$PWD/vita/patches/mh3-aurora-vita-settings-menu.patch" "$PWD/vita/patches/mh3-aurora-vita-safe-vertex-compiler.patch" "$PWD/vita/patches/mh3-aurora-vita-param-cache-and-icon-trace.patch"
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
