# First Training: native SingleLine support-query prerequisite

2026-10-20. Only `Maps/00_Training.dx` is the actual-map integration target.
This is the query required by original FindBase, not its support callbacks,
attachment lifecycle, a Falling scheduler, live crate destruction or complete
Training. The prior raw-Mesh checkpoint is documented in
TRAINING-RAW-SHAPE-STORES.md; no original commercial EXE/DLL is loaded/executed.

## Original contract

Read-only static Engine.dll1039AED0..1039B13B establishes SingleLineCheck:

- Call the same receiving ULevel.MultiLineCheck with CheckActors equal to
  bool(flags&0x17), actual native slot-zero LevelInfo only when bit4 is set,
  and the final extra byte forwarded unchanged. Actual LevelInfo ancestry is
  checked before the world provider runs.
- Traverse the returned linked order without another sort. For each candidate,
  exclude the source itself and its current Owner ancestors. This is not the
  candidate's Owner chain, its Base chain, or a cached source ancestry.
- Preserve mutually exclusive class precedence: LevelInfo requires04, Pawn01,
  Mover02, ZoneInfo08, all others10. LevelInfo also derives ZoneInfo, but cannot
  fall through into the ZoneInfo branch when04 is absent. For Others only,
  optional bit20 additionally requires bProjTarget OR(bBlockActors AND
  bBlockPlayers), with the original lazy field reads.
- Return the first accepted packet, copying every field. On a genuine miss
  change only Actor=None and Time1; retain all other caller words. There is no
  IsBlockedBy or floor-normal threshold for FindBase's flags0x17.

## Implementation and ownership

`quest_single_line_trace.h` implements this complete detached selection with
lazy providers. The result owns the collector arena, so local one-based Next
words remain usable only while that owner lives. No raw UObject address is
inferred from a local index. Positive traversal caps are portable safety policy
and can only be tightened. Owner visits accumulate across all candidates, and
every active selection action borrows the enclosing native-work callback;
there is no nested/root-reset allowance. Provider failure leaves the caller
packet unchanged and throws, never fabricates a clear result.

`portable_retail_single_line.inl` binds the service to the actual current native
Level/lookup generation, live qualified Owner/Bool fields and actual UClass
pointer ancestry. It calls RetailMultiLineLevel, not the presentation-active
diagnostic anchor wrapper. A single root-owned native hit mark encloses the
collector, selection, capacity accounting, qualified arena adoption and final
packet validation. The same collision root covers late refusals. Returned
vector capacity remains charged through lazy providers, then hands off to the
existing native arena without double charging. Mandatory pop is nonallocating
and precedes caller publication and any future SupportActor callback.

The accepted packet's Next becomes an opaque root-qualified word and is
already expired after pop, matching the native temporary-pointer lifetime.
It must never be traversed after return. Detached diagnostics expose only its
nonzero presence, not a usable capability or numeric native address. Misses
retain the seeded Location/Normal/Primitive/Item. World extent Item remains
explicitly unknown when the original Model provider did not write it.

The new diagnostic is rollback-only. It cannot publish begun play, change a
Base, select a floor cosmetically or supply a new save format. FindBase remains
unsupported until original support dispatch and retail SetBase are integrated.

## Verification boundary

The detached selector currently passes2397027 checks/93 explicit refusals,
including655360 independent byte/class/flag cases,61440 mixed logical-link
cases,120 tied permutations,17 raw-packet controls,24 exact lazy transcripts,
59 provider failure prefixes and14 shared-ledger cases. Strict ARM64 unit
syntax passes. The unit source oracle uses independent category truth tables
and logical permutations, not production selection/sort helpers.

Actual first-Training tests independently filter prior MultiLine packets using
detached actual class ancestry and declared Owner/Bool fields. They check the
same-world crate/NPC/mover/world identities, exact hit/miss packets, class/flag
laziness, source exclusion, hash/RNG/tag preservation, Model Item provenance,
empty marks/arenas, late geometry failures and shared-work cuts. The strengthened
focused run passes162561 checks/23 explicit refusals. It requires positive
Pawn/Mover geometry witnesses and nonzero selected Next, N-1 refusal/N success,
and a second same-root query exhausting the first query's remaining allowance.
The source map has no authored nonempty Owner edges. A clearly NONAUTHORED
dormant checkpoint fixture using actual Doctor1 and both crates separately
checks real two-hop Owner providers, then restores the exact original state
with two explicit Load publication revisions. It is not authored ownership or
SetOwner gameplay. No additional campaign map is loaded. The complete first-map
regression also passes1536139 checks/127 explicit refusals:1308 actual native-slot
actors,120 collidable nondeleted actors,478 registration links and174 independent
provider predictions. Full snapshot/save/revision/GC/RNG rollback passes. This
is still not complete world startup, automatic gravity, live crate destruction
or loot, or headset evidence.

The reviewed actual-Level adapter passes strict NDK27 ARM64 runtime syntax and
Android native compilation/link. The coherent full host build passes. Initial
CTest passes155 executed/179 listed,24 explicit owned-data skips and zero
failures in80.97s. Final CTest including the detached attachment protocol also
passes:156 executed/180 listed,24 explicit owned-data skips, zero failures,
45.75s. Skips are not actual-map or headset evidence.
Repeated actual startup still refuses at the deliberately unimplemented
WorldBases/FindBase slot631 LightSwitch0 boundary:529945 instructions,39425
writes,151787 shared native work with explicit262144, not exhausted. Both
attempts restore the full root and all40 source files remain unchanged. This
expected CLI exit2 is unfinished startup, not success or a reason to bypass
SupportActor. Diagnostic durations38.9101s/37.9423s occurred alongside tests;
they are not frame times or controlled performance measurements.

Ignored evidence files use `artifacts/training-single-line-*-20261020.log`.
Standalone generated executable: `artifacts/training-single-line-unit-20261020.exe`.

## Next original behavior, still unfinished

FindBase sweeps eight units downward with actual cylinder extent, observes
Hit.Actor, suppresses an unchanged Base or a hit already based on self, then
dispatches target.SupportActor(self), or self.SetBase(None,true) for no hit.
World startup subsequently clears actual-LevelInfo support via
SetBase(None,false). Retail SetBase requires exact pointer/count/callback order
and independent wrapping StandingCount bytes, not port reverse-list counts.
Those lifecycle changes and live support/falling/destruction/usable contents
remain required before a playable-physics APK or tutorial completion claim.

At this SingleLine checkpoint, `quest_actor_base.h` was implemented as
preparation. The subsequent runtime replacement and retirement of inferred
reverse-list/count authority are documented in TRAINING-ACTOR-BASE.md; FindBase
remains unwired. Fresh static review distinguishes captured
counter targets from reloaded event receivers: decrement captured oldBase, then
reread self.Base for Detach; write captured newBase after Detach; compare that
captured newBase with freshly reread self.Level; increment the captured target,
then reread self.Base for Attach. BaseChange follows only for nonzero sendEvent.
Prospective Base traversal precedes equal-base return. Counts wrap as bytes,
even when inconsistent with raw Base fields; no reverse-list inference occurs.
Partial provider effects join the caller's external journal, not an invented
local transaction. Standalone strict GCC and ARM64 checks pass; generated unit
passes18309 checks/3003 independent register-oracle cases/72 refusals,8 golden
transcripts,102 callback cases,28 provider failure prefixes and29 explicitly
synthetic outer-root controls. These are not runtime, map, support or falling
acceptance. Its CMake test is included in final shared verification.
