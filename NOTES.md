# Working notes (state of the port)

Where the Wind Waker port stands and what is being chased, so work can pick
up from here in a new session. Same habits as sms-vita's NOTES.md: every
measurement and decision goes here.

## Versions

- Game: GZLE01 (USA) Rev 0. The decomp checks the image's main.dol hash when
  build.sh extracts the embedded data.
- tww (zeldaret/tww): `9cbb155` (30 Oct 2026).
- sms-port (chasem-dev/sms-pc-port): `fb2a93b`, plus
  `patches/sms-port/0001-build-for-the-ps-vita.patch` copied unchanged from
  sms-vita v1.0 (`7711d10`), plus `0002` (32-bit PC build).

## Feasibility check (3 Nov 2026)

- Code files: 1,113 in configure.py; 953 match, 135 do not yet. None is left
  as assembly only: every one has a C++ file.
- Empty functions (`/* Nonmatching */` bodies, still to be decompiled):
  1.1 MB of the 7.8 MB of game code (14%). By part: the main program about
  5% (179 KB of 3.35 MB), the actor modules 21% (937 KB of 4.45 MB).
  - Worst: the event (cutscene) camera `d_ev_camera` (all of it), the main
    camera `d_camera` (37%: talk, manual, hookshot, ride, tornado, tower,
    hung, vomit, shield cameras), the sound interface `JAIZelBasic` (45%).
  - About 75 actor files almost all empty: Ganondorf, Gyorg, Mothula,
    Magtail, Boko Baba, Octorok, Bubble, Poe, Miniblin, Rat, Zelda, Deku
    Tree, Koroks, Komali, Great Fairy, Joel & Zill, many Windfall NPCs...
  - The list, per file, is written at configure time to
    `<build>/tww_stubs.txt` (tools/gen_sources.py).
- Pace: empty functions 7,007 (Feb 2026), 6,114 (May), 4,461 (Aug), 3,241
  (Nov): about 400 a month lately; at that pace all written by mid-2027.
- SDK: the game calls 309 SDK functions; sms-port's platform layer had 264.
  The rest are in `port/src/tww_platform.cpp` (see below).
- Four Swords Adventures, for later: two decomps (sh1ftmaker/fsa 29% code,
  game code 12%; 999sian/fsa-decomp claims 59% / 88% of functions, very new
  and automated: check what is real before trusting it). GBA-linked
  multiplayer would not carry over.

## How it is built

- The game: every retail object of tww's configure.py except the SDK, MSL,
  the runtime, the debugger and the REL glue (tools/gen_sources.py), with
  GCC, `-std=gnu++11` (unions of classes with constructors need it)
  `-fpermissive -Wno-narrowing`, force-including `port/include/tww_compat.h`
  (which includes sms-port's `port_compat.h`). `port/include/dolphin/types.h`
  replaces the decomp's and takes the integer types from sms-port's, so the
  game and the platform layer agree.
- The platform layer is sms-port/platform compiled against the Sunshine
  decomp's SDK headers (sms-port/decomp), as in sms-vita, minus the
  Sunshine-only parts (HD movies, endian_game, the warp menu, presence).
  OSAlarm and friends that the game and the two SDK header sets share have
  the same layout (checked by hand for OSAlarm; to do: static_asserts).
- The actor modules (415 RELs) are linked into the program:
  - DynamicModuleControl loads nothing (patch 0002).
  - Their static constructors run when the game links the module, not
    before main(): tools/rel_cc.py (a compiler launcher for the tww_rels
    library) moves each one's `.init_array` into a section `tww_ctors_<name>`,
    and `rel_ctors.cpp` (generated) runs them from do_link. Needed: before,
    daNpc_Tc_HIO_c's constructor read the save file before main() and
    crashed. 103 modules have such constructors.
  - Names defined by two modules (searchActor_Bm1, hio_set) made static.
- The embedded data (144 small textures, display lists, vertex tables) is
  extracted from the user's disc at the first build by the decomp's own tools
  (dtk split + its converters; tools/fetch.sh extract_assets).
  `-DTWW_PLACEHOLDER_ASSETS=ON` uses all-zero stand-ins instead, for link
  tests without a disc (tools/placeholder_assets.py).

## State

- 3 Nov: everything compiles (host gcc -m32) and links into a 32-bit Linux
  program (build-pc/tww). It starts, maps MEM1 and stops at "no game" here
  (no disc in my workspace). Next: the first run with the disc (run-pc.sh).
- 11 Nov, first run with the disc (a0080e2, GZLE01.ciso, WSL, llvmpipe):
  the disc image opens (2391 entries), ARAM is set up, the GL thread runs;
  crash in JFWSystem::init → JUTResFont::countBlock ("Unknown data block"):
  the system font is embedded in the program, big-endian and const.
  Fix (patch 0004): a converted copy (port_res_to_native_named, BFN). Same
  patch, ahead of the next crashes: RARC headers/tables converted when any
  of the four archive kinds opens (Mem, Aram, Dvd, Comp; Sunshine only
  needed Mem), each resource when first fetched (as sms 0011), the Yaz0
  length (as 0012), JSUInputStream's typed reads (as 0013).
  Not converted yet, by design: the platform layer logs each unknown
  resource magic once, which will list Wind Waker's own formats.

## What the first runs will most likely hit

- **Byte order of the game's files.** The Sunshine port converts the disc's
  big-endian data with patches to *its* JSystem (sms-port/decomp-patches
  0011 JKRMemArchive, 0012 Yaz0 length, 0013 JSUInputStream, 0017, endian-01
  ..19) plus platform/endian's converters. Wind Waker's JSystem is the
  decomp's own and has none of them yet; it also uses JKRAramArchive,
  JKRDvdArchive and JKRCompArchive, which Sunshine hardly did. Then Wind
  Waker's own formats: stage/room .dzs/.dzr, collision .dzb, messages .bmg,
  J2D .blo/.bck, events, particles (JPA, a newer version than Sunshine's?).
- Actors whose code is still empty must not be created (keep them from
  spawning, by profile name, from tww_stubs.txt).
- The camera's empty parts (talk camera etc.) will misbehave until the
  decomp fills them.
- 11 Nov, second run (0b88bb0): the font is fine; crash in
  JFWDisplay::createManager, the current heap NULL. Found here with a
  watchpoint: mDoDvdThd::main sets its own thread's current heap to NULL
  (`heap->becomeCurrentHeap()` with heap NULL, as on the console). On the
  console JKRThreadSwitch's switch callback gives each thread its own current
  heap back; the port had OSSetSwitchThreadCallback as a stub, so the DVD
  thread wiped the main thread's. Fix: sms-port patch 0002 calls the callback
  in switch_to on every switch (the default thread passed as NULL). The game
  then reaches the logo scene (dScnLogo phase_0) here, stopping where it
  mounts "System" (no files on my stand-in disc).

## Testing here without the disc

The start-up before the first file read can be run in my workspace: an empty
"disc" folder (`fakedisc/files`, plus `sys/boot.bin` starting GZLE01) as the
argument, under `xvfb-run` (32-bit Mesa: libgl1-mesa-dri:i386) and gdb.
Built with -DTWW_PLACEHOLDER_ASSETS=ON.
- 11 Nov, third run (96ef7fc): no crash; into the logo scene, GBA, memory
  card and DVD threads running (reading /framework.str); then frozen (the
  user stopped it). Two bugs:
  - Alarm deadlock: alarm_thread called OSGetTime() holding s_alarm_lock;
    OSGetTime runs the interrupt check, which ran alarm_poll, which takes
    s_alarm_lock. The main thread then waited in arm() forever
    (JFWGXDrawDoneAutoAbort sets an alarm each frame). Fix: alarm_now()
    reads port_time_ticks() (no interrupt check).
  - "unknown FIFO opcode 0x04" every frame: tww's own GX.h writes the GPU
    pipe through a raw 0xCC008000 union (a write sink here), bypassing
    sms_gx, so GF's (m_Do_graphic's screen clears, m_Do_ext) and J3DShape's
    raw commands were lost or misread. Fix: patch 0005, GX.h on TARGET_PC
    uses sms_gx/gxvert_pc.h and GXFIFO = GXPC_WGPipe (as Sunshine's GXVert.h).
  - The window said "Super Mario Sunshine": sms_gx's title is now
    port_window_title (weak default), set by tww_platform.cpp.
- 11 Nov, fourth run (8f86701): no FIFO errors, right title, black window,
  no crash; stopped by hand. The DVD thread looped in JKRFile::read on
  /framework.str (17,600 bytes asked): JKRFileCache asks for the size rounded
  up to 32 and JKRFile::read retries until it gets exactly that; the
  platform's reads stopped at the file's end. Fix (sms-port 0002): a read
  that starts inside a file and runs past its end returns the whole length,
  the rest zero-filled (the drive reads whole blocks). Checked here with a
  1,000-byte stand-in: on to JaiInit.aaf and System.arc.
- 11 Nov, fifth run (e48010d): past framework.str; sound up (SDL 32 kHz,
  DSP HLE mixer, 64 voices). Crash in JAInter::SeMgr::init writing
  SeParameter: the category/regist counts come from the sound table in
  /Audiores/JaiInit.aaf, read raw (big-endian) by SoundTable::init.
  Sunshine took its AAF from an archive (converted on fetch by name); Wind
  Waker reads it straight from the disc (JAInter::loadTmpDVDFile). Fix:
  patch 0006 converts it there with port_res_to_native_named (the
  platform's AAF converter: sound table, IBNK, WSYS, seq archive header,
  scene tables; same command set 1..8 as tww's checkInitDataOnMemory).
  Note also: "[audio] DSP: unknown command 0x8d00000a (2 words)" at boot
  (Wind Waker's DSP ucode command set may differ from Sunshine's; watch).
  Not converted yet: whole files read by mDoDvdThd_toMainRam_c
  (m_Do_dvd_thread.cpp:316), by entry number only.
- 11 Nov, sixth run (defc8be): same crash in SeMgr::init (line 78). The
  BreakLowerActiveDSPchannel reports are gone, so the AAF conversion
  changed something. Checked here: sizeof(SeParameter) = 0x444 and
  sizeof(SeqParameter) = 0x1718 on the PC, as on the console, so not a
  layout difference. Counts are bounded (u8 categories <= 16, regist 32),
  so the likely cause is a failed allocation (NULL) from the JAI heap.
  Patch 0007 logs the AAF's first words before/after conversion, the sound
  table's counts, and SeMgr's allocation with the heap's free space, and
  stops clearly if the allocation fails.
- 11 Nov, seventh run (4a78584): "SeMgr::init: 0 categories x 32"; JAI heap
  fine (1,085,052 bytes free); no [tww] line from loadTmpDVDFile or
  SoundTable::init. So the sound table was never set up, and with 0
  categories the loop bound 0*32-1 wraps around. JaiInit.aaf is not read by
  loadTmpDVDFile: m_Do_audio.cpp loads it with mDoDvdThd_toMainRam_c and
  passes the pointer (initLoadFileSw 4, checkInitDataOnMemory). Fix: patch
  0008 converts it in mDoAud_Create before setParamInitDataPointer (with
  before/after log lines). Patch 0006's hook stays (harmless, other files).
- 11 Nov, eighth run (749d065): **the Nintendo and Dolby logos are drawn**
  (archives, BTI textures, J2D and the GX translation work). JaiInit.aaf
  converted: 01000000 70060000 ... -> 00000001 00000670 ...; sound table
  version 11, 8 categories (271 167 166 65 404 419 599 111, then 97 75 in
  slots 16-17); SeMgr: 256 parameters, JAI heap 342,848 bytes free after.
  "[endian] resource format not converted: defaultse.bms" (a sequence:
  bytecode, read big-endian by the sequence parser; check if sound is
  missing later). No sound heard (none expected over the logos?).
  Crash after the logos in J3DMaterialFactory::createLockedMaterial
  (mpCurrentMtxInfo bad): a .bdl loaded by dRes for the next scene. The
  sms J3D converter skipped MDL3 ("not read by this J3D version"); Wind
  Waker's J3D reads it (locked materials). Fix (sms-port 0002): mdl3()
  converts the count, the 6 offsets, per material the DL {offset, size},
  patching offsets (6 x u16), current mtx (2 x u32), and the names; the
  DL bytes stay big-endian.
