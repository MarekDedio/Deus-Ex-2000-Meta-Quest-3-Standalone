# Original Actor stasis and its native timing prerequisites

Static owned-GOTY audit, 2026-10-25, supporting the next original non-Pawn
Tick integration for the entire first `Maps/00_Training.dx`. The immediate
cohort is its genuine crates, surviving Lockpick and WoodFragments. This
document implements no native service, changes no compiler input, and does
not establish completed Actor/Level Tick, collectible loot, automatic gravity
or a playable Quest build. It follows the genuine main/blend animation work;
it is not another player-menu/Input integration task.

Implementation update2026-10-25: the native allocation/default-copy chain is
now independently closed in
[Model timestamp initialization](ORIGINAL-MODEL-ZONE-TIMESTAMP-INITIALIZATION.md).
Fresh exact native Engine.Model timestamps start at Float+0. The portable
loader now retains that64-slot transient baseline and the original InStasis
query has a same-root runtime binding. The separately implemented native
distance phase is recorded in ORIGINAL-LEVEL-PLAYER-DISTANCE.md. Original
renderer publications and complete Actor/Level Tick are still unfinished. See
ORIGINAL-ACTOR-STASIS.md for the implemented scope and verification.

Read alongside [the wider Tick audit](ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md),
[the implemented post-animation phase](ORIGINAL-NONPAWN-TICK-TAIL.md),
[the native level clock](ORIGINAL-LEVEL-CLOCK.md),
[PointRegion](ORIGINAL-MODEL-POINT-REGION.md) and
[native actor order](TRAINING-WORLD-ACTOR-ORDER.md).

## Source authority and reproduction

No commercial EXE/DLL was loaded or executed. External `objdump` inspected
static bytes, import/export tables and bounded disassembly. The owned files
under `D:/Steam/steamapps/common/Deus Ex` were rehashed read-only:

| Source | SHA256 |
| --- | --- |
| System/Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| System/Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| System/Render.dll | `607F1077C5CD3E25F8A4CDE1FFACF4CF9260AEBC92A72BE40721CD6955BD75D1` |
| System/Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Addresses and offsets below are original preferred-image PE32 evidence, not
portable pointers or permission to copy the native layout into runtime objects.
Reproduction, from an ordinary PowerShell session:

```powershell
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x1036BFE0 --stop-address=0x1036C06D 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A1AC8 --stop-address=0x103A1BF0 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A55C1 --stop-address=0x103A5698 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103AED30 --stop-address=0x103AED93 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103AD5FF --stop-address=0x103AD650 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10B1766D --stop-address=0x10B176A2 'D:\Steam\steamapps\common\Deus Ex\System\Render.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10B1936A --stop-address=0x10B193B5 'D:\Steam\steamapps\common\Deus Ex\System\Render.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10B25150 --stop-address=0x10B25189 'D:\Steam\steamapps\common\Deus Ex\System\Render.dll'
```

Additional bounded witnesses are identified in the sections below. `objdump
-p` authenticates the exported names; `objdump -s` reads the cited constants
and unbound import-table cells without resolving or executing them.

## Exact InStasis entry and lazy gates

Engine export-table index1131, ordinal1132,
`?InStasis@AActor@@QAEHXZ`, has thunk `10302013` and complete body
`1036BFE0..1036C06C`. Ordinary `AActor.Tick` calls that helper directly at
`103A1AC8`, before owner dependency, parity assignment or animation.

The original helper evaluates these conditions in order:

1. Read source native XLevel (`Actor+AC`), its native Double TimeSeconds
   (`ULevel+DC`), and source LastRenderTime Float (`Actor+34`). Compute native
   time minus that Float in x87 extended arithmetic and require an ordered,
   strict result greater than Double5 (`1042ADA8`). The `test ah,41` at
   `1036BFF7` returns false for equality, lesser and unordered results. It does
   not first read `bStasis` or a renderer visibility list.
2. Require source `bStasis` (`+28`, mask00200000), otherwise return false.
3. If `bForceStasis` (`+28`, mask00400000) is false, require Physics Byte
   (`+30`) to be literal0 or5, otherwise return false. ForceStasis bypasses
   **only this Physics test**, not the earlier time/bStasis tests or the later
   zone/distance/NetMode work.
4. Read the stored source Region.ZoneNumber byte (`Actor+D0`). Read the
   corresponding native Model zone LastRenderTime Float through the native
   Level.Model pointer (`ULevel+98`), at `Model+108+24*ZoneNumber`. Compute
   native Level Double time minus that Float and test ordered strict `>5`.
   A true result proceeds directly to the final NetMode gate. Otherwise read
   source DistanceFromPlayer Float (`+38`) and require ordered strict `>1200`
   (Double constant `1042AD98`); equality, lesser and unordered return false.
5. Read **source Actor.Level (`+A8`)**, then that LevelInfo's NetMode byte
   (`+518`), and return true only if the byte is literal0. Other byte values
   return false; this is not a blanket rejection of nonstandard enum values.

The constants' original binary64 bytes are `00000000 00001440` for5 and
`00000000 00c09240` for1200, visible with `objdump -s` over
`1042AD98..1042ADB0`.

This corrects an ambiguous description in the earlier broader Tick audit:
the final `+518` belongs to **LevelInfo reached through Actor.Level**, not to
ULevel. Likewise it is not implicitly the native Level's slot-zero actor.
`1036C054..60` explicitly loads `[source+A8]` and then `[that+518]`.

The live native class/provider boundary must remain the previously audited
one for the actual cohort: DeusEx.DeusExDecoration, Engine.Inventory and
Engine.Projectile resolve their Tick implementation to AActor. An arbitrary
subclass merely satisfying IsA is not evidence of an unchanged native virtual.

### The true-stasis Tick return

At `103A1AD1..103A1B04`, true InStasis causes Tick to check actual
`bTransient` (`Actor+2C`, mask2). Only when that flag is true does it then read
Role Byte (`+31`) and, for numeric Role>=2, call actual Level.DestroyActor
virtual+94 with source and force0. Every ordinary true-stasis branch returns
integer1. It does not assign source.bTicked or execute owner/animation/state/
timer/lifespan/physics work first.

False InStasis continues to actual Owner/bTicked versus Level parity, native
pending-list deferral and source parity assignment, before the animation
phases. Adding an active/bDeleteMe/worldRemoved early filter here would not
reproduce the audited callee. DestroyActor has its own actual gates.

## Declaring schemas and native authority to preserve

The full owned Actor field chain and native copy association are recorded in
the broader Tick audit. These exact scalar declarations must be authenticated
at the adapter, rather than selected by an unqualified leaf name:

| Declaring field | Kind / flags / constraint |
| --- | --- |
| Engine.Actor.LastRenderTime | FloatProperty /0 / no referenced type |
| Engine.Actor.DistanceFromPlayer | FloatProperty /0 / no referenced type |
| Engine.Actor.bStasis | BoolProperty /1 / no referenced type |
| Engine.Actor.bForceStasis | BoolProperty /1 / no referenced type |
| Engine.Actor.bTransient | BoolProperty /1 / no referenced type |
| Engine.Actor.Physics | ByteProperty /23hex / Engine.Actor.EPhysics |
| Engine.Actor.Role | ByteProperty /20hex / Engine.Actor.ENetRole |
| Engine.Actor.Level | ObjectProperty /2 / Engine.LevelInfo |
| Engine.Actor.Region | StructProperty /2 / Engine.Actor.PointRegion |
| Engine.LevelInfo.NetMode | ByteProperty /0 / Engine.LevelInfo.ENetMode |

All dimensions are1. Require actual declaring Outer, actual Core property
class, source descriptor/type/flags and constraints, inherited ownership and
correct live storage kind. Source Region must be the **stored** PointRegion,
not a freshly recomputed facade. Its exact three members are Zone:Object
constrained to Engine.ZoneInfo, iLeaf:Int and ZoneNumber:Byte, all scalar,
flags0. `portable_actor_zone.inl` already authenticates this exact schema and
stored tuple; the stasis helper uses ZoneNumber, not Zone actor identity.

Native XLevel/Model/time must come from the actual current loader-owned
ULevel/Model binding and map-generation/lookup lifetime, not a script-written
XLevel overlay or a second Model chosen by name. Conversely the final
Actor.Level read is an actual named Actor object field, not permission to
replace it by native slot0. Existing `NativeLevelInfo(actor)` deliberately
uses native ULevel.Actors[0] and therefore is **not** that final provider.

The runtime currently has an optional native Actor.lastRenderTime slot set
to-10 by original startup/Spawn phases. Its generic reflected property Read
does not bridge that native slot. A future adapter must join the authentic
schema to the genuine current native value and journal any new native stores;
silently reading a default script value would disregard the initialized slot.

The original native zone table has64 fixed entries independent of serialized
NumZones. A source stored ZoneNumber outside0..63 reaches unchecked native
memory access; a portable implementation should explicitly refuse that active
case, not wrap it, choose zone0 or use current visible-zone membership instead.
Missing active timestamps, stale identities, unrepresented arithmetic and
exhausted shared work/allocation limits likewise need named refusals. Lazy
short circuits must not demand these providers when original code never
reaches them. Native unordered comparison behavior above is evidence; it is
not a claim that a future finite-input portable arithmetic profile already
implements every NaN/infinity case.

## Model zone timestamp: transient native data, not serialized zero

These independent static witnesses establish the native table layout:

- `UModel.PointRegion 103F5EFF..11` reads selected zone actor at
  `Model+104+24*z`. See the existing exact PointRegion audit.
- `FZoneProperties.operator=` export index576, thunk `103011E0`, enters
  `1031D930..43` and copies six DWORDs: native zone size24bytes.
- InStasis `1036C018..30` calculates `(ZoneNumber+11)*24`, therefore reads
  the Float at `Model+108+24*z`, four bytes after that zone actor.

The resulting relevant zone layout is actor+0, LastRenderTime Float+4,
connectivity UInt64+8 and visibility UInt64+10hex. This is a native shape,
not the serialized byte size of an object-reference record.

`UModel.Serialize` export index1500 has thunk `103026B2` to `103ACAA0`.
For package versions above61, `103ACB7F..AC` archives NumZones and passes
each `Model+104+24*z` to thunk `1030278E`. Its complete zone serializer
`103AED30..92` archives actor reference, eight bytes at zone+8, then eight
bytes at zone+10hex. It deliberately does **not** archive LastRenderTime+4.
The current `PortableModelZone` actor/connectivity/visibility structure
therefore does not contain the transient timestamp needed by stasis.

Absence from that archive does not prove initial zero. The actual UModel
default constructor thunk `1030122B` enters `1031E3E0..548`, then calls
EmptyModel(1,0) through `10303D28` at `1031E530`. The called complete body
is `103AD3E0..690`. Its64-zone initialization at `103AD60F..64E` clears
actors, initializes connectivity/visibility, but does **not** store zone+4.
PostLoad thunk `10301F0A` to `103ACE90..F2E` rebuilds node/surface links
and delegates to its base; this bounded body also supplies no timestamp
initialization. These witnesses alone do not establish initialization. The
later independent native registration/default-copy/fresh-CreateExport audit
linked above proves Float+0 for all64 entries of a newly constructed exact
Model; it does not authorize resetting an already live Model.

Do not manufacture that value from a missing package field, allocate a zeroed
array and call it original initialization, or equate it with actor render time.

## Renderer publications that supply live timing

Owned Render.dll export index75, `URender.OccludeBsp`, has thunk
`10B010F0` to `10B173D0`. Within that body:

- `10B1766D..8D` reads actual native Level.Actors[0], loads its TimeSeconds
  Float at+478, captures that Float in a local and writes it into the viewer
  zone's Model timestamp at `+108+24*z`.
- `10B1936A..B2` reaches another zone from the BSP work and writes the
  previously captured TimeSeconds Float into that selected Model zone timing.

These are native visibility-processing publications, not proof that only the
zone containing the camera counts. A future renderer integration still needs
the complete admission/visibility gates and multi-view publication contract;
a renderer's list of submitted actor meshes is not the native zone table.

Separately, `10B25150..61` reads actual native Level.Actors[0].TimeSeconds
Float and copies its bits into the Actor+34 reached through the current
sprite's Actor pointer at+94. The nearby draw-type checks follow that store.
This is a concrete actor-time publication witness, not a complete audit of
the containing draw path's entry gates or proof of every actor render route.

Crucially, both witnesses source the **LevelInfo TimeSeconds Float**, not
native Level Double time converted later, Android wall clock, HMD predicted
display time or a new renderer-local animation clock. The existing level-clock
audit explains why Float(native stored Double) is not always the same as the
original Float mirror. Time/dilation updates and renderer publication must
retain their original spill/ownership distinction.

## DistanceFromPlayer is an earlier Level.Tick pass

This is a newly identified prerequisite, not renderer-maintained distance.
Before the ordinary actor loop at `103A5698`, `ULevel.Tick` executes the
following branch at `103A55C1..5696`:

1. Read actual native Level.Actors[0].NetMode. Nonzero bypasses this distance
   pass. This caller gate, unlike InStasis's final gate, really uses slot0.
2. Follow native Level.Engine(+40), Engine.Client(+44), its Viewports data
   pointer(+2C), first viewport, then viewport.Actor(+30). A null Actor skips
   this pass. Native code assumes the earlier engine/client/viewport services;
   it does not substitute a fabricated local player or renderer camera.
3. Require actual PlayerPawn ancestry. The original comparison at
   `103A55F1` is to `Engine.APlayerPawn.PrivateStaticClass` at1058C160,
   independently authenticated by the export table. Require that Pawn's
   actual Player pointer(+5D4) to be nonnull; otherwise skip the pass.
4. Load native FirstDynamic(+110) at103A5620, then copy that
   PlayerPawn.Location(+114/+118/+11C) once into X/Y/Z locals at5626..5634.
5. In ascending order, compare the index against **live** Actors.Num on
   each iteration and read the live slot. Null skips to the next slot. No
   bDeleteMe/active/IsA cohort filter is present in this loop.
6. For each nonnull actor, load live Location deltas in Z/Y/X order in PC64.
   Compute Y-squared plus Z-squared, then add X-squared with each arithmetic
   instruction retaining native extended precision. Spill that sum to a
   binary64 argument before calling imported Core.appSqrt.
7. Core.appSqrt is not a guessed CRT call: Engine IAT10599CA4 contains
   Hint/Name RVA0029BAB4, importing `?appSqrt@@YANN@Z`. Core export index1406,
   thunk `101032BA`, enters `10123290..96`: load the binary64 argument,
   x87 FSQRT, return the extended result. Level then FSTs an otherwise-local
   Double at `103A5689`, without popping/reloading it, and FSTPs the retained
   result to DistanceFromPlayer Float at `103A5692`. The destination Actor is
   freshly reloaded from the live native Actors data/index at568C..568F; it is
   not assumed to remain the source Actor loaded before the arithmetic.

Implementation update2026-10-25: the exact native phase and actual Engine.Client
edge are now bound separately; see ORIGINAL-LEVEL-PLAYER-DISTANCE.md. This does
not complete the surrounding Level.Tick schedule or renderer publications.

Neither VR head pose, camera/view target nor a per-actor lazy current-distance
query replaces this captured-Pawn-location pass. Reordering it into actor
Tick can observe movement that happened earlier in the frame, unlike the
original common prepass. Additional pathological native index/null/identity
and active nonfinite cases require honest portable admission rather than
changing the ordinary source ordering.

## Remaining implementation boundary

At this static audit's initial checkpoint no production equivalent of the
native distance prepass or complete stasis provider was found. The subsequent
same-root InStasis query is recorded in the implementation update above;
The distance phase is subsequently implemented separately as linked above;
render publication and automatic scheduling remain missing. The genuine falling Physics2 cohort can
avoid a zone-time read through the original Physics short circuit, but this
does not establish that its flags are always eligible or that landed actors
can run indefinitely without renderer-time integration.

The next coherent production work must join genuine stasis/native tick
context, owner/parity/pending ordering, genuine main/blend continuation and
the already implemented post-animation phase within the same current-Level
root. It also needs actual Level time/pause/caller delta, live actor slots,
spawn parity/pending publication and deferred-destruction lifetime. Shared
work, write cutoffs, GC roots, rollback/checkpoints and renderer publication
must cover the new native fields, not only reflected script properties.
This document does not authorize publishing a failed startup, changing
original loot ownership, skipping unsupported callbacks or running other
maps to substitute for first-Training acceptance.

Only this new documentation file is written by this audit. No build, runtime
test, package mutation, APK or headset acceptance is represented here.
