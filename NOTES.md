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
- 11 Nov, ninth run (3c61902): the MDL3 fix worked (models load); sound
  picks the first scene (sea_T, room 44). Crash leaving the logo scene in
  JPAResourceManager::swapTexture (pTexRes->registNum bad): the common
  particle archive, JPAC1-00, not converted (Sunshine has single JEFFjpa1
  files only). It is loaded whole by the DVD thread (l_particleCommand),
  not fetched from an archive. Fix:
  - sms-port 0002: port_endian_jpc (dispatch on "JPAC1-00"): header, each
    emitter through port_endian_jpa plus its resource ID, and the BEM1,
    FLD1 and TDB1 blocks, which Wind Waker reads as structs
    (JPADynamicsBlockArc etc. point at +0x0C; Sunshine stream-reads them);
    then the TEX1 blocks (size, ResTIMG header at +0x20).
  - patch 0009: mDoDvdThd_toMainRam_c::execute converts what it loads by
    magic.
  Checked here with a synthetic JPAC (one emitter, BEM1/FLD1/TDB1, one
  TEX1): every field converts to the expected value.
  Also seen: "resource format not converted: ball128b.i4" (8,192 bytes, a
  raw 128x128 I4 image; check how d_s_logo uses it if anything looks off).
- 11 Nov, tenth run (81dac71): the JPAC is read now; stopped at
  JPABaseShape.cpp:93 (makeColorTable: colTbl NULL, bad colour-key
  counts/frames). Wind Waker's JParticle is another version than
  Sunshine's: BSP1 has its key offsets at data+0x04 (Sunshine's converter
  swapped block+0x12...), so none of the sms per-block converters fit.
  Rewrote port_endian_jpc with Wind Waker's own layouts (blocks read as
  structs at +0x0C): BSP1 (0x54, plus the {s16 frame, RGBA} colour keys at
  offsets from the block start), ESP1 (0x60), SSP1 (0x48), ETX1 (0x24),
  KFA1 (f32 keys from 0x20), BEM1 (0x80), FLD1, TDB1 (u16), TEX1. Checked
  with a synthetic JPAC containing every block type.
- 11 Nov, eleventh run (21a1300): particles fine. Crash in dADM::SetData
  (dScnLogo_Delete): the actor data table is big-endian. With it, two more
  of Wind Waker's own tables read whole by the DVD thread and flagged "not
  converted" in the log: item_table.bin ('ITEM_TABLE', s16 count at 0x0A)
  and FmapDat.bin (u32 last index, 8-byte entries with two s16; 396 = 4 +
  49 x 8). ActorDat.bin: s32 block count, {u32 tag, count, offset} per
  block, ACFN/ACNA u32 string-offset arrays, ACDS bytes. Fix:
  port/src/tww_endian.cpp converters, called in dScnLogo_Delete before the
  tables are handed over (patch 0010; the fmap has no magic, so not by
  format). Checked with stand-in tables.
- 11 Nov, twelfth run (0c01f81): **the logo scene ends cleanly**; the play
  scene for the title (sea_T, room 44) starts; crash in
  dStage_dt_c_offsetToPtr: stage.dzs not converted ('....STAG', 4,608
  bytes), and event_list.dat next (227,804 bytes). Both come from
  archives, so by name through port_endian_game (was a stub).
  - Stage/room files (.dzs/.dzr): s32 chunk count, {tag, count, offset}
    per chunk; per chunk type a record size and its fields, from d_stage.h
    (actor records 0x20 with fopAcM_prmBase_class at +8, TGSC 0x24, STAG,
    AROB/RARO, FILI, Pale, Colo, Virt, LGHT/LGTV, PATH/RPAT, PPNT/RPPN,
    SOND, FLOR, MULT, SHIP, 2DMA, DMAP, MEMA; byte-only ones listed as
    such; RTBL's offsets to shared room records). Unknown tags are logged
    ("[tww] ...: chunk 'XXXX' not converted"). Converted before = the
    big-endian count reads huge, so not converted twice.
  - event_list.dat: 0x40 header (7 x {offset, count}), event (0xB0), staff
    (0x50), cut (0x50), data (0x40) records, f32/s32 arrays, strings.
  Both checked here on synthetic files.
- 11 Nov, thirteenth run (1aa7bcc): stage.dzs converted, but crash in
  dStage_stagInfo_GetParticleNo (stage info NULL): no chunk handler ran.
  dStage_dt_c_decode (and the actor loader's actor_node_names) compare the
  tag's bytes with the name strings' (*(int*)"STAG"), so the tags must
  stay in file order. Fix: the converter leaves the 4 tag bytes and swaps
  only count and offset; looks the type up by the big-endian tag.
- 11 Nov, fourteenth run (4709900): stage chunks handled now; then a jump
  through a bad pointer: gdb shows JKRExpHeap::createRoot with garbage
  arguments and a return address on the stack (no usable backtrace).
  Diagnostics added:
  - tools/crash.gdb (run-pc.sh): after the backtrace, registers, the code
    at the crash address (guarded), and every code address found on the
    stack with its symbol (recovers the callers after a wild jump; checked
    with a test program calling through a bad function pointer).
  - The stage converter logs each file's chunks ("[tww] stage.dzs: N
    chunks: STAG:1 ...").
  - Patch 0011 logs each stage chunk handler before it runs.
- 11 Nov, fifteenth run (207e552): the stack scan worked: phase_3 (title
  play scene) -> check1stDynamicWave, a not-yet-decompiled function
  (`BOOL f() { /* Nonmatching */ }`). GCC drops the return of a non-void
  function that runs off its end, so the call ran on into the next code
  (JKRExpHeap::createRoot.cold). This applies to every empty function that
  returns a value: tools/stub_returns.py, run by fetch.sh on the clean
  checkout before the patches, gives each one `return {};` (499 functions
  in 80 files at 9cbb155: 321 BOOL, 100 bool, 64 cPhs_State, a few
  integers and pointers; void functions, constructors, destructors and
  references left alone). Patches apply on top of it unchanged; my local
  tww branch is now port2: a LOCAL commit with the script's result, then
  the patches (future patches are made the same way).
  Also seen: stage.dzs for sea_T has 13 chunks (STAG RTBL EVNT MULT EnvR
  Colo Pale Virt RPAT RPPN ACTR RCAM RARO), all handled.
- 11 Nov, sixteenth run (96fba9a): the stub fix worked (past phase_3 into
  phase_4, dStage_Create). Handlers STAG, MULT, RCAM, ACTR ran; crash in
  dStage_roomReadInit (RTBL) writing entry->m_rooms: the entry pointer is
  bad. offsetToPtr runs once for the stage (only roomLoader repeats it, on
  room files). Suspect: the converter's RTBL pass skipped (a size it was
  given smaller than the file?) or wrong values. Diagnostics: the stage
  converter logs the size it got, any chunk it skips as outside the file,
  and RTBL's first entries after conversion; patch 0012 logs the entries
  the handler receives.
- 11 Nov, seventeenth run: RTBL entries were 0x7c010000-style (swapped
  twice). STAG is 0x14 bytes on disc (the 0x20 struct's tail is runtime
  only); converting 0x20 ran into the first three RTBL entries at 0xB4.
  Fix: STAG converts 0x14 bytes, and the converter refuses any chunk whose
  records would run into the next chunk (logged). Broader boot logging:
  patch 0013 logs every next-stage request and the first creation of each
  process kind (with its module name from DynamicNameTable); crash.gdb
  calls port_crash_describe_context() for the recent disc files.
- 11 Nov, eighteenth run: the stage file now loads completely (all 13
  chunk kinds handled, RTBL right). The title scene created its first
  actors; the ship (d_a_ship, 167) returns an error on the title (the
  MET_KORL event flag is not set) and deleting it crashed: its process
  header (method table) was zero. Cause: fopAcM_ct does `new (p) T()`;
  for a class with no constructor of its own GCC value-initialises, i.e.
  zero-fills the whole object first (MWCC did not). Patch 0014 drops the
  "()" there and in the five effect processes / one scene doing the same.
  The stage-change log now prints only when the request changes.
- 11 Nov, nineteenth run: the ship fix worked; the title scene created
  more actors and started loading Room44, crashing in cBgS::ConvDzb: the
  room's collision file (room.dzb) was still big-endian. Added
  tww_endian_dzb (header, vertices, triangles, blocks, tree, groups,
  triangle infos), dispatched for any *.dzb through port_endian_game;
  synthetic test passes.
- 11 Nov, twentieth run: room.dzb converted fine; more actors created,
  then Link's creation (daPy_lk_c phase_1) crashed in fpcPi_Change: his
  `this` was 4 bytes before the process pointer. GCC (Itanium ABI) puts a
  derived class's vtable pointer at offset 0 when its base (fopAc_ac_c)
  has no virtual functions; MWCC put it at the end. A DWARF scan of the
  actor objects found 51 such classes (all offset 4: Link, NPCs, items,
  shop items, moving platforms...), none in the other process families.
  Patch 0015: on PC base_process_class has one empty virtual function
  (the vtable slot), and leafdraw_class / fopAc_ac_c inherit their headers
  instead of holding them as a first member, so the vtable pointer of
  every actor shares that slot. Re-scan: 0 of 305 offset; gdb confirms
  base_process_class and fopAc_ac_c at the daPy_lk_c address. Only 15
  source lines named the old members.
- 11 Nov, twenty-first run: the layout fix worked; Link got through
  phase_1 into his model setup (daPy_lk_c ctor -> J3DSkinDeform::
  initMtxIndexArray) and crashed there: that code walks GX display lists
  by hand, and on PC display lists stay big-endian, so vertex counts and
  indices came out byte-swapped. Patch 0016 reads them big-endian (also in
  changeFastSkinDL). sms-port 0002 now also converts CLS1 blocks of BLS
  cluster files (skin deform data; they went through the J3D converter
  with the block left as it was); synthetic test passes, shared index
  arrays swapped once.
- 11 Nov, twenty-second run: Link's skin setup passed; then his heap
  (GameCube estimate 0xB0000) ran out in initTextureScroll: `new
  J3DAnmTextureSRTKey` returned NULL and GCC constructed into it anyway
  (it assumes operator new never returns NULL unless -fcheck-new; MWCC
  checked). Fixes: -fcheck-new for the game code; patch 0017 skips the
  estimated heap on PC for the game's own max-then-shrink fallback, and
  logs each kind's first heap use vs the GameCube estimate (to see how
  much bigger PC allocations are, and why).
- 11 Nov, twenty-third run: with the max heap Link still gets NULL from
  `new J3DAnmTextureSRTKey` in initTextureScroll, right after
  JKRReadIdxResource from LkAnm.arc (an ARAM archive: the read goes
  through the ARAM thread). Other kinds' heaps used at or below the
  GameCube estimates (sea 0x4204 of 0xa000), so PC sizes are not the
  issue. Suspect: the current heap changes across that read (thread
  switch). Patch 0018 logs failed allocations (heap, type, free, current,
  thread) and the heap each kind's createHeap runs in; crash.gdb prints
  the current/root/system heaps.
- 11 Nov, twenty-fourth run: the failure is "0x88 bytes with no current
  heap": JKRHeap::sCurrentHeap was NULL right after the ARAM read in
  initTextureScroll (Link's heap 0x81603380 had been made current).
  JKRThreadSwitch::callback (retail) makes the next thread's saved heap
  current with no NULL check; every JKRThread starts with NULL, and the
  restore of the main thread's heap evidently did not happen on PC.
  Patch 0019: the demo version's check (no saved heap: keep the current
  one) on PC, and the first 40 heap switches of Link's first createHeap
  logged, to see the sequence if anything else is off.
- 11 Nov, twenty-fifth run: Link was created (used 0x26060 of the
  0xb0000 estimate) and d_a_title started; crash in
  daTitle_proc_c::proc_init2D (title_logo.blo panes NULL). Two findings:
  1. The heap-switch log showed the main thread coming back with the
     ARAM thread's heap: JKRThreadSwitch::callback saves and restores in
     one pass over the thread list, so when the next thread is listed
     first the current thread saves the already-switched heap. (Link's
     allocations after the ARAM read went to the wrong heap.) Patch 0020
     saves first in its own pass.
  2. .blo screens stay big-endian (dispatcher PE_FMT_KEEP_BE, for
     Sunshine's patched J2D); Wind Waker's J2D reads whole headers raw.
     Patch 0021 converts them (J2D_BE32/16). Same story ahead for
     messages: MESGbmg1 is kept big-endian and f_op_msg_mng reads INF1
     entries and the text's control codes natively; to do when text
     shows up (file select). sms-port's decomp-patches/endian-* are a
     checklist of formats like these (endian-14 was the skin-deform one).
- 11 Nov, twenty-sixth run: past the title logo: d_a_title, Agb, Bg and
  ~20 island actors created (ikada, wood, grass, bridge, tsubo, pig,
  kanban, kamome, kani, palm, knob...); heap use at or under the
  GameCube estimates everywhere. Crash: Link's procWait_init ->
  getAnimeResource: "alloc failed 0x2c in 'SLID' heap 0x0 free of 0x0":
  createAnimeHeap's heap had been shrunk to nothing (its sizing object
  was not allocated in it). (The doubled "-> main" switch lines are the
  main thread registered twice in the JKRThread list, not a bad switch.)
  Patch 0022: on PC the anime heaps keep their requested size, with
  diagnostics of size/use/current heap for the first 12.
- 11 Nov, twenty-seventh run: the title scene finished creating and runs
  frames (dScnPly_Execute). Crash in the event manager starting the
  opening cutscene: dRes_control_c::getRes("Demo51", "title.stb") with
  the archive mounted but mRes NULL (setRes must have failed). No game
  error lines in any log so far: the game's OSVReport prints with printf
  and stdout was buffered (lost at a crash). Now: stdout unbuffered (the
  game's OSReport_Error/Warning show up), getRes logs and returns NULL
  for an archive not set up (the cutscene falls back to the stage copy),
  and setRes logs the game heap when it cannot get a heap.
  Open question: Link's createAnimeHeap dummies for J3DAnmTransformKey and
  J3DAnmTexPattern used 0 bytes in their heaps (the other two kinds were
  allocated); the calls are in the object code. Worked around (0022).
- 12 Nov, twenty-eighth run: getRes now reports Demo51 "mounted but not
  set up" and returns NULL; the stage has no title.stb either, so the
  event code's JUT_ASSERT(demo_data) halts. No setRes heap failure was
  logged. Still no game error lines: the release build calls
  OSReportDisable() in mDoMch_Create (developmentMode == 0), so the
  game's OSReport_Error output never existed. Patch 0024 keeps reports on
  on PC and names the tag of "Unknown data block" (three of those, from
  J3D animation/cluster loaders).
- 12 Nov, twenty-ninth run: same halt; still no game error lines at all
  (OSReport_Error forces output, so setRes really reported nothing).
  The three "Unknown data block" are J3DModelLoader::load meeting MDL3
  in .bdl files (normal; the GameCube prints them too). Reading setRes /
  loadResource / the destructor, every path leaves mRes set or clears
  mpArchive with it, so either setRes never ran for Demo51 or the
  dRes_info_c was overwritten. Patch 0025 logs every archive's set-up
  (files, table, data heap, info address) and the full state of one
  found without a table.
- 12 Nov, thirtieth run (5554c54; the previous two ran the old build:
  the browser had saved the new bundle under another name, and once the
  WSLg display hung in xcb_connect). The set-up log showed it: Demo51
  reused Logo.arc's dRes_info_c slot (0x8a74784) and still held a stale
  archive pointer, so setRes thought it was done. deleteRes ends the
  record with an explicit destructor call, whose final stores
  (mpArchive = NULL, ...) GCC removes as dead (-flifetime-dse: the
  object's lifetime ends there). MWCC kept them; the game reuses
  destroyed objects everywhere. Fix: -fno-lifetime-dse for the game code
  (checked on a reduced copy: 1 of 3 null stores kept by default, all 3
  with the flag). This may also explain the createAnimeHeap dummies.
  The run command now pulls the newest tww-vita-update*.bundle.
- 12 Nov, thirty-first run (1c95e53): -fno-lifetime-dse fixed Demo51
  (set up: 6 files) and the title runs frames and DRAWS: the crash is in
  the first frame's opaque list, dGrass_packet_c::draw. The grass sets
  its arrays with GFSetArray, which writes a physical address; the arrays
  are embedded assets (game code data, host addresses outside MEM1).
  Patch 0026: GFSetArray calls GXSetArray on PC. sms-port 0002: GX
  physToPtr treats a "physical" address past emulated memory as a host
  address (the embedded material display lists carry IMAGE_ADDR(texture)
  = host pointer >> 5). Embedded vertex arrays are typed C values (Vec,
  colours), so native order: no big-endian registration needed. (For the
  Vita: host addresses there are above 0x40000000; the embedded DLs will
  need relocating instead.)
  Anime heaps type 0/3 still used 0x0 with -fno-lifetime-dse: still open.
- 12 Nov, thirty-second run (58e66e1): THE TITLE SCREEN SHOWS ("The
  Legend of Zelda" after the Dolby logo). Crash a few frames in: the
  raft (daObj_Ikada_c::_execute) calls JAIZelBasic::seStart, not
  decompiled, whose body is two OSReports and no return: GCC compiles
  running off a non-void function as impossible and the call went on into
  data. stub_returns.py now also handles bodies with code (Nonmatching /
  TODO, non-void, last statement not a return): 520 functions (was 499;
  two old ones were void, fixed). Then a scan of every file compiled
  with -Wreturn-type (instead of -w) left four, fixed by hand in patch
  0027 (JHIhtonl, d_operate_wind's known bug, two switches). To re-run
  that scan after decomp updates: ninja -t commands, -w -> -Wreturn-type,
  grep "control reaches end|no return statement".
- 12 Nov, thirty-third run (504f558): the title ran for a while (2164
  seStart calls, each logging that it is not decompiled), then halted in
  JAInter::SoundTable::getInfoPointer: unregistered SE category, from
  Link's animation sound (setAnimSound): sound ID 0x00380000 = 0x3800
  byte-swapped. Link's animation sounds live inside his BCKs, at the
  header's SE-anim offset (0x1C), in the .bas layout; the J3D converter
  swapped the offset but not the table. sms-port 0002 converts it
  (synthetic test passes). (sms-port already converts stand-alone .bas
  files, same layout.)
- 12 Nov, thirty-fourth run (042ed75): IN GAME. Link moves on Outset
  with the HUD (hearts, buttons, rupees); ran 220 s, no crash. But the
  title logo screen covers it as a translucent white box: JUtility::TColor
  stored its u32 straight into r,g,b,a (channels reversed on a
  little-endian host), so the .blo's colours (alpha 0 white -> alpha FF)
  became opaque. Patch 0028: TColor by value (sms-port endian-04), and
  JSUInputStream::readBE keeps a value whose read fails (endian-18);
  seStart's two lines said once (they made an 850 KB log).
  sms-port's decomp-patches/endian-* still to port when their area comes
  up: 03 (bmg messages, Wind Waker has its own reader), 07 (JAIGFrameStream),
  10 (JASTrack sequence tables, for sound), 11 (card saves), 12 (THP),
  17 (JAIBasic DVD files).
- 12 Nov, thirty-fifth run (9e76143): THE TITLE SCREEN, right: logo,
  sparkle, PRESS START, (c) line, Outset behind. But not the GameCube's
  title: Link stands on the beach, controllable, with the HUD, instead of
  the ship-sailing cutscene. title.stb (JStudio) was "not converted".
  Sunshine has no JStudio, so nothing to borrow. Split:
  - port/src/tww_endian.cpp tww_endian_stb: header, blocks, object
    flag/id size, sequence words (type in the top byte; types >= 0x80
    carry `param` bytes of paragraphs), paragraph headers (16/32-bit
    variable form), and the embedded JFVB function values in full (their
    paragraphs are 4-byte words, except 0x10 ID list and 0x15 u16 pair).
  - patch 0029: STB object paragraph payloads stay big-endian and are
    read with JGadget::binary::beLoad (TParseValue_raw_ for the value
    iterators, and ~20 direct reads in JStudio objects/adaptors).
  Synthetic test: structure native, payloads BE, FVB constant converted,
  second pass a no-op.


## PC review (14 Nov 2026)

- Kept both existing dependency checkouts. Their original tracked diffs
  were captured before editing and compared afterwards: unchanged outside
  JAIStreamMgr.cpp and f_op_msg_mng.cpp, the two deliberately edited files.
  The complete tww and sms-port patch series also applies successfully to
  temporary copies of the pinned checkouts.
- The previous tww-pc.log stopped in StreamLib::LoadADPCM (codec assertion
  at JAIStreamMgr.cpp:992), reading Audiores/Stream/title.afc. Patch 0030
  converts the copied 0x20-byte stream header, whether sourced from DVD or
  the AAF list. Also converts PCM16 samples when the stream uses codec 2;
  decoded ADPCM samples are already native. The title stream no longer
  hits this assertion. Audible output was not checked: automated runs used
  SDL_AUDIODRIVER=dummy.
- The next failure was a SIGSEGV in itemMsgGet_c::getMessage while creating
  the Name scene. Patch 0031 keeps the shared BMG resource big-endian (the
  JMessage stream reader expects that), but reads raw INF1 sizes, counts,
  offsets and message numbers in disc order. Copies of JMSMesgEntry_c have
  their numeric fields converted. The two raw text control-word reads and
  BMC colour reads are corrected as well.
- Verified screenshots: File Selection, Name Entry, and the opening text
  "This is but one of the legends of which the people speak...". A
  60-second window run completed its test interval without crashing.
  The extended X11 navigation run stopped advancing after opening text;
  two timed gdb samples locate the game waiting for GXPC_Present, with the
  GL thread in SDL_GL_SwapWindow / Mesa and llvmpipe workers active. This
  identifies the wait site, not its underlying cause. Do not call it fixed.
- Additional smoke runs: headless EGL, 180 seconds, captured through frame
  5400; Wayland window, 100 seconds, captured through frame 2700. Both
  ended at the requested timeout without a crash. Scripted input timing
  reached different menu states; they do not prove that starting a new
  game, completing the opening, or a full playthrough works. Later Wayland
  captures show the cloud background without the menu, another item to
  investigate. No graphics-backend default was changed on this evidence.
- ./build-pc.sh configures, builds and links successfully with the existing
  extracted real assets. Build failures used to be hidden by `|| true`
  resetting PIPESTATUS; now Ninja/tee errors fail the build. The runner
  propagates game failures, and gdb uses --return-child-result. crash.gdb
  skips stack commands after normal exit and captures 24 frames per thread
  after an interruption/crash, enough to see past the GL driver.
- Fetch no longer resets or cleans existing dependencies. When a patch
  fingerprint changes it requires a clean checkout at the pinned commit,
  otherwise it stops preserving the work. Tested with an untracked local
  file and with a mismatched clean revision. Build/game exit-code handling
  was tested with failing stand-ins. Bash syntax and git diff --check pass.
- run-pc.sh now defaults SMS_SAVE_DIR to run-pc/card-a, separate from the
  Sunshine platform's shared default. Existing saves at the old location
  are not moved or deleted; SMS_SAVE_DIR can select that location explicitly.
- Logs, before-edit dependency diffs and screenshots from this review are
  under run-pc/review and run-pc/*-frames (ignored local artifacts).
  Next: diagnose the X11 presentation wait and the menu disappearing during
  scripted navigation, verify entry into gameplay with controlled input,
  then test actual audio and saving. Vita work remains deferred.


## Outset free-walk on PC (14 Nov 2026, continued)

The reproducible entry point is now `./run-pc.sh --outset` (environment
`TWW_START_OUTSET=1`). It starts a new game in memory at `sea`, room 44,
layer 0, point 204: the beach. No existing save is loaded or overwritten by
the start hook. The default startup path remains available without the flag.
This is a free-walk bring-up route, not completed story progression.

Corrections, in addition to the preceding review:

- 0032: optional direct launch from the logo scene to PLAY_SCENE, initializing
  the new-save state once, rather than on every logo draw. Tested point 0
  first (the pier), then selected actual beach spawn 204 from the room's
  native player-spawn table. Point 206 is the story's initial lookout spawn.
- 0033: the real island never completed creation because NPC profiles 317,
  318, 319, 321, 322, 333 and 334 returned cPhs_INIT forever. Gdb recorded
  600+ SubCreateProcess visits for them, while the play/room nodes waited
  for their child creation queues. `gen_missing_actors.py` identifies the
  creation functions carrying the explicit stub-return marker (including
  bodies containing only static collision definitions), or empty marked
  bodies. It generates 82 missing profiles at configure time. Implemented
  creators retaining a Nonmatching comment are kept. Spawn helpers reject
  these before allocating parameters; the stage's preallocated route frees
  its parameters before declining the spawn. NPCs/enemies whose creators
  are missing will be absent; implemented partial actors are not blanket
  excluded merely because other methods have stubs.
- 0034: even event 0xFF schedules DEFAULT_START, and its undecompiled event
  camera left Link in system-demo wait with input suppressed. The explicit
  beach route skips the entrance demo. Later automatic story triggers on
  the island could similarly lock Link in DEMO_LOOK_AROUND2, so OTHER,
  COMPULSORY and POTENTIAL orders are rejected in this route on sea/44.
  Explicit interaction types are retained, but dialogue/story progression
  is not validated. Normal starts retain their event behavior.
- 0035: a subsequent timed gdb sample found the actual main-thread freeze
  in JFWDisplay::calcCombinationRatio, inside its signed tick-accumulation
  loop, with the GL thread idle. Host retraces can have unsuitable timing
  for that loop; native tick arithmetic now calculates the boundary in
  constant time with 64-bit intermediates, zero guards and a bounded blend
  ratio. `check/test_timing.cpp` compares against the retail loop for normal
  timing and covers degenerate/wrap values. Do not attribute all earlier
  freezes to SDL_GL_SwapWindow from a single interrupted sample.
- 0036: Link's idle head/body posture was wrong. The signed rotation-key
  Hermite interpolator, J3DHermiteInterpolationS, had only an MWCC assembly
  implementation and returned an uninitialized float on GCC. The host path
  now calls the existing JMAHermiteInterpolation with signed key values.
  The pose and walking animations now render normally. Numerical tests
  compile the actual J3DAnimation.cpp/JMath.cpp on the host, retaining active
  assertions, and check curves, slopes, signs and both key-table layouts.

Verification:

- Full PC build passes with real extracted assets. The complete patch series
  applies to a temporary clean copy of the pinned tww tree, and regenerates
  the identical 82-profile list. Existing dependency changes outside the
  intentionally edited files remain byte-for-byte identical to the initial
  diff snapshots. sms-port was not edited.
- Gdb-backed headless and normal X11 window tests verified Link execution,
  camera/HUD rendering, idle, walking in different directions, stopping on
  release, and following changing ground height in room 44. A water-entry
  test switched to swim states 54/55, and later respawned on the island.
- After the timer correction, a 120-second window test remained active and
  controllable. After the animation correction, a further 75-second window
  test recorded movement through more than 3,900 player frames with demo
  type 0 throughout, reached procFrontRoll_init, and returned to idle.
  Sample positions: beach start (-201085.562, 83.113, 315016.000), then
  (-201722.719, 80.983, 314789.250) and (-203655.359, 333.128, 315821.156).
  At the latter position the collision ground height was also 333.128.
  Final idle was (-204101.438, 45.398, 314860.656), matching the ground.
- Final animation run: `run-pc/probe-dDRGgN`; timer stability run:
  `run-pc/probe-iKgj7L`. Logs and PNG/PPM captures stay in those ignored
  local directories. `tools/probe-pc.sh` / `tools/movement.gdb` reproduce
  bounded runs with separate save directories and sampled player state.
- Audio was muted with SDL's dummy driver in these automated tests. Manual
  C-stick camera is still a decomp stub; the follow camera and L recentering
  work. NPC interactions, saving and full story progression are still
  separate unfinished areas. Vita work remains deferred.


2026-11-15 normal startup and audible output investigation:

- Added tools/normal_flow.gdb: virtual-keyboard-only navigation through title,
  card creation/selection and naming; the illustrated opening plays completely.
  No game scene, name buffer, save flags or player position is injected.
- Probe w2HnTO completed the illustrated opening, reached original sea room 44
  point 206 and ordered event 38 (awake). It then crashed loading a BTK into
  the BTP heap. Link dProcTool bypassed the big-endian parser for one animation
  ID. Patch 0037 routes that read through the existing parser. Build passes.
  Both deterministic probe 8QQnHJ and visible/audio probe qPQyMp passed that
  resource-loading failure but then faulted reading sp88 hand parameters in
  dProcTool. Normal startup is still blocked at awake; it is not playable yet.
- Probe --audio explicitly selects real SDL output and records audio.wav.
  Probe qPQyMp opened the 32000 Hz / 512-frame SDL device, with audible-output
  routing enabled. A 111.936-second recording contained nonzero PCM, peak
  8384, zero clipped samples. These measurements do not verify perceptual
  correctness or all music/effects. bgmStart and seStart in JAIZelBasic remain
  undecompiled, and DSP command 0x8d (Dolby delay) is not implemented by HLE.
- Diagnostics default to dummy audio and can run headless; that explains why
  earlier tests did not show a window or play through the host speakers.
  Use --audio without --headless/--deterministic for a window and host audio.


2026-11-15 continuation: original startup, audio and publication baseline:

- Patch 0038 reads STB paragraph data-ID sizes big-endian. The enclosing STB
  structure is converted on load, but this embedded header remains part of
  the raw payload. The native size displaced actor data by hundreds of bytes.
  check/test_jstudio.py exercises the actual header/parser and u16 iterators
  against a synthetic aligned ID, unaligned animation IDs and hand data.
- Complete PC rebuild passes. Timing, signed-key animation, JStudio payload
  and the platform's native-width mixer regression all pass. All 38 tww and
  3 sms-port patches apply to temporary clean copies of the pinned sources;
  both patch stamps match. Existing dependency checkouts are preserved.
- sms-port patch 0003 opens SDL paused, primes the existing 52 ms queue and
  then unpauses outside its mutex. SMS_AUDIO_STATS=1 logs cumulative missing
  device frames every ten seconds; --audio probes enable it. It does not
  claim to eliminate load hitches: probe-oDQ0o0 reports three startup gaps,
  331 ms total / 279 ms largest, with no additional gaps in the next observed
  intervals. This run was concurrent with a headless startup diagnosis and
  is not a final isolated performance measurement. WAV data alone cannot
  measure device underruns because it records produced, not consumed, PCM.
- PROGRESS.md documents the first-island objective through departure, real
  successful tests, limitations and reproduction commands. GitHub publication
  is explicitly authorized by the user; no game data will be uploaded.


2026-11-15 milestone: normal startup hands control to Link:

- Patch 0039 repairs BMG header getters and text offsets used by JMessage,
  copied cutscene metadata and 16-bit wait/font-size tags. The synthetic BMG
  header check fails before this patch and passes afterwards. Probe-Y93Dgb
  traverses the awake dialogues but waits forever at final camera cut 347.
- Patch 0041 recovers typed event argument readers, PAUSE completion/timers
  and the absolute FIXEDFRM branch using the extracted retail assembly. Trim
  string tags no longer depend on the host's word order. Relative FIXEDFRM
  and the other missing handlers remain explicitly unsupported.
- Patch 0040 recovers common dispatch for non-positional UI/dialogue sounds
  0x0800..0x083f, based on retail 802A8268..802A850C. The first float is pitch,
  the second volume; user data goes to Actor, signed reverb to port 9, camera
  ID is 4. Handles use the existing 24-slot pool and retire through JAudio.
  Broader seStart remapping and bgmStart are not implemented by this patch.
- Probe-U7Jmuo: real title/file selection, newly named game, COMPLETE illustrated
  opening, original awake movie/dialogues, final camera, demo=0/0, keyboard
  movement distance 166.342. Explicit SUCCESS and wrapper status 0. Headless
  deterministic/dummy-audio test: not an audible performance validation.
  Local screenshot: run-pc/normal-start-verified.png.
- Diagnostics now sample expensive player/camera reads, preserving actual
  execution counts and keyboard hold durations. GDB ignore_count changes from
  a stop callback proved unreliable and were removed. A deliberate diagnostic
  interruption of probe-G8CBCW is not a game regression. Do not edit a sourced
  GDB script while a probe runs: GDB may resume reading it after run returns.
- GDB probes propagate child failure status. The success flag lets crash.gdb
  finish a passed gameplay probe without labelling its breakpoint as a crash.
- Audio underrun diagnosis probe-uUbjVb stopped at the first missing output
  samples: game thread waits in gx::glt::mainWait -> shaderSetup, while the GL
  thread is reading programCacheLoad. That host wait does not service emulated
  interrupts. This is a specific cause to fix, not proof every hitch has it.
- TWW's DSPADPCM_FILTER and DSPRES_FILTER are native u16 arrays. The inherited
  mixer setup consumes numeric u32 pairs, first coefficient in the high half.
  Patch 0043 packs the tables at TWW's TARGET_PC setup boundary; casting their
  addresses directly swapped AFC predictor pairs and resampling taps on x86.
  check/test_dsp_tables.py links the actual TWW constants and native mixer,
  checks a synthetic AFC frame against hand-calculated samples, and proves
  either unadapted table changes the rendered output. The decode-only mixer
  hook falls back to default predictors when coefficient [1][0] is zero, so
  it alone would have hidden this defect; the regression uses render too.
- TWW_CAPTURE_TOWER=sample/all enables consecutive XFB and active-view camera
  captures in normal_flow.gdb. A changing stage name does not mean its camera
  exists yet; guard the pointer. CameraInfo.mCameraPos/mCameraTarget are saved
  camera state, not the live view, and were zero in these startup probes.
- The user's visible test after 0043 confirms menu/page sounds and the absence
  of the reported cinematic flashing. Link action voices, roll SFX timing and
  shoreline wave pacing remain open. Link's voice selectors/dispatch are still
  empty, independently of the conversation camera blockers.
- Entering Grandma's house requests LinkRM point 1 room 0 wipe 10. The native
  crash was SIGFPE in JUTFader::control: dOvlpFd_FadeIn requests startFadeOut(0).
  Patch 0044 treats this as an instantaneous fade, preserving positive ramps.
  The production fader test fails before the change and passes after it.
  room_transition.gdb tests the same destination/wipe through an injected
  next-stage request: loading and movement pass, but this does not test the
  door interaction or story progression. Keep that distinction in reports.
- After 0044 the user entered Grandma's house and reached the Hero's Clothes
  presentation. A second crash asserted groupID < 16 in JPAEmitterManager:
  JStudio's adaptor_do_PARTICLE read a raw big-endian selector as native int.
  Patch 0045 fixes that read (0x000001fa had become 0xfa010000, group 250).
  The isolated clothes_scene.gdb regression checks the selector against its
  bytes, validates the emitter group/resource, and reaches hero clothes and
  keyboard movement after the scene. It injects a next-stage request, not a
  complete normal-story path. The roll SFX timing is now user-confirmed good;
  Link voices and shoreline animation pacing remain open.

## Outset conversation checkpoint (15 Nov 2026)

- 0046 removes only the overly broad room-44 event-order filter from the
  --outset diagnostic route; keep entrance setStartDemo suppression. Aryll's
  post-clothes omedeto is automatic, so a Talk prompt is not expected here.
- 0047 implements FIXEDFRM @STARTER/oo with actor attention/yaw. Independent
  numeric checks for all three omedeto cuts pass within 0.15 game units.
  Beware cSGlobe's inconsistent getter/setter names: V(angle) writes the
  horizontal component while U() reads it. Do not swap those calls casually.
- 0048 recovers GETITEM wait 27 / transition 5 / completion from retail
  800BBD88..800BC360. The actual engine earns telescope inventory slot 0=0x20.
- 0049 restores TALK's retail event wrapper (saved type, style slot 3).
  talktoCamera is a functional PC implementation ONLY for message cut 0,
  not a matching reconstruction of 8016D8D0..80170434. It frames actual
  interlocutors using style dimensions and a 20-frame triangular transition.
  Ready flags only follow completed framing. Other cuts report unsupported.
  Added candidate yaw search with both speakers' sight lines after seeing
  the tower pillar obscure Link in the first successful capture.
- probe-b4vlMv status 0: real clothes, automatic Aryll events, telescope,
  following dialogue, control and 234.527 units keyboard movement. Isolated
  next-stage requests skip travel, not story flags. tools/aryll_story.gdb
  repeats this and additionally checks actual TALK timing and target view.
- No full-menu-through-telescope route or later rescue/boat verified yet.
  Manual C-stick, background music, item fanfare and Link voices still open.
- Final obstacle-search build: probe-sRpzuV status 0, post-telescope movement
  234.549. Frame 4500 shows both speakers clear of the pillar. TALK observer
  checks finite view, no early ready flags, frame-20 ready and target center/FOV.
  LQEGlr failed before run due to GDB's int/long symbol spelling; corrected.
- All 49 patches apply to a clean pinned archive and reproduce both camera
  source files byte-for-byte. Existing dependency trees were preserved.
- Final normal-start regression probe-QXA2ta status 0: title/file/name, full
  opening and tower awake, control and 166.342 units movement. Headless, dummy
  audio; no new claims about audio fidelity or completion of all Outset.

## Functional PC manual camera (patch 0050)

- manualCamera was empty while C-stick input/mode selection already worked.
  Added a PC orbit implementation using existing mWork scratch storage, real
  attention anchor, style angular limits and entering distance. Native mode
  selection, L recentering, DMC and the post-engine collision pass are reused.
  This is NOT a matching reconstruction of retail vertical zoom/FOV coupling
  or its special combat/vehicle branches. Keyboard tested, physical pad pending.
- probe-IPp5jo status 0: outdoor orbit both ways, pitch, >360 degree wrap,
  idle without drift, walking and L. Ae4oIf caught reversed style endpoints
  in LinkRM (MM18=35/0 vs MM01=0/30); fixed min/max normalization. That was a
  debugger assertion failure, not a native crash. nZDFXv then passes exterior
  and interior, including stationary Link while rotating the view.
- A close-wall capture showed Link's hair filling the view. Enable transient
  flag 0x10000000 in manualCamera so Run's existing PlayerHideDist=70 check
  sets attention status 0x20. Native preparation clears it every frame and
  player drawing restores the model after the camera moves away / exits.
- Final visible probe-kFU55x status 0, normal clock / muted audio. Observer
  checks finite views, bounded angle steps, style pitch limits, stable idle,
  Link movement, close-view hiding (100 frames) and no lingering hide after L.
- probe-VX57Ad status 0: manual input before Aryll's automatic scenes, earned
  telescope, completed TALK and 234.579 movement. This precedes only the last
  model-visibility tweak. tools/aryll_story.gdb now includes that rotation.
- All 50 patches apply cleanly and reproduce d_camera.cpp byte-for-byte.
  Original game window was left alone; diagnostic windows close after tests.
- Final expanded observer probe-Pkl2hc status 0: simultaneous walking/orbit
  passes outside and inside (471.885 degrees per combined-input segment),
  plus 111 close-view hiding checks and restoration after L. No code changes
  after the final visible test; this only expands the input regression.

## User regression after the normal startup / 0051 and sms-port 0005

- User observed flashing in title and first Aryll conversation, story-movie
  audio stutter, black view after I and no movement after drowning/respawn.
  Prior orbit tests started sideways and missed the first-up subjective path.
- MbsFJx catches invalid camera values after entry into mode 4 / SX31. Ordinary
  subjectCamera never set ready: the completion block was incorrectly nested
  inside telescope flag 0x10. Retail 80170FC0..80171000 makes it common.
  Moved it, bounded the reciprocal, corrected target/extended-target guards
  from 80171310..80171498, and used whole spherical assignments to avoid the
  known getter/setter naming mismatch. e7ba1F passes ordinary subjective view
  plus Q exit. PC C-stick up now enters orbit like every other direction.
- bN1DwK first-up orbit passes. A76HLF: keyboard-only first up, up swimming,
  natural restart and 306.431 units post-respawn movement, status 0. u940tc
  was interrupted due to an observer requiring land after walking off the
  respawn bridge; that was not a native crash. camera_recovery.gdb covers it.
- XGetImage captures of the original live X11 window confirmed alternating
  complete and mostly-black partial pictures (RGB mean about 77 vs 21).
  Internal headless title captures did not expose this presentation defect.
- sms-port 0005 moves GXPC_EndPresent into the GL presentation callback after
  swap. The EFB must be bound before any next queued guest draw, rather than
  only after the waiting caller resumes. Keep swap targeting the window and
  keep audio IRQ servicing. Production-function ordering test fails before,
  passes after (SDL/HDR/minimized). JTmT0c window captures no longer contain
  the partial-black frames; title/dialogue user confirmation still pending.
- User requested arrow-key camera: project PC bindings override main stick
  to WASD and camera to arrows/IJKL. Honor explicit SMS_BINDINGS and local
  run-pc/bindings.txt. Both launchers use them; normal_flow uses A for left
  menu navigation. c8fJRc validates arrow camera with stationary/walking Link
  in exterior and interior, status 0. Controls on a physical pad unchanged.
- Original log copied before launching again. Its 60 SDL stats samples report
  zero underruns. Do NOT claim story-movie audio stutter is fixed on that
  evidence; decoded/audio scheduling gaps still need measurement.

## Follow-up: presentation isolation, telescope allocation and stream analysis

- User reported title flashing again after the first sms-port 0005 candidate.
  Current patch drains GL with audio service, then syncs the presentation with
  serviceGuest=false and restores EFB before releasing the command. Eeu9DA
  title/file menu observer exits 0; 19 valid file-menu window captures remain
  complete. Capture process outlived the window (XBadDrawable), not a game crash.
  Human verification of current title/conversation presentation is pending.
- 0052 preserves both reusable equipment solid heaps on PC. The user crash
  attempted a 0x108-byte model allocation from a zero-capacity heap: unused
  dummy allocations may be elided before adjustSolidHeap. blwa07 exits 0,
  earns telescope, observes both capacities 0xe610, creates equipment model
  and verifies finite initial scope view. Opt-in TWW_TEST_TELESCOPE_MODEL=1
  assigns the earned inventory slot directly; assignment UI is not tested.
- Guided telescope/postman tutorial is NOT validated. Experimental controllers
  did not complete its aiming/zoom stages; these are not successful tests and
  do not alone establish a native tutorial bug. Missing Sturgeon is AJ1 with
  empty creation/logic and excluded actor profile, not a targeting-camera fix.
- Opening story uses StreamLib title.afc. Independently decoded disc bytes
  match 409600 runtime samples per channel exactly. Eight-second internal
  mix comparison shows stable 15.610..15.611 s offset, with no lost chunks.
  Internal WAV precedes SDL playback and cannot prove audible continuity.
  Stream buffer retains >1 s; GDB-induced output starvation is not evidence
  of the user's cause. User explicitly confirms sound cuts, still unresolved.
- Final clean reproduction passes 52 tww / 5 sms-port patches, comparing
  d_camera.cpp, d_a_player_main.cpp and all three changed GL platform files.
  Presentation ordering regression and shell syntax checks pass.
- Follow-up real-device measurement: keyboard startup on isolated card, GDB
  detached as opening music starts, then 30 s with no debugger/captures. All
  six subsequent SDL reports show zero underruns, queue 56..69 ms. Test process
  was meant to terminate afterward, but process-group termination missed the
  detached inferior (PID 6225). User reported audio without a window; explicitly
  terminated it, confirmed no tww processes, and fixed the local runner to check
  and terminate its detached PID. This does not validate host/device delivery
  or disprove user-reported cuts; no audio fix is claimed.

## Telescope story progression: 0053 and 0054

- 0053 restores Link body pitch in subjectCamera's actor-angle path (retail
  80171128 reads 0x2B4), and separates pitch/yaw during target lock at
  8017144C / 80171578. Build and clean patch reproduction pass.
- Digital aiming controllers oscillated; those experiments are not successful
  tests. Existing left Ctrl HALF_TILT support is now documented, but keyboard
  telescope progression is not yet validated.
- KpMk8u reaches telescope stages and real zelda_fly with native pad-axis input,
  then blocks on unsupported TALK 12. Interrupted deliberately to fix framing.
- 0054 adds retail shoulder-shot family TALK 11/12/30/31, swapped actors,
  eye-height offsets, immediate readiness and 55/65 FOV (8016F978..8016FCC8).
  Actual dialogue for cut 12 is exercised; other three remain unvisited.
- 5fUimA exits 0: earned clothes/telescope, model creation, tutorial phases
  1/2/3/4, real zelda_fly, following TALK 12 ready/FOV55, control return and
  234.382 units of walking. No forced quest flags, actor positions or camera
  readiness. Parent driver still requests diagnostic stages and assigns earned
  item to X; new helper uses native g_axis input and normal keys, no game logic.
- TWW_TEST_TELESCOPE_STORY=1 with tools/aryll_story.gdb imports the reproducible
  tools/telescope_story.py observer. Once equipped, monitor every frame even
  when the tutorial repositions Link; never gate it on old walking distance.
- These are headless deterministic muted tests, not visual/audio verification
  or a full normal playthrough. User prioritizes story and currently finds
  sound correct. Audio output explicitly disabled in all recent story probes.
- BdLGzo failed due to an observer UnboundLocalError, not a native crash;
  A7jD2v/Oq922E controllers were interrupted; gnQLD2 stopped to silence tests.
  No completion claims for them. Final clean reproduction: 54 tww / 5 sms-port.

## User-requested stop: Orca continuation, 0055, pending UNITRANS

- Session explicitly stopped by user; do not launch further tests until resume.
  Last active debugger PID 29293 was interrupted; probe-gImrnP exits 255
  deliberately. All newly created source/scripts and diagnostics retained.
- 0055 adds FIXEDFRM @PARTNER/oo using event getPt2, per retail 800B0858..74.
  The entrance event participant is the DOOR, not Orca or message talk partner.
  flFuZa/gImrnP numerically verify actual Eye/Center transforms and readiness.
- cvG47g repeats telescope full success (234.382 walking) then requests Ojhous,
  preserving earned state. Camera observer initially assumed NPC/player only;
  fixed by caching daKnob00_Execute(i_this), not inlined execute(this). AKoHU0
  was an observer error due to that inline wrapper. These are not native crashes
  or successful training runs. Existing Aryll STARTER observer is disabled only
  when Orca continuation begins because it assumes Aryll's actor.
- Local earned native savedata: run-pc/orca-regression/earned-savedata.bin,
  0x778 bytes from cvG47g after genuine telescope/fly/dialogue events. Diagnostic
  replay run-pc/orca-regression/replay.gdb restores that savedata then requests
  Ojhous point 0 room 0. Not a complete normal traversal, not shipped as save.
- flFuZa/gImrnP reach correct Orca dialogue 0x965 -> 0x966 -> 0x94e, choose yes,
  enter plmoveAction. Sword is still 0xff; NO sword/training completion claim.
- Last diagnosis gImrnP: active event 1 Ji1_linkmove. CAMERA staff 8 cut 24
  UNITRANS is unfinished (flag 52 false, ready0/0/0). Link cut11 001n_wait,
  TIMEKEEPER cut16 WAIT, Ji1 cut20 03_dummy and DIRECTOR cut22 FADE all done.
  uniformTransEvCamera is still an empty stub at d_ev_camera.cpp:209.
  Arguments: StartCenter=(-245,16,-38), StartEye=(411,414,-428), StartFovy=51;
  Center=(-245,-29,-38), Eye=(411,369,-428), Fovy=51, Timer=60, BSpCurve=0,
  BGCheck=0. Implement real transition/ready/completion, never skip event flags.
- Experimental tools/orca_story.py continuation is opt-in via TWW_TEST_ORCA=1;
  training input code unvalidated because UNITRANS blocks before lessons.
  TWW_ORCA_SAVE_CHECKPOINT optionally retains earned savedata locally only.
- Native PC build succeeds; clean 55-patch reproduction matches d_camera.cpp
  and d_ev_camera.cpp; local tww patch stamp updated. sms-port 5 unchanged.
  Renderer/audio not changed this continuation; all probes explicitly muted.
- Local research retained: orca-regression/initial.gdb, replay.gdb,
  unitrans-retail.txt; shared disassembler user-camera-regression/disasm-retail.py.
  No extracted assets, saves or recordings are added to Git.

## 2026-11-18 resumed: Orca sword training passes (0056 / 0057)

- User resumed and requested maximum story progress, recording errors.
- 0056 absolute Cartesian linear UNITRANS: retail 800B3334 readiness on entry;
  800B3390..3410 ratio (frame+1)/Timer, return true at frame>=Timer. Actual
  60/30 frame trajectories verified every update by native FinishBreakpoint,
  including Eye/Center/FOV/readiness/return. Other variants remain explicit
  unsupported: spline, relative actors, BG correction, non-unit Cushion.
- New blocker: Ji1_BadSpeak CAMERA cut43 FIXEDPOS, @PLAYER/n, CtrCus0.2,
  Eye(0,-50,-400), BGCheck2. Message0x96f stayed at status1 because camera stub
  never became ready. 0057 follows retail 800B2680..2B60: eye resolved once,
  attention target followed with CtrGap/CtrCus, radius/timer/optional bank.
  Only player target/relative eye implemented; actual correction shots verified
  numerically including center smoothing, eye position and ready/completion.
- Controller corrections: no attack while walking in for first two lessons;
  counterattack prompt is mDoStatus (dActStts_PARRY), NOT mAStatus. No native
  gameplay patch/quest flags/counter edits to make attacks accepted.
- sOYoKv: UNITRANS passes, exposes FIXEDPOS; server restart interrupted driver,
  no final status artifact. bwWCN6 advances through spin but old controller
  waits on wrong prompt; external timeout then 10s kill gives137. A private
  experimental GDB continuation updated controller after SIGINT and got one
  genuine parry before kill; not a pass. Removed auto-continue from replay;
  current checked-in replay honors stop/timeout normally.
- IKgLHt exits0: phases0..5 accepted, real sword presentation creation,
  saved selected sword0x38, final dialogue, control and236.439 walking. Both
  camera numerical observers pass, including real rejected-hit dialogues.
  This is a muted headless diagnostic using native savedata earned in cvG47g,
  requesting Ojhous; not menu/door traversal or visual/audio validation.
- Added tools/orca_replay.gdb requiring explicit TWW_ORCA_CHECKPOINT; no save
  bundled. TWW_SWORD_SAVE_CHECKPOINT optionally retains genuinely earned state
  after success. TWW_TEST_FOREST=1 chains experimental ForestStory, stage A_mori
  point0 room0 layer-auto, keeping sword/flags; no combat counters modified.
- DOJypj is currently repeating training to retain earned-sword-savedata.bin
  and enter forest. Forest code unvalidated; no Tetra rescue/departure claim.
- Local FST/room inspection confirms A_mori ACT0 contains Zl1 and3 Bk actors;
  Omori is the Deku tree area, not Outset forest. Only local disc data parsed,
  no assets uploaded. list-forest.cpp initially missed -I sms-port/src for
  port_host.h; fixed helper include path, not a game-build regression.
- Diagnostic data loops now reject unsigned0xffffffff/out-of-table indices;
  prior debug output '=type2443' was an observer overread, not proven corruption.
- Clean57-patch reproduction matches both camera files and Link; local stamp
  updated. sms-port remains5 unchanged. Native build succeeds.

## 2026-11-18 forest intro and first fight (0058 / 0059)

- Earned sword checkpoint comes from DOJypj's real six lessons; replay requests
  A_mori explicitly and is not a full menu/door run. No item/kill/event grants.
- 0058 implements default quadratic UNITRANS B-spline keys 0,0,1,1 and Cushion.
  Independent per-frame oracle passes forest 70/20-frame curves. Linear Orca
  60/30 regression t43LWK exits 0 (camera-only mode, not all lessons again).
- jnxL8V chased carried airborne enemies; 0QSOGQ hit terrain on a direct path.
  ow9Aej follows 17 local waypoints using actual movement, reaches first fight
  but AMORI_DEMO_2 cut34 RESTOREPOS was empty; bounded timeout, not native crash.
- 0059 follows retail 800B76C8..7E00 distance/visibility/lineBGCheck selection,
  saved views 0/1/pushPos, player target, FarTimer interpolation. Dest9 and
  nonplayer targets remain explicitly unsupported. NearTimer and Cushion are
  read by retail but not used in its actual restoration branches.
- DJRkq4: restore mode2/timer30 returns control; ordinary attacks deplete ground
  Bokoblin502 health to0 and delete it. Afterwards controller sticks at terrain
  near (4097,3365,-1241) chasing carried enemies. Rescue remains unverified.
- Added optional TWW_FOREST_FIRST_FIGHT to finish a bounded first-fight regression,
  finite-view/readiness observer for RESTOREPOS and bird-path diagnostics for
  subsequent runs. Native build succeeds; clean59-patch application exactly
  matches camera and Link source files. Local stamp updated after comparison.
- All tests muted/headless; no visual quality or sound fidelity claim. Local
  earned saves, extracted assets and route coordinates are not published.

- Closed regression orpEW2 exits0: curved intro, actual navigation, RESTOREPOS
  finite view/readiness completion at30 (also called at31), first actual kill.
- DJRkq4 timeout124 final GDB commands failed because replay.gdb was edited
  while open; do not edit active .gdb scripts again. Logs before timeout valid.
- Birds remain gated by unk_2F2 (room switch), not yet running carry/path logic.
  FnFrxq tries a postfight input route to TagEv activation zone; no forced switch.

- FnFrxq timeout124 confirms no native crash; path tried a352-unit climb and
  stayed below the ledge. KdgVt8 tests an alternate <=245-unit portal route.
  Link inputs only; no camera/position/switch/health writes. Warnings for raw
  cut*_pos resources need future fidelity investigation despite first kill.

- KdgVt8 intentionally SIGINT/255 after stalling at route6/28. l0vHkS repeats
  with continuous analog steering and no periodic A/roll while navigating:
  faster real movement, same unreachable step. Intentionally SIGINT/255,
  diagnostic succeeds: wall-type2, counter2, full stick1, wall catch height170;
  target ledge245 exceeds reach. Do not modify Link HIO to pass the test.
- Next: find authentic route via smaller steps/jumps (static room geometry
  alone omits dynamic obstacle collisions), activate TagEv, then prove both
  carried Bokoblins, Tetra rescue and return. No new native blocker demonstrated
  beyond the fixed RESTOREPOS. No active probes left after these stops.

- Manual forest run resolves route uncertainty: user reaches two-enemy ambush.
  Native failure is FIXEDPOS @TARGET unsupported; player demo2 remains while
  Bokoblins land (action31) and attack (action4/5). User closes window normally.
- 0060 maps @TARGET to event mPtItem/item partner per retail800B09B8..09DC,
  selected by TagEv demoInitProc(type1). Build and clean60-patch match pass.
  Encounter not yet validated. Isolated tests failed for driver reasons:
  LlsNRk inline wrapper lacked this; DeaUGt/elli8d positioning hits obstacle,
  so no encounter completion proof. Switched to user-controlled visible replay.
- Current manual log forest-manual/run.log; previous evidence preserved as
  before-target-fix.log. Audio enabled only for user's visible play window.

## Manual 0060 success: Tetra rescue, Aryll abduction, beach yuukaigo blocked

- User confirms both later Bokoblins defeated, Tetra rescued, bird abducts Aryll,
  then stuck talking to Tetra at beach by pirate ship. Live capture confirms
  close-up Tetra at beach; no full continuous visual observation was made.
- Native log confirms enemy508/509 health0 and transition A_mori -> sea;
  0060 FIXEDPOS @TARGET validated by this manual continuation.
- Stopped GDB50486 intentionally with SIGINT; appended-only diagnostic commands
  preserved source-file offsets and successfully exported actual earned native
  savedata (1912 bytes) to forest-manual/beach-earned-savedata.bin. It is NOT a
  full scene snapshot; replay of beach event from it still unvalidated.
- Active event250 yuukaigo; camera staff988 cut3366 UNITRANS, start8912 true,
  done false. Arguments BGCheck3 Center(55,-46,90) Eye(-137,30,228) Fovy60,
  RelActor @STARTER, RelUseMask --oo, TransType1, Timer10. Current implementation
  explicitly rejects relative/non-Cartesian UNITRANS, leaving Link demo2/proc4.
  Zl1 WAIT complete, other staff wait; do not attribute to dialogue code yet.
- Window/sound closed; native source unchanged after 0060 build and clean60
  reproduction. Next implement this spherical relative transition from retail,
  verify event/control and proceed toward shield/boat. No island-complete claim.
