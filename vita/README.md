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

The first game boot can link `tww_vita_no_card` to report empty GameCube
slots through the normal CARD API. A save backend is needed to write data.
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
git -C "$SW" apply "$PWD/vita/patches/switchwaker-vita-gcc.patch" "$PWD/vita/patches/switchwaker-vita-gx-ownership.patch" "$PWD/vita/patches/switchwaker-vita-title-log.patch" "$PWD/vita/patches/switchwaker-vita-first-painter.patch" "$PWD/vita/patches/switchwaker-vita-frame-tail.patch" "$PWD/vita/patches/switchwaker-vita-muted-opening.patch" "$PWD/vita/patches/switchwaker-vita-muted-waves.patch" "$PWD/vita/patches/switchwaker-vita-muted-listener.patch" "$PWD/vita/patches/switchwaker-vita-muted-ambience.patch" "$PWD/vita/patches/switchwaker-vita-muted-playback.patch"
git -C "$AURORA" apply "$PWD/vita/patches/aurora-vita-dvd.patch"
git -C "$GXM/.." apply "$PWD/vita/patches/mh3-aurora-vita-gxm.patch" "$PWD/vita/patches/mh3-aurora-public-gx.patch" "$PWD/vita/patches/mh3-aurora-public-gx-headers.patch" "$PWD/vita/patches/mh3-aurora-vita-fatal-log.patch" "$PWD/vita/patches/mh3-aurora-vita-recorded-gx.patch" "$PWD/vita/patches/mh3-aurora-vita-packed-shader-fetch.patch"
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
The 01.28 build runs muted and reports empty memory-card slots. The GXM
readback path for the Picto Box still needs implementation.
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
