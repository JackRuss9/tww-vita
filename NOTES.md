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
