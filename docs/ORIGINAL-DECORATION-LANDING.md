# Original ordinary Decoration landing coordinator

2026-10-23. Source prerequisite for first-Training crate gravity, not enabled
falling or a headset fix. The original instruction/field audit is recorded in
[ORIGINAL-TRAINING-CRATE-FALLING.md](ORIGINAL-TRAINING-CRATE-FALLING.md).
No commercial executable or DLL is executed or loaded.

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

Still required: bind authenticated actual native fields and selected event to
the caller's VM journal, reach the coordinator from real swept physFalling,
settle lower/upper Training crates onto true support, release the upper crate
after lower-crate destruction, advance actual loot falling, and verify committed
Quest input/save/render/audio. No runtime adapter includes this header yet;
the registered host test is currently its only consumer. No new APK is claimed.

## Native binding follow-up

Existing owned metadata records/current validators authenticate scalar original
Actor.bIsPawn Bool flags0x2, Region Struct flags0x2->Actor.PointRegion,
Location Vector flags0x23, CollisionRadius/Height Float flags0x23, Velocity Vector
flags0x21, Physics enum Byte flags0x23->Actor.EPhysics; ZoneInfo.bBounceVelocity
Bool flags0x1 and ZoneVelocity Vector flags0x21; Decoration.numLandings Int flags0x2.
Every field is dimension1/baseField0 with its actual declaring Outer and
qualified Core metaclass. Read the stored PointRegion, not the public computed
Region facade. Its Zone member is Object<Engine.ZoneInfo>, flags0/dimension1.

Ordinary Vector decoding validates all components as finite. Thus future
q.zoneVelocity needs a narrowly authenticated raw native ZoneVelocity reader
to preserve inactive unordered comparisons; do not globally weaken VM/save
finite policy. q.velocity can reuse the actual raw Actor Velocity capability so
inactive old X/Y words are not spuriously rejected when only Z is needed.

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
