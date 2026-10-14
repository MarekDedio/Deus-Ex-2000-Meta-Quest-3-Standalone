# Original startup primitives and pawn links

This increment supplies dependencies for real world startup, not a running
campaign. The production Quest loop still uses separate camera/gameplay/UI
containers and does not yet execute the audited whole-world bootstrap or own
a possessed JCDenton. See [WORLD-STARTUP-GAPS.md](WORLD-STARTUP-GAPS.md).

## Read-only authority

The user's original GOTY Core.dll SHA256 is
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Original binaries were not executed. Bounded NDK27 disassembly and compiled
script inspections are retained under ignored
`artifacts/startup-primitives-20261011/`. Addresses are original image VAs,
not portable layouts. The original Engine hash and world ordering are in
[WORLD-BOOTSTRAP-CONTRACT.md](WORLD-BOOTSTRAP-CONTRACT.md).

Core bodies: ANSI literal `0x10130890`, Unicode literal `0x10130990`,
ObjectToString `0x10131f00`, Min `0x10134a20`, Sin `0x10135d20`, Tan `0x10135e80`,
FClamp `0x101364e0`, spaced concat `0x1013ac30`, insensitive comparison
`0x1013b900`, Len `0x1013bad0`, InStr `0x1013bc40`, Mid `0x1013bdf0`,
Left `0x1013bfd0`, Right `0x1013c1a0`, and Log `0x1013cae0`.

## Implemented behavior and explicit limits

- VM FString transport is UTF8/WTF8, preserving UTF16 code units, paired and
  isolated surrogates and embedded-NUL storage. Indexing uses original UTF16
  units, not transport-byte count. ANSI zero-extends bytes; it is not CP1252
  or UTF8 input. ANSI literals retain1023 bytes while consuming the entire
  encoded literal; Unicode literals have no such original truncation.
- Tagged VM string ingress requires an exact compact-count payload and final
  terminator. It copies only the at-most-five-byte count before checking
  length and encoded allocation limits; malformed/trailing payloads fail.
  Existing presentation-string decoding remains separate and unchanged.
- Mid adds unsigned32 start/count before clamping and defaults to65535. Left
  and Mid zero-pad copied storage after NUL. Right constructs the C-prefix
  of the suffix instead. Len counts stored units; InStr searches C-prefixes.
  Concats retain all first-operand storage and append the second C-prefix;
  native168 inserts one space. Caps changes ASCII lowercase only; original
  unsafe embedded-NUL Caps storage is explicitly refused.
- Insensitive comparison uses an explicit default C-locale ASCII fold, not
  fabricated full Unicode case mapping. This is not Unicode name-table,
  localization or UI-font acceptance. Existing finite-value VM ingress still
  refuses NaN/Inf. FClamp preserves ordered/reversed bounds and signed zero;
  Sin187/Tan promote float input to double and store a float result. Retail
  appSin10123240 uses x87 FSIN; out-of-range absolute values >=2^63 leave the
  operand unchanged, which Sin preserves. In-range Sin uses portable double
  libm. This is not a bit-exact x87 FSIN/FPTAN claim across host/ARM math libraries.
- Known input-only primitives capture each value before later argument
  callbacks; unknown/native OUT operations retain their real aliases.
  ObjectToString validates actual identity and returns its canonical full
  package/outer path (null returns None). Native Rename/SetOuter is not
  implemented. Log uses original ScriptLog default or explicit Name category,
  a C-prefix message and a fixed format. Diagnostic output may remain visible
  after failed execution; world effects and writes still roll back.
- Pawn natives529/530 use the actual native XLevel slot-zero LevelInfo,
  not mutable reflected Level/XLevel. Add always prepends, including repeated
  insertion. Remove splices the first match and retains Self.nextPawn.
  Cyclic nonterminating traversals fail with bounded rollback. Original
  Pawn.Destroyed, not generic actor destruction, owns script removal.
- Spawn no longer runs an invented/reversed AttachTag rule. World attachment
  and physical FindBase are distinct phases; overlap-based SpawnBase is not
  verified original floor/slope/mover physics.

## Verification boundary

Generated controls cover input snapshots, precise string boundaries, original
clamp/slice quirks, native pawn topology, callback failure, warm/cold saves and
GC. Optional owned-data controls call actual Pawn declarations on Training
humans/robot and an Intro bird, and actual Engine.Pawn.Destroyed on an observed
empty-owned-resource branch. They do not skip authored peer/inventory teardown
and claim it was tested.

The original InitGame control explicitly spawns DeusExGameInfo before begun
play and calls its inherited original function. Options, logging and the actual
base Mutator birth must complete. OUT behavior, save replacement and rollback
are checked; unsupported optional dynamic mutator loading must report its
actual callee/native evidence and remove partial births/writes. This controlled
call is not a real LoadMap, level.Game binding, complete world lifecycle,
most-derived player possession, original inventory or campaign-playability proof.
