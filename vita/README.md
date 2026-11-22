# SwitchWaker on Vita

The startup probe runs SwitchWaker's `pc::loadAssets()` against the Vita
CISO reader. It checks the DOL and RELs against the USA revision 0 hashes,
loads the 163 arrays into memory and fills an array registered by the probe.
It calls Aurora's OSInit, JFWSystem::firstInit and SwitchWaker's machine initialization routine
for the command, archive, game and Zelda heaps. It uses SwitchWaker's OS SDK to run a worker thread
that allocates from the shared JKR heap, post messages from the worker
and a timer callback, and join the worker. It exercises the game's JKRDvdFile
with both path and FST entry opens for the title and ocean archives.
The Vita DVD adapter completes those reads inline and delivers the result
through JKRDvdFile's existing message queue.
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
without a result. The executable covers the machine initialization for
heaps, file access and input. The full JFW/GX backend and the game loop still need to
be linked for the first full boot.

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
  Port the GX interface and movie support alongside the renderer. A replacement
  Aurora checkout needs its interfaces and patches reconciled first.
- Use `sms-vita` as the reference for file paths, Vita input, logging and
  runtime shader compilation. Use the native game build to identify the next
  graphics compatibility work as it reaches the actual boot path.
