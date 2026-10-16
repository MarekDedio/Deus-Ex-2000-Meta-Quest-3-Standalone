# Pinned original-instruction sine lookup profile

2026-10-16. This is generated mathematical data, not extracted game assets or
executed commercial code. It makes an explicit original-instruction rotation
table available to ARM64 consumers of the verified Actor/Brush coordinate
builders. It does not instantiate native GMath, start a world or enable Move.

## Authority and provenance

Original Core.dll SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Static FGlobalMath constructor `1014431E..10144346` executes:

1. FILD signed32 index0..16383.
2. FMUL Double bits`3F3921FB54442D18`.
3. FSTP Double argument.
4. appSin `10123240`: FLD Double; FSIN; return retained result.
5. Caller FSTP Float into the16384-entry table.

No portable libm call, Float intermediary, cardinal-zero edit, symmetry fold
or radians expression replaces those boundaries. The half-turn value is a
small nonzero residual, deliberately retained.

The committed numeric profile is explicitly pinned:

| Property | Value |
| --- | --- |
| Profile | `retail-core-x87-fsin-intel-306a9-nearest-pc64-v1` |
| CPU | GenuineIntel, CPUID leaf1 EAX`000306A9` (local i5-3470) |
| x87 control | `037F`, nearest-even, PC64, exceptions masked |
| Secondary measured control | `027F`, nearest-even, PC53 |
| PC53/PC64 result differences | 0 of16384 final Float words on that CPU |
| Canonical payload | 16384 uint32 words, little-endian,65536 bytes |
| Payload FNV1a64 | `EFA5F38AF2167897` |
| Payload SHA256 | `B8D756044134EFB1BBEDBE2B842138D6B8D8DC970EF1A69EDD0D03CB07ECE10C` |
| Generator source SHA256 | `1BCBCE3DF91B7EE7599341060CFA6CB391A65A173D391DE34F9BF0B4E2616C24` |

The source digest uses its generated UTF-8/LF text. A Git checkout configured
to expand CRLF must normalize line endings before comparing that digest.

Core's constructor does not set its incoming control word. CRT startup
`1016F0EE..1016F0FD` calls `_initterm` over`101A7000..101A78A4`; GMath thunk
`10105BA0` occurs at entry72 (`101A7120`). Exact original static-init control
state is not proven. Complete measured PC53/PC64 equality narrows that gap on
the pinned CPU, not on all x87 implementations. Neither a universal FSIN
claim nor a captured original process/table is asserted.

## Runtime form

`native/quest_original_sine_table.h` contains the complete constexpr uint32
payload, constexpr bit-cast Float array and explicit `Table()` span. Target
builds need no FSIN, libm, host generator, downloaded asset or runtime I/O.
The complete16384 source words are generated data, not another formula.
The array occupies65536 bytes; compile-time conversion does not require a
runtime stack buffer. Builders still take an explicit table argument:

```cpp
const auto coords = QuestVr::BuildOriginalBrushCoords(
    location, rotation, prePivot, scale340, scale32c,
    QuestVr::OriginalSineProfile::Table());
```

This explicit profile does not authorize fake Actor/UClass identity,
native-field mapping, a default identity matrix, stale captured coordinates
or using registration bounds as final narrow-phase collision. Its future
native engine consumer must establish the actual object/virtual dispatch,
read current typed fields, capture once per Model call and join the caller's
transaction/work budget. Original physics/startup integration remains.

## Reproduction and readback

Configure/build the existing desktop workspace, then build target
`quest_original_sine_table_generator`. This standalone owned source executes
only its own CPUID/x87 instructions. It pins the vendor/signature above,
saves/restores the caller control word and generates both precision modes
before emitting any data. Wrong CPU/compiler/architecture returns77 without
a table; a PC53/PC64 disagreement is a refusal, not a silent replacement.

`quest_original_sine_table_generator.exe --profile` prints provenance,
canonical FNV, counts and explicit authority limits. `--block0` through
`--block31` (or `--block N`) each emits512 words:64 lines,8 zero-padded
lowercase hex words per line, comma terminated. Concatenating all32 blocks in
order reconstructs the exact Bits initializer. Preserve header metadata and
the explicit accessor when applying regenerated source data. No executable
writes a source file as a side effect.

The initial generation was collected in32 validated blocks: exactly512 words
per block,16384 total and independently calculated canonical FNV. A second
complete generation matched every word. A separate source readback parsed
all16384 words and hashed65536 canonical bytes to the SHA256 above. This
guards truncation, chunk ordering and source-copy mistakes, not just the
generator's own claimed fingerprint.

## Verification

`quest_original_sine_table_test` verifies every Bits/Values/span word, finite
range, cardinal/residual values, canonical fingerprint and explicit-table
Actor/Brush/wrapping-angle consumers on every target. On GNU x86 with the
exact pinned CPU it independently regenerates every entry with separately
written hardware instructions, compares all16384 words and separately
generates PC53 entries. Other CPUs explicitly skip only that exact-profile
hardware comparison; platform-independent integrity/consumer checks remain.

Initial targeted result:81943 checks,11 platform-independent angle cases,
16384 exact hardware comparisons, zero PC53/PC64/profile differences.
Strict ARM64 syntax checks pass for both consumer test and the generator's
explicit unsupported-platform path. No original DLL/EXE was loaded/executed.
Coherent full host build passes. CTest120 passed/135 registered/15 explicit
owned-data skips/zero failures in34.79s.
Local ignored logs are `artifacts/original-sine-*-20261016.log`.
