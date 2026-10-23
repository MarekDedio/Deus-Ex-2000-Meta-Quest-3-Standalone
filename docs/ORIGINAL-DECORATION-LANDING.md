# Original ordinary Decoration landing coordinator

2026-10-23. Source prerequisite for first-Training crate gravity, not enabled
falling or a headset fix. The original instruction/field audit is recorded in
[ORIGINAL-TRAINING-CRATE-FALLING.md](ORIGINAL-TRAINING-CRATE-FALLING.md).
No commercial executable or DLL is executed or loaded.

Follow-up: the same coordinator now includes original basic non-Pawn/
non-Decoration landing (failed Decoration cast103CF5F1 ->103D02AB). It skips
counter/reset/fit/Carcass providers, invokes selected Landed, rereads Physics
and uses SetPhysics0(original floor) if still2. This enables the correct native
branch for dropped Inventory without pretending it is a crate. Fixture gates
now pass10462 checks/129 refusal controls. Original explicit physFalling is
also runtime-bound; see ORIGINAL-ACTOR-FALLING.md for actual first-Training
gravity evidence, distinct from the earlier explicit sweeps recorded below.

`native/quest_decoration_landing.h` implements the recovered ordinary
non-Pawn/non-Carcass Decoration path of processLanded103CEF60..103D04DF using
caller-supplied live native fields, real Level.SingleLineCheck, selected Landed
and original SetPhysics providers. It creates no separate root, work allowance,
inventory grant, forced Base, floor snap or simplified landing callback.

The zone-bounce shortcut precedes class checks, preserves short-circuit x87
unordered comparisons, rereads the stored zone and performs Z/Y/X Float scratch
arithmetic before X/Y/Z raw stores. Ordinary landing retains the original five
query bound, initial0x37 zero-extent trace, four0x17 recovery traces, same hit
packet, retained extent, fresh endpoint radius/location reads and actual INT
clear results. Counter/sum/direction arithmetic wraps as original DWORD stores.
Recovery increments the live counter and returns with its velocity, without
calling Landed or changing Physics/Base. Normal landing resets the counter,
dispatches selected Landed, rereads live Physics, and calls SetPhysics0 with the
original by-value supplied floor only if Physics remains2.

Review found and fixed an input-aliasing bug: the supplied normal is now taken
by value at entry, before query reentry, not copied from caller storage later.
The regression mutates the external source normal during a fit query and proves
the selected event still receives the original packet.

Finite active arithmetic uses shared retained x87-PC64 dyadics and explicit
binary32 stores. Active NaN/infinity arithmetic and Float overflow remain
unimplemented branches and explicitly refuse; inactive raw NaN/signed-zero
classification is permitted. Pawn, non-Decoration and Carcass branches likewise
refuse when encountered, not through preflight of inactive providers. This is
not a complete implementation of every processLanded receiver/numeric domain.

The registered `quest_decoration_landing` host test passes10241 checks and112
refusal controls. Coverage includes all16 binary clear-result patterns, trace
packets/order, live query changes, wrapped INTs, counter>=5, callback mode/Base/
deletion changes, setter reentry, every shared work/write cutoff, raw pending
prefix and caller-fixture rollback, unsupported classes/numeric branches and
normal aliasing. Strict ARM64 syntax checking passes. These fixture checks are
not an original-runtime integration or an independent adversarial arithmetic
oracle for every rounding boundary.

```powershell
& artifacts/prerequisites-20261009/build/quest_decoration_landing_test.exe
```

Evidence: artifacts/decoration-landing-test-20261023.log and
artifacts/decoration-landing-strict-20261023.log.
The coherent full desktop build also passes; shared CTest passes175 executed/
207 registered with32 explicit owned-data skips, zero failures in76.35s.
The new landing test is one of the executed tests, not one of those skips.

At the earlier coordinator-only checkpoint, remaining work was to bind
authenticated actual native fields and selected event to the caller's VM
journal, reach the coordinator from real swept physFalling,
settle lower/upper Training crates onto true support, release the upper crate
after lower-crate destruction, advance actual loot falling, and verify committed
Quest input/save/render/audio. The registered host test was then the header's
only consumer; the later native-binding follow-up below supersedes that part.
No new APK is claimed.

## Native adapter and real swept-contact follow-up (2026-10-23)

The later source increment now includes portable_decoration_landing.inl in the
production host. The preceding unused-header statement describes the earlier
checkpoint, not the current source. Actual stored Region/Zone, authenticated
qualified fields, native current-Level traces, original selected Landed and
native3970 are bound on the same execution/root. The counter uses a native
no-animation-rebuild Int proxy. Three raw Velocity proxies are pre-admitted
before any intermediate word; a separate private raw ZoneVelocity capability
preserves inactive NaN classification without weakening ordinary VM/save reads.

`--sweep-training-crates` adds two explicit diagnostic MoveActor deltas after
real GameInfo/Login initialization and before unchanged normal pairing/Possess.
It never assigns a support, guesses a contact position, forces begun/state
flags or directly calls an event at an arbitrary coordinate. Each original
authored Falling crate receives delta(0,0,-128) through real Level.MoveActor;
only its actual floor collision (hit.Time<1, normal.Z>0.7, not deleted/bouncing)
can reach processLanded. The diagnostic still does NOT calculate a gravity
timestep or synthesize impact Velocity. Original zero Velocity remains zero.

Initial actual first-Training observation:

| Crate | Actual sweep result | Original landing/support result | Shared writes |
| --- | --- | --- | --- |
| Lower | Time0, normalZ1, actual world LevelInfo0; current Z-103.899864 already contacts floor, no move | selected DeusExDecoration.Landed; Physics2->0, Base=LevelInfo0 |14762->14790|
| Upper | Time0.300157398, normalZ1, actual lower crate; Z-16.8998642->-55.3200111 | same selected event; Physics2->0, Base=lower crate |14790->14822|

Both use one real initial fit query, normal non-recovery landing, zero counter,
original SetPhysics and actual stored zone LevelInfo0. No floor snap or direct
Base write is used. The unchanged later startup reaches the genuine DamageHUD
fixed-Name-array refusal (218605 instructions/24252 writes/native492023), then
the complete world/save/RNG/hash/client/lookup/Actor/GC root is undone and all41
owned sources remain unchanged. Discovery evidence:
artifacts/training-native-landing-discovery-20261023.log. This original collision/
support result is stronger than the coordinator fixture but still NOT automatic
falling, a committed world, live VR weapon input or headset acceptance.

Final repeated native runs reproduce both complete landing rows exactly,
including stored zones, pose/velocity, Base, counter, clock packets and shared
write prefixes. Native work is492023cold/491481warm of1048576, not exhausted.
The combined repeated command then executes genuine damage on both supported
crates and original player loot pickup in that SAME root:

- Lower crate before damage: Physics0/Base=actual world. Its fatal second hit
  invokes original DropThings, which calls actual3970 on its based upper crate.
- Upper crate before its first damage: Physics2/Base=None, with its original
  ten HitPoints. This proves support loss/restart from original callbacks,
  not a manual detach or a replacement inventory object. Its subsequent fall
  is not advanced by this diagnostic.
- Both deaths create six actual WoodFragments and one surviving Lockpick each.
  Original player transfer/first-fit grid placement and same-node NumCopies1->2
  merge still complete. Shared damage writes14822->14824->14989->14991->15133;
  pickup writes15133->15203->15241.

Combined repeats reach the unchanged later HUD boundary at224415 instructions/
24673 writes; native512377cold/511750warm. Every detached damage/loot/landing row
matches between repeats; complete root undo/all41 unchanged sources pass.

With shared write allowance14812, repeated interruption reaches original
DeusExDecoration.BaseChange PC0 opcode20 during the upper crate's nested
support callback. The lower landing has completed at14790; upper is entered
but returned=false/captured=false. Default after-values in that incomplete row
are not observations. Both repeats stop at203071 instructions/14812 writes,
native463111cold/462569warm. Complete undo restores both the first landing and
partial second movement/support. Normal no-command startup retains its exact
218253-instruction/24192-write/native491158 HUD boundary with no landing rows.
All cases preserve41 owned sources, full save/state/revision/hash/RNG/client/
lookup/Actor/GC state and genuine later startup refusal; no partial startup is
published. Exit2 means that observed startup boundary, not a completed startup.

```powershell
& artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe --owned-source-root 'D:/Steam/steamapps/common/Deus Ex' --sweep-training-crates --repeat 2 --native-work 1048576
& artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe --owned-source-root 'D:/Steam/steamapps/common/Deus Ex' --sweep-training-crates --collect-training-loot-via-player --repeat 2 --native-work 1048576
& artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe --owned-source-root 'D:/Steam/steamapps/common/Deus Ex' --sweep-training-crates --damage-write-limit 14812 --repeat 2 --native-work 1048576
```

Final evidence: training-native-landing-repeat-20261023.log,
training-native-landing-combined-20261023.log,
training-native-landing-late-20261023.log and
training-native-landing-normal-20261023.log in artifacts/. Strict ARM64 runtime/
CLI checking, focused host build and Android native compile/link pass on the
final source. No new APK is packaged from this backend-only milestone.
The final coherent full desktop build passes. Shared CTest passes175 executed/
207 registered,32 explicit owned-data skips, zero failures in49.62s. Separate
owned original crate destruction passes347 checks/two genuine contents
refusals. Evidence: training-native-landing-full-build-20261023.log,
training-native-landing-full-tests-20261023.log and
training-native-landing-crate-regression-20261023.log.
The separately rebuilt owned first-Training SetPhysics regression also passes
172882 checks/53 explicit refusals, including19 partial-write cutoffs, real
Base/count changes, native raw-word zeroing and unchanged full Doctor animation
clocks. This is a regression of that existing setter, not an animated Decoration
landing or gravity-phase claim. Evidence:
artifacts/training-native-landing-physics-regression-20261023.log.

The future loot landing path must not silently inherit this coordinator's
non-Decoration refusal. Static original103CF597..103CF604 confirms that a failed
actual Decoration ancestry cast jumps directly to103D02AB selected Landed,
without reading numLandings or running fit traces. This is a separate missing
non-Pawn/non-Decoration branch, needed by Inventory loot; it is not permission
to call a Decoration-only helper on a replacement crate-shaped item. Pawn and
Carcass branches remain separate as previously recorded.

## Binding metadata and journal contract

Existing owned metadata records/current validators authenticate scalar original
Actor.bIsPawn Bool flags0x2, Region Struct flags0x2->Actor.PointRegion,
Location Vector flags0x23, CollisionRadius/Height Float flags0x23, Velocity Vector
flags0x21, Physics enum Byte flags0x23->Actor.EPhysics; ZoneInfo.bBounceVelocity
Bool flags0x1 and ZoneVelocity Vector flags0x21; Decoration.numLandings Int flags0x2.
Every field is dimension1/baseField0 with its actual declaring Outer and
qualified Core metaclass. Read the stored PointRegion, not the public computed
Region facade. Its Zone member is Object<Engine.ZoneInfo>, flags0/dimension1.

Ordinary Vector decoding validates all components as finite. The bound
q.zoneVelocity therefore uses a narrowly authenticated raw native ZoneVelocity
reader to preserve inactive unordered comparisons, without globally weakening
VM/save finite policy. q.velocity reuses the actual raw Actor Velocity
capability so inactive old X/Y words are not spuriously rejected when only Z
is needed.

Original Actor.Landed has native0/flags0x800 and one required HitNormal Vector
(StructProperty/Core.Object.Vector, flags0x80/dimension1). Decoration.Landed is
native0/flags0x2 with the same packet, but its actual parameter is spelled
HitNormall. Selected DeusExDecoration.Landed is native0/flags0x2 with required
HitNormal PLUS ordinary locals Rot (Rotator) and HitSound (Sound). Authenticate
the required parameter subset, not variables.size()==1 or selected FUNC_Event.
Dispatch same-root state-aware ProcessEvent/CallEvent; never substitute the
empty Actor body. This metadata preparation is not native runtime acceptance.

Reuse RetailSingleLineLevel's actual clear0/1 INT and shared packet; landing
does not consume Item/worldItemKnown. Reuse ActorSetPhysicsNative for mode0 and
the original supplied floor, preserving callback deletion/mode reentry rather
than introducing an extra bDeleteMe exit. Pre-admit all three raw Velocity word
proxies before the first store, as existing3970 does.

The native numLandings INT write needs a narrowly authenticated no-clock proxy
patterned on WriteActorPhysicsByte. Generic ReferenceFor is NOT sufficient:
UpdateClockProperty currently rebuilds an existing actor clock for unrelated
generic property writes too. Journal the actual Int through the same execution,
Touch/storage/committed-state path without that reconstruction; do not broaden
global clock exclusions or invent a separate counter. Preserve full real
animation-clock bytes in the later native integration test.
