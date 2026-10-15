# Training weapon and crate gaps (2026-10-13)

## Current follow-up (2026-10-16)

Tracked original keyring/knife/crowbar assemblies and the quiet trigger are now
accepted by the user; see HELD-ITEM-VISUALS.md. This supersedes the historical
invisible-hand description below, not the remaining original equip/Fire work.

Native sound264 recipient routing now executes with real UObject indices,
caller/native Level authority, exact hearing/BSP checks and synchronous original
ClientHearSound events. Bound-player native audio still requires a prepared
commit-only consumer. In isolated actual Training damage, None-instigator fatal
hits pass sound/AI and fail at Engine.Fragment.CalcVelocity:7, missing VRand252;
the whole checkpoint/revision is restored. Legal nonnull Pawn-instigated damage
still reaches the earlier MakeNoise512:428 boundary. The generic live damage
helper has deliberately not been widened to fake decoration health/destruction.

The new owned native279 diagnostic calls genuine Destroy, never Destroyed by
hand. Explicit isolated bBegunPlay=true/bStartup=true controls prove the actual
Containers.Destroyed/FastTrace548/super callback prefix (68/65 script instructions
versus dormant1), repeated-destroy invariance and world removal without fake loot.
With original Contents=Lockpick retained and bStartup=false, both crates reach
Containers.Destroyed:681 VRand252, before Spawn278:758, and completely roll back.
Source bytes/timestamps and GC remain unchanged. These fixtures do not establish
world startup or a real possessed player. Tests pass257 checks/two expected
contents refusals, plus152 original damage controls.

Next work includes original VRand rejection/normalization and RNG rollback,
MakeNoise/native clock/hearing, fragment lifecycle/GetMeshTexture1013, actual
contents births and landing/Base support. AI713/715, GetPlayerPawn720 and
SetPhysics3970 already have runtime adapters; storing Physics is still not
falling simulation. Full melee destruction, loot, gravity and whole committed
scene publication remain unfinished. No replacement APK for this backend batch.

## Historical initial report / audit

After testing the installed streaming/inventory replacement, the user reports
freezes basically fixed and browsing working, but an invisible selected crowbar,
rejected damage on breakable crates and a floating crate stack. These are open
gameplay features, not accepted as fixed by the browsing checkpoint.

## Held weapon and attack

RenderPlayerVisuals submits only the body and fixed hands. Its hand source is
a restricted WeaponHandsTex subset of NanoKeyRing's original first-person mesh;
it does not load or submit the selected weapon. Selection feeds an older
technical-name-based trigger attack (crowbar12damage/2m), not PutInHand,
BringUp/PutDown, original attack animation or a saved equipped-item identity.

Original WeaponCrowbar instead supplies PlayerViewMesh=Crowbar,
PickupViewMesh=CrowbarPickup and ThirdPersonMesh=Crowbar3rd. HitDamage is6,
maxRange/AccurateRange80 Unreal units, bHandToHand/bInstantHit true and
AmmoName=AmmoNone. Original WeaponDamageType produces 'shot' for this case.
ProcessTraceHit invokes Other.TakeDamage with actual owner Pawn, hit position,
momentum and damage type; real skill/AugCombat state affects its multiplier.

A held visual must resolve typed properties of the actual collected actor,
preserve original materials/pose and use an audited per-asset grip transform.
Do not invent a CrowbarPOV path, reuse NanoKeyRing-specific source vertex IDs,
or turn flat-screen camera offsets into a VR grip transform. First-person assets
may contain hands; partition deliberately so the right hand is not doubled.
Selected-item presentation alone is not proof of authored equip/unequip.

## Damage refusal

DamagePortableRuntimeActor explicitly accepts only Engine.Pawn. Therefore all
breakable decorations currently return TARGET NOT DAMAGEABLE irrespective of
their original properties. Widening that guard to use the same generic health
accumulator would still lose original decoration HitPoints, invincibility,
thresholds, damage types, DropThings/Frag, destruction callbacks and Contents.
The original damage/destruction chain must execute transactionally with its
actual spawned actors and save/rollback semantics.

For this pair bStatic/bInvincible/bOnlyTriggerable/bExplosive are false and
minDamageThreshold is0. The effective handler is the auto state's
DeusExDecoration.Active.TakeDamage, not a class-level function. A fatal hit
calls DropThings, emits sound/noise, spawns WoodFragments through Frag and
invokes Destroy, which dispatches Containers.Destroyed. The first concrete
current script blocker is DropThings' BasedActors iterator native306; the
iterator host previously implemented only AllActors304. The new native306
implementation now uses the retail live ULevel scan, direct Base and actual
UClass pointer ancestry, not the pin's cached BasedActors/name filter. The
original two crates' empty-base DropThings passes in a read-only integration
probe; this is not fatal damage, support physics or a gameplay fix. Additional contracts
include MakeNoise512, PlaySound264, GetMeshTexture1013, fragment lifecycle and
FastTrace548. The latter is evaluated before false bGenerateTrash, so it cannot
be skipped. Any failure must roll back earlier damage, bases, births, random
state, collision and destruction rather than partially disappearing the crate.

## Floating stack

The relevant authored Training pair is:

| Actor | Original position X,Y,Z | Physics | Mesh vertical extent |
| --- | --- | --- | --- |
| CrateBreakableMedGeneral1 | 1022.110,-275.103,-103.900 | PHYS_Falling | -24..24 |
| CrateBreakableMedGeneral2 | 1023.128,-274.079,-16.900 | PHYS_Falling | -24..24 |

Mesh Origin, RotOrigin and actor PrePivot are zero; DrawScale is1. The original
center separation87 minus combined half-heights48 leaves39 Unreal units,
approximately0.743m, before native falling/support simulation. Base is None.
Both original instances have HitPoints10 and Contents=Lockpick. Thus the gap
is not explained by an incorrect decoded crate pivot or scale. The current
Quest path has no automatic falling/support physics to settle the stack.

SupportActor assigns the standing actor's Base; BaseChange restarts Falling
when a pushable object loses its base. DropThings releases supported actors
when the lower crate breaks. Native falling/contact invokes Landed, stops the
non-pawn and assigns its support; storing the Physics byte is not simulation.
Contents spawning is permitted only when !Level.bStartup, creates one actual
Lockpick, sets Falling and randomized velocity, then enters Pickup/dropped.
It does not directly append an invented inventory entry. Nearby
CrateUnbreakableMed5 is genuinely invincible and must remain distinguishable.

Acceptance needs original damage thresholds/invincibility, correct contents
births and collection, a visible controller-aligned held crowbar, attack timing,
falling/landing/support after the lower crate is destroyed, and save/load of
these states without stale visuals, duplicate contents or streaming freezes.
Original player ownership, script startup and general physics remain incomplete.

## Runtime prerequisites added in the current batch

The detached held-item descriptor reads the actual collected Inventory actor,
including its inactive picked-up identity, through typed linked declarations.
PlayerViewMesh/PickupViewMesh/ThirdPersonMesh, scale, Skin/eight MultiSkins slots,
Int damage/ranges and bHandToHand retain separate optional failures and exact
source provenance. No asset-name guessing, package I/O, equipment-state writes
or script execution occurs in this query. Current mutable class defaults must
not leak into old map instances; frozen birth patches remain separate.

Class property access now requires actual authored Class.Children/Next
membership rather than Outer/name coincidence. Malformed chains publish no
valid prefix, and orphan/foreign field reads roll back preceding writes.
The new qualified actor-property reader preserves exact declaring-field
identities needed by the original focus-name/status UI.

These are prerequisites. They do not themselves add a weapon draw path,
original player equip/Fire, fatal crate destruction, spawned contents, gravity,
or a playable lamp. See ORIGINAL-LAMP-CONTRACT.md for the lamp's original Frob,
native sound, ScaleGlow and dynamic BSP-lighting gaps.
