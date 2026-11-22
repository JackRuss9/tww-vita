# SwitchWaker on Vita

The first hardware check is a standalone homebrew that tests startup, storage,
GXM initialization, display output and the controller. GXM draws an RGB
triangle to an offscreen surface, copies it to the display framebuffer, and
checks two pixels from both surfaces. The probe runs independently of the game.

## Build and run the hardware probe

From the repository root in Ubuntu with VitaSDK, CMake and Ninja:

```sh
cmake -S vita/probe -B build-vita-probe -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-vita-probe
```

Set `VITASDK` to the SDK installation directory. Install
`build-vita-probe/tww-vita-probe.vpk` in VitaShell and open **TWW Vita Probe**.
Expect a red, green and blue triangle on a dark background. A solid red
screen indicates that a graphics step failed. Press START
while moving both sticks. The app logs the stick readings at the time START is
pressed, then exits. It also exits on its own after about 30 seconds.
Collect `ux0:/data/tww/probe.log` before running the probe again.

The probe embeds the MIT-licensed color shaders from
[libvita2d](https://github.com/xerpi/libvita2d/tree/a8f15ab09d5233f0a4e4ad0e8f6ade0da888cbed/libvita2d/shader).

For an optional disc read test, keep your GameCube CISO at
`ux0:/data/tww/WindWaker.ciso`. The probe reads the GameCube header from the
first stored CISO block and logs the disc ID, revision, file size and block
size for `GZLE01` (USA) or `GZLP01` (Europe).

## Integration points

- Use [SwitchWaker at `1080514`](https://github.com/centollOS/SwitchWaker/tree/108051434f2f67437947e78015b3dd409be6e1c6)
  as the first game-source baseline. Its native build requires Aurora's GX,
  OS, DVD, CARD and THP interfaces, among others. Start with the USA revision
  0 build so the existing PC test disc can serve as a reference.
- Evaluate [Grarak's Aurora GXM backend at `93f167c`](https://github.com/Grarak/mh3-wii/tree/93f167caadb46a7bd83351d65068b43ca0949f6e/aurora-main).
  Its Vita build uses GXM, with a shader bank generated for Monster Hunter Tri.
  SwitchWaker uses Aurora at `3227d76` with its own patch series.
  Port the GX interface and movie support alongside the renderer. A replacement
  Aurora checkout needs its interfaces and patches reconciled first.
- Use `sms-vita` as the reference for file paths, Vita input, logging and
  runtime shader compilation. A representative GX draw through Aurora is
  the next graphics check before trying the opening on Vita.
