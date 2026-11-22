# SwitchWaker on Vita

The startup probe opens the CISO and checks the inputs the game will need to
boot. It maps the disc, reads the executable header and file system table,
and seeks to the start and end of two archives used by the native title and
logo code. The probe runs independently of the SwitchWaker executable.

## Build and run the hardware probe

From the repository root in Ubuntu with VitaSDK, CMake and Ninja:

```sh
cmake -S vita/probe -B build-vita-probe -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-vita-probe
```

Set `VITASDK` to the SDK installation directory. Install
`build-vita-probe/tww-vita-probe.vpk` in VitaShell and open **TWW Vita Probe**.
Expect a green screen if all start-up reads succeed, or red if a read fails. Press START
while moving both sticks. The app logs the stick readings at the time START is
pressed, then exits. It also exits on its own after about 30 seconds.
Collect `ux0:/data/tww/probe.log` before running the probe again.

The earlier graphics probe embedded the MIT-licensed texture shaders from
[libvita2d](https://github.com/xerpi/libvita2d/tree/a8f15ab09d5233f0a4e4ad0e8f6ade0da888cbed/libvita2d/shader).

Keep your USA revision 0 GameCube CISO at `ux0:/data/tww/WindWaker.ciso`.
The log names each checked archive and reports `boot: disc startup data pass`
when the disc checks succeed.

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
