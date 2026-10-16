# Original native level clock (2026-10-16)

This is the clock prerequisite for MakeNoise512 and future original world
simulation. It is an explicit CPU-owned clock phase, not a complete world Tick,
world startup, possessed player, AI/noise implementation or new Quest build.

## Retail authority

Static evidence only: the commercial DLL is never loaded or executed.
Engine.dll SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Tick's exported thunk10301104 reaches103a51a0. Native ULevel time is a Double
at+dc; LevelInfo.TimeSeconds is a separate Float at+478. Owned Engine.u declares
TimeSeconds as scalar Float flags0 and TimeDilation as scalar Float flags0x21
(Edit|Net). Exact inherited declaring fields, not closest same-name shadows,
are used by this phase.

At103a5468..103a5487, the arithmetic order is:

1. Load incoming Float delta and multiply by Float TimeDilation in PC64.
2. FST the scaled delta to Float without popping/reloading the retained product.
3. Add prior native Double time to that retained product in PC64.
4. FST the sum to native Double without popping/reloading the retained sum.
5. FSTP the same retained sum to LevelInfo.TimeSeconds Float.
6. Only later clamp the stored scaled Float actor delta to Float0.005..0.4
   (bits3ba3d70a..3ecccccd), at103a54a8..103a54df.

Neither the scaled-delta Float spill nor the actor-delta clamp controls native
clock accumulation. The Float mirror is not necessarily Float(Double time):
prior1+2^-24 with delta2^-63 and dilation1 retains a sum just above the Float
midpoint; Double storage rounds back to the midpoint, but the separately stored
Float mirror rounds upward. Independent x87 tests exercise this case.

Original fresh LoadMap resets native Double and reflected Float to zero at
1038d74d..1038d7f1. Its already-begun restore branch copies the serialized
Float mirror to native Double at1038dc4e..1038dc5f. These are explicit startup
boundaries, not permission to infer native time from the mirror on every query.
Original ULevel.Serialize1039d594..1039d5c5 itself serializes a Float conversion
of native time. The port's exact Double checkpoint below is a deliberate native
continuation format, not a claim that the original save format retained64 bits.

## Runtime ownership and transaction

Only the actual serialized Engine.Level receives a loader-owned native clock
baseline. Metadata-only fixtures have no fabricated ULevel/time. Native slot0
LevelInfo and its immutable nativeXLevel link establish ownership; reflected
Actor.Level and XLevel patches do not redirect the clock, TimeDilation or mirror.

ReadPortableRuntimeLevelTime reads that native Double. The explicit
AdvancePortableRuntimeLevelClock entry performs this arithmetic phase and the
typed mirror write transactionally. It neither schedules actors nor starts the
world. The future exclusive CPU world coordinator must call it once per level,
not once per actor, and keep the other audited tick phases in order. No time
advancement or UObject access is added to the XR rendering thread here.

Native time has a separate transaction journal. Mirror writes join the normal
typed VM write/rollback budget; any later refusal restores both. An exact
clock-only mirror delta does not increment world geometry revision or re-decode
actors. Other actor/default/AI/collision/state changes retain normal publication.
The nonpublishing startup probe now journals its native reset/restore too.

Portable safety policy accepts finite nonnegative prior time and raw delta,
and finite dilation. Negative dilation is not silently clamped, but an actually
negative resulting time is refused. Nonfinite storage/Float overflow is refused
atomically, never saturated to a plausible time. Signed zero is preserved.

## Versioned continuation

Script codec11 appends a mandatory record with actual map-qualified native
Level identity and exact little-endian Double bits. Codecs1..10 remain byte
identical. Both nonmaterializing verification and materialization validate
identity, finite nonnegative time, byte/string/working-memory budgets and full
consumption. Existing RNG/state/decal/integer-array domains may coexist.

Runtime envelope16 identifies codec11. Envelope14 remains the existing mover
wrapper and envelope15 remains the quick-inventory wrapper; their nested payloads
can carry16. New clock state does not repurpose either existing wrapper number.
The untouched native+0 baseline is omitted, preserving old ordinary saves.
Nonzero time and -0 retain the explicit record. Restoring an older clock-absent
save resets native time to the loader+0 baseline, never merges current history
or guesses it from its independently mutable reflected Float. This is an
explicit portable legacy migration policy.

Restore validates exact actual ULevel identity and stages all allocating work
before any publication. Native time publishes with the rest of the complete
checkpoint. Read-only validation must not change either clock, actor state,
UObject roots or revision. Original packages remain read-only.

## Verification scope

Pure phase tests use independent x87 instructions plus retained-product/sum,
double-rounding, signed-zero, clamp and overflow controls. Codec tests include
independent golden legacy1..10 vectors, exact Double boundaries, randomized
roundtrips, coexistence, truncation, malformed values/identities and budgets.
Generated production sound/clock fixtures exercise the public native phase,
typed fields, geometry-revision suppression, full warm/cold restore, old-save
reset, argument/ownership independence and atomic refusal. The owned Training
probe also runs the phase with original field schemas and restores its complete
pre-clock baseline before testing both crates.

Terminal verification counts and current build status are recorded in STATUS.md.
At this clock-phase checkpoint MakeNoise512 remained separate work; its later
production implementation and audited cache/hearing/callback contract
is in ORIGINAL-MAKE-NOISE.md.

Final evidence: pure phase418883 checks/85883 safety refusals; codec8385/105;
generated production sound/clock2605/20; actual Training clock/crates479/4;
separate original contents347/2 and accepted NPC280519/26. Full host build and
strict ARM64 syntax pass. CTest104 passes/119 registered/15 explicit owned-data
skips/zero failures,27.67s. Logs are local ignored
`artifacts/crate-native-clock-*-20261016.log`. Review found and repaired an
initial uncharged raw mirror write before final verification: the final handler
uses the VM write path and proves zero-write-budget complete rollback. No new
APK or device result is claimed by these tests.

The separate original38-package startup probe passes and retains its exact
Mover.Move266 refusal at Engine.Mover.BeginPlay:166, Training slot126, with
repeated complete checkpoint/GC rollback. This verifies no new startup
regression; it does not establish a started world or player.
