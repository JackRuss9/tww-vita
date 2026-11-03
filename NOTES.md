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
