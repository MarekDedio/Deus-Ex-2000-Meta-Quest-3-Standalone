# Original GOTY native world-startup operations

Read-only static DLL/script audit, 2026-10-11. This supplements
`WORLD-BOOTSTRAP-CONTRACT.md`; it does not prove a booted or playable campaign.
No original executable or DLL was executed. All addresses are static PE image
virtual addresses, not portable object offsets.

## Evidence and authority

Original owned files under `D:/Steam/steamapps/common/Deus Ex/System`:

| DLL | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| DeusEx.dll | `B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04` |
| Extension.dll | `93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C` |

NDK27 `llvm-readobj` identifies exports/imports and `llvm-objdump` reads bounded
bodies. PE section mapping and import ordering independently connect vtable
slots and IAT operands to exact symbols; objdump's nearest unrelated symbol
heading is not function identification. Logs and the read-only PE inspection
script are ignored artifacts in `artifacts/world-native-startup-20261011/`.
Earlier complete Actor/Core InitExecution bodies are in
`artifacts/spawn-frame-audit-20261011/`.

## InitExecution and native subclasses

AActor virtual `+0x38` is exported thunk `0x10302194`, body `0x103deb40`.
Its only initialization call is imported UObject::InitExecution at
`0x103deb6d` through IAT `0x10599ad0`. The remaining body asserts:
state frame exists and belongs to self, XLevel exists, slot0 exists, self.Level
matches actual XLevel.Actors[0], and Level is nonnull. It does not initialize
the actor's latent timer, reflected timer fields, native Spawned data or zones.

UObject thunk `0x1010286f` reaches body `0x10150750`. It asserts an authored
Class, frees any existing frame, requests a fresh `0x28`-byte allocation from
GMalloc and publishes the following frame fields:

| Original frame offset | Initialized value |
| --- | --- |
| `+0x00` | FStateFrame vtable |
| `+0x04` running Node | receiver.Class |
| `+0x08` Object | receiver |
| `+0x0c` Code | null |
| `+0x10` Locals | null |
| `+0x14` | null |
| `+0x18` selected StateNode | receiver.Class |
| `+0x1c`, `+0x20` positive ProbeMask | all 64 bits set |

It ORs UObject.ObjectFlags with HasStack `0x02000000`, preserving other bits.
There is no HasScript, pre-existing HasStack or begun-play gate. **Frame `+0x24`
is not written in this constructor/body.** A malloc allocation is not proof
of zero; no deterministic native latent action can be inferred from that
unassigned word. Portable Continue while Code is absent is an explicit portable
policy, not a recovered GOTY initialization value. Once original state-entry
or label operations clear latent action and enable code, their separately
audited semantics apply. The actor-owned latent timer must not be reset merely
because its state frame is replaced: neither this Actor nor Core body writes it.

The native Actor vtable census covers all **47** exported Engine Actor classes,
all **8** exported DeusEx Actor classes and Extension.PlayerPawnExt. Every
InitExecution slot uses the same base AActor operation. DeusEx entries use
thunk `0x1001af74` through IAT `0x10027250`; Extension uses `0x100545f4` through
`0x10059290`, both independently mapped to AActor::InitExecution imports.
This resolves the native override question for these campaign Actor classes,
not arbitrary third-party DLLs or unrelated UObject-derived native classes.

All those native Spawned slots use the empty AActor base except **AMover**.
AMover slot `+0x74` is thunk `0x10303d87`, body `0x103af790`--`0x103af7ca`:
copy the three Location words from Actor `+0x114` to `+0x488`, then the three
Rotation words from `+0x120` to `+0x4b8`. These destinations correspond to the
mover's base-transform storage by layout, but this audit does not claim an
independent original property-name/offset registration proof. It has no other
write, callback or superclass call. This native Spawned is a **birth** step,
not part of LoadMap's InitExecution loop: do not apply it to all loaded movers.

The inspected portable `InitializeActorExecution` establishes class-backed
modern frames and HasStack, journals the changes and retains actor-owned latent
timer. Its portable local-revision/transition bookkeeping remains portable
implementation detail; the original Core body does not clear unrelated UObject
flags. State natives must not fabricate a frame outside these owned call paths.

## SetActorZone: exact world `(actor,1,1)` behavior

ULevel thunk `0x10302a77` reaches body `0x1039b940`--`0x1039be94`.
The second integer is the callback-suppression/test flag; the third is the
force-refresh flag. The initial actor deletion-bit check returns without work.
The following statements concern the observed world call with both flags1.

1. Validate the actual native Level actor slot0 as LevelInfo. This is not a
   scan for some reflected LevelInfo or the actor's mutable Level overlay.
2. **If receiver is actual slot0**, write its Region directly as
   `{Zone=slot0, iLeaf=-1, ZoneNumber=0}` and return. Its Location does not
   enter BSP traversal. No Pawn fields or callback is touched.
3. For other actors, detect native Pawn ancestry. Force-refresh first resets
   Region and, for Pawns, FootRegion/HeadRegion to that same fallback tuple.
4. Call root UModel::PointRegion with Location and actual slot0 fallback.
   Publish all three Region fields even when Zone identity is unchanged.
5. For Pawns, resolve FootRegion at Location minus CollisionHeight on Z,
   then HeadRegion at Location plus EyeHeight on Z. Publish all tuple fields.
6. If the actual LevelInfo.NetMode is not3 and Pawn.PlayerReplicationInfo
   exists, publish Pawn.Region.Zone to PRI.PlayerZone. NetMode3 skips this
   update. This check is after the three region calculations.

**No zone-change script callback executes for flags `(1,1)`.** With the second
flag zero and changed Region.Zone, the body instead calls oldZone.ActorLeaving
(actor), actor.ZoneChange(newZone), publishes Region, then newZone.ActorEntered
(actor). FootZoneChange(newFootZone) and HeadZoneChange(newHeadZone) likewise
occur only when their identity changes and the second flag is zero, before
publishing their own tuples. Their native FNames are independently identified
by exported ENGINE_ActorLeaving/ActorEntered/ZoneChange/FootZoneChange/
HeadZoneChange at `0x105836e8`, `0x105836ec`, `0x10583708`, `0x105836cc`,
`0x105836c8`. Callbacks can mutate gameplay, so ordinary tick zone changes need
that separate ordering contract and transactional/reentrancy coverage.

This body has **no** bWaterZone-to-Physics4 assignment or SetBase call.
Existing portable `SpawnZone` additionally applies those birth-specific
operations and cannot be used unchanged for world `(1,1)`. The inspected
native SpawnActor body likewise has no recovered direct bWaterZone-to-Physics4
assignment. Actual movement-time zone callbacks are separate script paths;
portable water behavior is not evidence of a native world initialization step.

## Native GameInfo birth arguments differ from script execSpawn

LoadMap `0x1038d55a`--`0x1038d59d` passes the selected Class and zero for every
other native SpawnActor argument: FName InName=None, Owner=null,
Instigator=null, Location and Rotation all-zero, final Actor/template argument
null and final two integer flags0. The original exported signature is
`SpawnActor(UClass*,FName,AActor*,APawn*,FVector,FRotator,AActor*,int,int)`.
Native SpawnActor unconditionally writes birth.Tag=Class.Name at
`0x10395230`--`0x10395236`; this overrides serialized Tag defaults.

Script execSpawn also passes null final Actor argument and two zero flags,
but captures its receiver.Instigator at `0x103e3805`. After successful native
SpawnActor returns, `0x103e3837` writes the optional script SpawnTag onto the
birth, including an explicit or omitted Name None. LoadMap makes **no** such
post-return Tag write. Therefore a world GameInfo birth is not faithfully
expressed by calling the script Spawn declaration, even with null Owner and
zero transforms. Reuse transactional native birth internals with an owned
direct-world argument mode; do not mutate LevelInfo.Instigator temporarily or
claim class-default Tag preservation. Script Spawn's post-return Tag semantics
are independently implemented and covered by callback, later-argument alias,
notification replacement, rollback and cold-save regressions. Its ordinary
inputs are detached as each expression evaluates; direct world birth still
bypasses that wrapper. This does not resolve the other generic birth gaps.

### Before-begun native birth does not return at the script gate

The exact stack layout is Class `+0x08`, FName `+0x0c`, Owner `+0x10`,
Instigator `+0x14`, Location `+0x18..0x20`, Rotation `+0x24..0x2c`, optional
Actor/template `+0x30`, integer flags `+0x34` and `+0x38`. The exported
signature and `ret 0x34` independently confirm this layout. In particular,
`+0x30` is **not** an integer flag. The final `+0x38` flag controls the
Role/RemoteRole swap at `0x1039533f`; LoadMap passes zero.

There is no `bBegunPlay` early return in the native SpawnActor body. Actor
ProcessEvent suppresses individual script events before begun-play, but the
following successful native birth steps remain reachable in order:

| Step | Original address / condition |
| --- | --- |
| SetOwner, publish Instigator | `0x103954e4`, `0x103954ee` |
| native InitExecution, native Spawned | `0x103954f6`, `0x103954fd` |
| script Spawned, PreBeginPlay, BeginPlay | `0x1039551c`, `0x10395534`, `0x1039554c`; suppressed before begun-play |
| abort if marked deleted | `0x1039554f` |
| native SetActorZone(actor, !template, 1) | `0x10395559..0x1039556c`; null template makes flags `(1,1)` |
| LastRenderTime = -10 | `0x10395572` writes native Actor `+0x34` |
| script PostBeginPlay | `0x1039558e`; suppressed before begun-play |
| native CheckEncroachment(actor, Location, Rotation, 0) | `0x103955d6`, unless integer `+0x34` is nonzero |
| destroy and return None if encroached | `0x103955e8` |
| script SetInitialState | `0x1039560f`; suppressed before begun-play |
| native physical FindBase | `0x1039569c`, only eligible no-Base / bCollideWorld Pawn, Decoration or Inventory with Physics0 or5 |
| script PostPostBeginPlay | `0x103956b6`; suppressed before begun-play |

Ordinary GameInfo ancestry is none of Pawn, Decoration or Inventory and its
native Spawned is the empty Actor operation, so its direct LoadMap birth still
needs its actual Location-derived Region and native LastRenderTime before the
later world InitExecution pass. Returning immediately after InitExecution
incorrectly loses these unconditional native operations. Reusing world
SetActorZone `(1,1)` for this null-template GameInfo call is supported by the
exact flags, not by an assumption that all dormant births are complete.

The zero encroachment flag does not remove CheckEncroachment. Its exported
thunk `0x10302536` reaches body `0x1039a350`: at `0x1039a39a` it tests Actor
`+0x1dc` mask `0x0d` (bCollideActors1, bBlockActors4, bBlockPlayers8). If
all three are clear and native Brush (`+0x178`) is null, it returns false
immediately through `0x1039a8fb..0x1039a90d`. SpawnActor cleared Brush at
`0x1039535c`. The flag mapping is independently checked by exported native
execSetCollision (`0x10301393` to `0x103e1900`), which reads masks1/4/8 in
the authored SetCollision parameter order NewColActors/NewBlockActors/
NewBlockPlayers. **bCollideWorld bit2 is not part of that early mask.**
Thus a portable noncolliding GameInfo path can establish this exact false
fast path. Any of the three tested bits requires the remaining exact native
collision contract or explicit refusal; refusing bCollideWorld as well is a
conservative narrower policy, not the recovered mask. Nonnull brush/mover
conditions beyond that fast path are not fully audited here.

The separate native tail also remains: conditional Level
`+0x108/+0x104` in-tick birth queue (`0x103956b9..0x103956f4`) and the
actual LevelInfo.SpawnNotify chain (`0x10395775` onward) run after PPP. Script
SpawnNotification is still individually gated by Actor ProcessEvent, not a
reason to skip that native chain wholesale. This audit has not established
a complete before-begun SpawnNotify return contract. The deliberately
nonpublishing probe must scope or refuse those
unsupported conditions rather than assert a fully reproduced native birth.

ULevel::Tick independently resolves the queue's guard: exported thunk
`0x10301104` reaches body `0x103a51a0`. Tick sets native Level `+0x108` to1
at `0x103a5271`, and clears it at `0x103a5b50`. At `0x103a56a6` it resets the
`+0x104` list head; it ticks live slots and then processes pending records from
that head at `0x103a56cd..0x103a570b`. Each record is `{Actor*, next*}` and its
actor is ticked if Actor's `bTicked` bit `0x200` differs from Level `+0x10c`
tick parity. The latter parity is toggled at `0x103a5b4a`.

The SpawnActor queue predicate tests **only Level `+0x108`**, not an Actor bit
or class. If set, it requests eight bytes aligned8 from GMem through the
independently mapped FMemStack::PushBytes IAT `0x10599bd4`, stores birth and
old head, then publishes the new head at `0x103956ee`. This is an in-tick
newly-spawned actor queue, not an unconditional startup queue. A diagnostic
that owns a standalone non-ticking world transaction can scope this branch
out, but must not be invoked from a future native tick without supporting
or refusing that active queue. This does not recover full Actor.Tick,
recurring newborn-tick ordering or native memory-stack lifetime.

### Existing generic script Spawn limitations

These world corrections do not silently establish the public script Spawn
contract. The existing generic path still needs separate regression work for:

- completing native birth operations before begun-play, rather than returning
  at a gate that belongs only to individual Actor.ProcessEvent calls;
- removing any claimed recovered native water assignment unless independently
  supported by original evidence;
- calling eligible native FindBase/SetSupportBase **before** PPP, not afterward;
- exact CheckEncroachment and supported SpawnNotify/active-tick tail behavior.

The world diagnostic may use a narrower strict internal path and explicitly
refuse unsupported branches. Its rollback controls prove only that supported
prefix; they do not convert legacy script Spawn approximations into GOTY-native
behavior or make an incomplete world safe to publish.

## Root PointRegion traversal and prior defects

UModel thunk `0x1030391d` reaches body `0x103f5db0`--`0x103f5f38`.
Fallback is the nonnull caller-provided LevelInfo, leaf `-1`, zone number0.
Empty BSP returns exactly that tuple. For nonempty BSP, the selected side is1
for nonnegative signed plane distance and0 otherwise (the comparison constant
at `0x10429838` is double zero). The body uses the same side for:

- child `node.iChild[side]`, original node `+0x20`;
- leaf `node.iLeaf[side]`, original node `+0x38`;
- zone `node.iZone[side]`, original node `+0x34`.

If the selected child is `-1`, that side terminates immediately. A missing
front child on exact plane equality does not authorize taking a present back
child. Original root-outside/CSG flag bookkeeping updates an outside value but
does not alter this geometric side selection or final leaf lookup.
When UModel.NumZones (`+0x100`) is zero, result ZoneNumber is0 regardless of
node zone bytes; otherwise it uses the selected node zone byte. A null actor
in that zone's native 64-entry zone table falls back to slot0 without changing
leaf or zone number. The serialized count and null table slots must remain
distinct. Nonfinite points, cyclic/malformed indexes and allocation/work budgets
need explicit portable refusals; this audit does not import native unchecked
memory access or prove x87 rounding equivalence at nearly coplanar points.

The portable Region code inspected before the initial correction had four
specific mismatches: leaf0 used with zone1 and leaf1 with zone0; equality could
take the opposite child; empty BSP leaf initialized0; LevelInfo receiver had
no slot0 special case. It also selected fallback by a class scan rather than
the native slot0 identity. Generated tests should isolate each mismatch using
actual serialized UModel/ULevel data, including an intentionally misleading
secondary LevelInfo and overridden reflected Level binding.

The subsequent numeric/cache correction is implemented and independently
tested in ORIGINAL-MODEL-POINT-REGION.md. It removes the later Float X/Y/Z
arithmetic loop, retains PC64 Z/Y/X comparison without a distance spill and
uses the current loader-owned native Model/cache rather than a second root
selection. That PointRegion query is not the complete movement-time
SetActorZone coordinator or accepted original world startup.

## FindBase is not an overlap query or direct SetBase

AActor thunk `0x10301749` reaches body `0x103c9470`--`0x103c9566`.
It constructs an FCheckResult and calls actual native XLevel.SingleLineCheck
(ULevel virtual `+0xc0`) with:

- source/self;
- start = self.Location;
- end = self.Location minus `(0,0,8)` (constant `0x1042b148` is float8);
- extent = `(CollisionRadius,CollisionRadius,CollisionHeight)`;
- trace flags `0x17`, final byte argument0.

If the returned hit actor differs from existing Base, and a nonnull hit actor
is not already IsBasedOn(self), call `self.SetSupportBase(hitActor)`. No hit
means the null-support path. There is no pointer-sort or choice from current
overlap contacts, nor an audited floor-normal threshold in this body.
Native SingleLineCheck filtering, swept actor/BSP extent geometry, nearest-hit
ordering and tie behavior remain required separate collision contracts.

SetSupportBase thunk `0x1030146f` reaches `0x103c9390`--`0x103c940c`:
with nonnull target, resolve and execute **target.SupportActor(self)**;
with null, call **self.SetBase(None,true)**. The original Engine.Actor.SupportActor
script (`15` logical bytes, flags `0x802`) calls StandingActor.SetBase(self)
at PC9. Derived script overrides can change this outcome; replacing the event
with a direct native self.SetBase(hitActor) bypasses original gameplay.
LoadMap subsequently clears a resulting Base equal to actual LevelInfo with
self.SetBase(None,false), as audited in `WORLD-BOOTSTRAP-CONTRACT.md`.

Existing portable `SpawnBase` overlap queries, stable pointer sorting and
direct SetBase do not establish this FindBase contract. World AttachTag's
separate first-tag-match SetBase(candidate,false) rule is already known and
must not be confused with physical support discovery.

## Implementation and proof boundary

Implement recovered frame/zone operations inside the same outer world VM
transaction, not through independently committing public actor calls. Journal
frame/HasStack/local revisions, reflected region fields, PRI updates, births,
slots, native attachments, timer retention and RNG along with callbacks.
Reacquire live slots after script phases. Total work is bounded independently
of actor-array growth. A staged diagnostic that reaches unavailable physical
FindBase must roll back the successful prefix and report exact world phase,
slot/receiver/native/function/PC evidence; it must not publish bBegunPlay or
claim a running world. If no actor requires physical FindBase, the separately
unimplemented native actor-slot compaction/save-ordering boundary still requires
an honest terminal refusal and rollback. Passing generated regressions establishes these
specified operations, not automatic Training startup, real Login/SetPlayer,
player possession, UI ownership, ticking physics or full campaign playability.
