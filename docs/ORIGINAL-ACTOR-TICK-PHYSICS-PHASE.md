# Original Actor Tick: ordering before the physics phase

Owned GOTY static audit, 2026-10-24. Integration target remains only the entire
first `Maps/00_Training.dx`, specifically its genuine upper crate, dropped
Lockpick and WoodFragment cohort. This records bounded original conditions
needed for a frame-major diagnostic and eventual real world Tick. It adds no
runtime implementation, proves no completed world scheduler and is not
headset acceptance.

Update2026-10-25: the post-animation non-Pawn control flow and real committing
phase binding are now implemented and tested separately. See
[the implementation and exact expiry arithmetic](ORIGINAL-NONPAWN-TICK-TAIL.md).
The surrounding stasis/owner/animation/world scheduler is still unfinished;
the historical audit below is not a claim of a completed Actor/Level Tick.

Read alongside [native Actor order](TRAINING-WORLD-ACTOR-ORDER.md),
[performPhysics epilogue](ORIGINAL-ACTOR-PHYSICS-EPILOGUE.md),
[rotation](ORIGINAL-PHYSICS-ROTATION.md),
[timers](ACTOR-TIMER-CONTRACT.md) and
[deferred destruction](ORIGINAL-DEFERRED-ACTOR-OWNERSHIP.md).

## Evidence and exact native identities

No commercial DLL/EXE was executed or loaded. Installed objdump reads static
bytes; our existing script inspector reads owned package tables/bytecode.
Rechecked owned `D:\Steam\steamapps\common\Deus Ex` SHA256:

- `System/Engine.dll`:
  `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
- `System/Engine.u`:
  `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
- `System/DeusEx.dll`:
  `B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04`.
- `System/DeusEx.u`:
  `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480`.

The exported native `AActor.Tick(Float,ELevelTick)` is export-table index1667,
ordinal1668, thunk10303DAA, body103A1AA0 onward. The ordinary non-Pawn paths
described below return through103A1AED..103A1B04. Native `ULevel.Tick` is
thunk10301104, body103A51A0; its ordinary dynamic loop is
103A5698..103A570E and final cleanup is103A5B35..103A5BB1.

Static reads of exported vtables give:

| Actual exported native class | Vtable | Tick slot+64 | performPhysics slot+8C |
| --- | --- | --- | --- |
| AActor | 104268DC | 10303DAA | 10303242 |
| ADecoration | 104282F0 | 10303DAA | 10303242 |
| AInventory | 104286F8 | 10303DAA | 10303242 |
| AProjectile | 10428850 | 10303DAA | 10303242 |
| ADeusExDecoration | 10027744 | 1001AF50 | 1001AFAA |

This authenticates these native base implementations, not arbitrary subclasses
or a guessed complete native virtual table for every generated actor. The
bounded real cohort ancestry is authenticated below. APawn paths remain
different.

### Actual first-Training cohort native dispatch

The owned serialized Class exports have the following exact case-sensitive
paths and object flags. Export numbers below are one-based original package
references, not portable class indices. Every row has ObjClass0 and a genuine
serialized Class payload. **RF_Native0x04000000 on that export**, not inherited
ClassFlags bits or an English class name, determines a native Class boundary.

| Package.Class | Export | ObjFlags | Direct superclass |
| --- | --- | --- | --- |
| DeusEx.CrateBreakableMedGeneral | 20445 | 000F0004 | DeusEx.Containers |
| DeusEx.Containers | 116 | 000F0004 | DeusEx.DeusExDecoration |
| DeusEx.DeusExDecoration | 14 | 040F0004 | Engine.Decoration |
| Engine.Decoration | 45 | 040F0004 | Engine.Actor |
| DeusEx.Lockpick | 1082 | 000F0004 | DeusEx.SkilledTool |
| DeusEx.SkilledTool | 683 | 000F0004 | DeusEx.DeusExPickup |
| DeusEx.DeusExPickup | 17 | 000F0004 | Engine.Pickup |
| Engine.Pickup | 606 | 000F0004 | Engine.Inventory |
| Engine.Inventory | 6 | 040F0004 | Engine.Actor |
| DeusEx.WoodFragment | 255 | 000F0004 | DeusEx.DeusExFragment |
| DeusEx.DeusExFragment | 138 | 000F0004 | Engine.Fragment |
| Engine.Fragment | 316 | 000F0004 | Engine.Projectile |
| Engine.Projectile | 193 | 040F0004 | Engine.Actor |
| Engine.Actor | 2 | 040F0004 | Core.Object |

In particular, **Engine.Fragment is not a native Class**. Its nearest native
ancestor is Engine.Projectile; no native Fragment vtable override should be
invented. Engine.Fragment, DeusEx.DeusExFragment and DeusEx.WoodFragment each
have ClassFlags0x32 and logical Class script length0 in the owned payload.
Engine.Projectile has ClassFlags0x33 and logical Class script length0. This
does not mean these classes lack selected state/function bytecode: those
declarations are separate exports and remain real callbacks.

For these three exact surviving cohorts, the nearest actual native paths are:

- CrateBreakableMedGeneral: **DeusEx.DeusExDecoration**.
- Lockpick: **Engine.Inventory**, not DeusEx.DeusExPickup.
- WoodFragment: **Engine.Projectile**, not Engine.Fragment.

Their actual native Tick/performPhysics dispatch is AActor, including the
DeusExDecoration case. Static bytes at1001AF50 are
`FF 25 38 72 02 10`: jump through IAT10027238. That unbound original IAT cell
contains Hint/Name RVA0002CDBA, importing
`?Tick@AActor@@UAEHMW4ELevelTick@@@Z` from Engine.dll. Static bytes at1001AFAA
are `FF 25 94 72 02 10`: jump through IAT10027294. Its Hint/Name RVA0002D122
imports `?performPhysics@AActor@@UAEXM@Z` from Engine.dll. These are import
thunks, not native DeusExDecoration override bodies. Engine's vtable values
already match the exported AActor thunks10303DAA and10303242 for ADecoration,
AInventory and AProjectile.

A future adapter should resolve each actual actor's genuine nearest native
Class using RF_Native and authorize only these proven native paths for this
bounded cohort dispatch. A derived class can have inherited ClassFlags while
still being non-native. Do not allow an arbitrary Decoration/Inventory/Fragment
subclass merely because it passes IsA; another actual nearest native Class
could override a virtual method. None of this selects, replaces or skips the
original state/class script callbacks used inside performPhysics.

Metadata reproduction uses the existing read-only
`LoadPortablePackageTables` implementation in
`native/surreal_portable_package_tables.cpp`: select each exact table export
above, require ObjClass0, retain ObjFlags and follow ObjBase through actual
positive exports/negative imports with `GetPortableObjectPath`. The existing
table-only audit executable
`artifacts/conditional-destroy-audit-20261021/class_export_flags.exe` also
reproduces the crate/Containers/DeusExDecoration/Decoration/Actor rows and
ClassFlags, without constructing any UObject. The additional Lockpick and
WoodFragment chains were independently read from immutable package bytes in
PowerShell using the same table layout: header name/export/import count-offset
pairs at12/16,20/24,28/32; names are version68 compact-length strings plus
UInt32 flags; exports are compact ObjClass, compact ObjBase, Int32 ObjOuter,
compact ObjName, UInt32 ObjFlags, compact ObjSize and conditional compact
ObjOffset; imports are compact ClassPackage/ClassName, Int32 ObjOuter and
compact ObjName. No class-default/runtime fallback is involved. ClassFlags
for the four zero-script classes were read after UField/UStruct/UState's
actual serialized header, refusing to skip a nonzero script by its logical
length. Nonzero-script ClassFlags are not inferred by this raw-byte check.

Additional static-byte reproduction:

```powershell
& objdump -p 'D:\Steam\steamapps\common\Deus Ex\System\DeusEx.dll'
& objdump -s --start-address=0x100277A8 --stop-address=0x100277D4 'D:\Steam\steamapps\common\Deus Ex\System\DeusEx.dll'
& objdump -d --start-address=0x1001AF50 --stop-address=0x1001AF56 'D:\Steam\steamapps\common\Deus Ex\System\DeusEx.dll'
& objdump -d --start-address=0x1001AFAA --stop-address=0x1001AFB0 'D:\Steam\steamapps\common\Deus Ex\System\DeusEx.dll'
& objdump -s --start-address=0x10027238 --stop-address=0x1002723C 'D:\Steam\steamapps\common\Deus Ex\System\DeusEx.dll'
& objdump -s --start-address=0x10027294 --stop-address=0x10027298 'D:\Steam\steamapps\common\Deus Ex\System\DeusEx.dll'
```

Reproduction from the port directory:

```powershell
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A1AA0 --stop-address=0x103A1BF9 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A23CE --stop-address=0x103A278D 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A545E --stop-address=0x103A5710 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x1036BFE0 --stop-address=0x1036C06D 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103952EF --stop-address=0x10395352 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103956B9 --stop-address=0x103956F4 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'artifacts\prerequisites-20261009\build\script_bytecode_inspect.exe' 'D:\Steam\steamapps\common\Deus Ex' 'Engine.Actor.Tick' 'Engine.Actor.Expired'
```

## Native fields and reflected authority

Existing owned full Actor field-chain evidence is in ignored artifact
`artifacts/collision-hash-save-audit-20261018/engine-actor-properties.log`.
The chain plus actual Actor copy10323B20..10323D3C corroborates:

| Field | Native position | Actual declaring schema |
| --- | --- | --- |
| bDeleteMe | +28 bit0x80 | Actor Core.BoolProperty, scalar, flags0x2 |
| bTicked | +28 bit0x200 | Actor Core.BoolProperty, scalar, flags0x2002 |
| bStasis | +28 bit0x200000 | Actor Core.BoolProperty, scalar, flags0x1 |
| bForceStasis | +28 bit0x400000 | Actor Core.BoolProperty, scalar, flags0x1 |
| bIsPawn | +28 bit0x800000 | Actor Core.BoolProperty, scalar, flags0x2 |
| bTransient | +2C bit0x2 | Actor Core.BoolProperty, scalar, flags0x1 |
| Physics | +30 Byte | Actor Core.ByteProperty, scalar, flags0x23, EPhysics |
| Role | +31 Byte | Actor Core.ByteProperty, scalar, flags0x20, ENetRole |
| RemoteRole | +32 Byte | Actor Core.ByteProperty, scalar, flags0x21, ENetRole |
| LastRenderTime | +34 Float | Actor Core.FloatProperty, scalar, flags0 |
| DistanceFromPlayer | +38 Float | Actor Core.FloatProperty, scalar, flags0 |
| Owner | +3C pointer | Actor Core.ObjectProperty, scalar, flags0x22, Actor |
| TimerRate | +48 Float | Actor Core.FloatProperty, scalar, flags0 |
| TimerCounter | +4C Float | Actor Core.FloatProperty, scalar, flags0x2 |
| LifeSpan | +50 Float | Actor Core.FloatProperty, scalar, flags0x1 |

All are owned by Engine.Actor; flags/dimension/type/constraint/declaring
identity must still be authenticated at the portable adapter. These are
native field associations, not permission to use serialized UProperty Offset
as a portable memory layout. bTicked is independent of bDeleteMe.

The owned ENetRole payload, already decoded in ORIGINAL-DESTROY-ACTOR.md,
establishes0=None,1=DumbProxy,2=SimulatedProxy,3=AutonomousProxy,4=Authority.
This audit keeps **numeric ELevelTick tests** where reached; it does not infer
the native ELevelTick names from another engine's source.

## AActor.Tick entry: stasis, owner deferral, parity write

1. Call the actual `InStasis` native helper through thunk10302013 at103A1AC8.
   Its bounded body is1036BFE0..1036C06C. True bypasses all normal animation,
   state, timer, lifespan and physics work. In that true branch, if bTransient
   (+2C bit2) is set and Role>=2, invoke Level.DestroyActor virtual+94 with
   source and force0; then return1 (103A1AD1..103A1B04). False continues.
   No source bTicked assignment occurs on this stasis return.
2. Read actual Owner (+3C). If nonnull, read **Owner.bTicked**, not source's
   bTicked, and compare it to the current native Level parity (+10C)
   (103A1B05..103A1B23). If unequal, allocate an8-byte aligned8 GMem record
   `{sourceActor, oldLevelPendingHead}`, publish it to Level+104 and return0
   before marking/ticking source (103A1B25..103A1B88). This preserves owner-first
   dependency by deferred retry; it is not an unconditional recursive owner
   Tick and not a source-owned queue.
3. Otherwise set source.bTicked to the current native Level parity, preserving
   all other packed flags (103A1B8B..103A1BAD). Original masks parity to bit1
   when storing it. There is no source-already-ticked early return in this
   native method. A caller's separate queue test must not become a new callee
   gate.
4. Then derive the bIsPawn/actual Pawn context and enter animation work.
   bIsPawn alone is not an actual Pawn cast, and ordinary non-Pawn cohort actors
   do not enter Pawn-only preprocessing. AnimSequence/frame/rate/tween and the
   four blend channels are processed before normal role/state/timer/physics
   dispatch. The main animation loop starts103A1BF9; the blend loop is reached
   at103A21D0..103A23C8. Reached notifies/AnimEnd are synchronous and can mutate
   current state/rates/physics or create/destroy actors. This audit does not
   rederive every animation arithmetic spill; it does not authorize skipping
   those phases to call the physics suffix as if it were full Tick.

### InStasis is not simply a bStasis flag

The bounded helper first computes native Level.TimeSeconds Double minus
Actor.LastRenderTime Float and requires a strict result>Double5
(constant1042ADA8). Then it requires actual bStasis, and either bForceStasis or
Physics0/5 (1036BFFC..1036C016). It reads the region zone-number byte+ D0 and
an entry in native Level.Model's +98 table, requiring the corresponding last
render timing to be old enough or DistanceFromPlayer>Double1200
(1042AD98), before finally requiring Actor.Level's byte+518==0
(1036C054..1036C06C). The raw native table access is not yet an independently
typed portable Model timing service here. Do not replace it with a rendered
mesh list or claim every old invisible actor is in stasis. Exceptional active
arithmetic/index/lifetime cases need explicit refusal rather than guessed
success. More detailed renderer-time/zone-table integration remains required.

## Exact non-Pawn callback and physics suffix

After animations,103A23CE reads RemoteRole first and Role second. The following
is the ordinary **non-Pawn** path, not the special simulated Pawn/player code:

### RemoteRole!=3 and Role>=2

At103A2643 read the native ELevelTick input. Literal1 returns1 immediately,
after animations/parity but before Tick, ProcessState, Timer, LifeSpan and
physics. Other values follow:

1. Explicit live `IsProbing(FName0x150/336)` test
   (103A2653..103A2669). This is probe36, Tick. If enabled, resolve selected Tick
   and invoke real ProcessEvent with a copied Float DeltaTime packet
   (103A266B..103A268D). Probe false skips only this event, not subsequent work.
2. Call actual source ProcessState virtual+14 with original whole deltaSeconds
   (103A2690..103A2698). It is not contingent on Tick being enabled and must
   preserve selected/running state, latent work and callbacks in the same root.
3. Execute the original timer block103A269B..103A2709, including counter
   reduction/one-shot rate clear before the selected Timer callback. See the
   separate exact timer contract; never synthesize catch-up Timer callbacks.
4. Read actual LifeSpan at103A270B. Exact Float0 and unordered NaN skip expiry
   work (the branch tests C3). Otherwise subtract whole deltaSeconds in PC64
   and FST Float at103A2721 without popping the extended result; compare that
   still-live PC64 result, NOT its rounded stored Float, against
   Double0.0001 (1042AC40). A reached finite result<=0.0001 selects actual
   Expired and calls real ProcessEvent with no parameters, then invokes
   Level.DestroyActor(source,force0), then returns1. There is no new test of
   LifeSpan after Expired to cancel that destroy. Full callback/exceptional
   arithmetic behavior belongs to the real journaled suffix, not a silent
   local actor disappearance.
5. Only when not returned by expiry, read **fresh** Physics Byte at103A275C.
   Zero skips performPhysics completely, including its rotation/PendingTouch
   epilogue. If nonzero, compare **fresh** Role to literal3 at103A2763. Role3
   skips it; every other nonzero mode calls selected native performPhysics
   virtual+8C with whole deltaSeconds (103A2768..103A276D).

Thus a prior Tick/state/timer callback can change Physics/Role before this
dispatch. Do not choose the mode/eligibility once at frame start, or treat
every zero-Physics actor as still requiring per-frame performPhysics rotation.
Conversely, after a nonzero-mode performPhysics has begun, its own unconditional
rotation/PendingTouch tail remains mandatory even if its movement callback
changes mode to0 or deletes source.

### Other confirmed non-Pawn role paths

- If RemoteRole==3, Role must equal4 to enter the special branch. It invokes
  ProcessState and the timer block (103A249F..103A251C), then joins the tail
  at103A278D, bypassing the normal LifeSpan and performPhysics suffix. Other
  Role values in this RemoteRole branch skip to that tail directly. This audit
  does not authorize mapping that branch to the normal standalone suffix.
- If RemoteRole!=3 and Role<2, skip the normal Tick/state/timer/lifespan block.
  At103A2775, only Physics==2 calls performPhysics (103A277B..103A2783), without
  the normal literal1 tick-type early return. No other Physics mode runs here.

For a non-Pawn, the remaining network/player tail eventually returns1 through
103A1AED. This audit does not implement the distinct Pawn/player/network work.

The exported FName addresses are independently verified from the export table:
Tick105836B8, Timer10583718, Expired105836B0 and AnimEnd105836E0. Owned base
Engine.Actor.Tick is native0/flags0x800/logicalBytes2 with one required scalar
FloatProperty DeltaTime, flags0x80; Engine.Actor.Expired is native0/flags0x800,
logicalBytes2 with no parameters/result. Empty base scripts do not permit a
selected state/class override to be skipped. Use real FindFunctionChecked and
ProcessEvent semantics, with fresh original probe/begun/deletion gates.

## Level dynamic order: live slots, then deferred batches

Native LoadMap partition/FirstDynamic ownership is recorded separately. On the
normal ticking branch, ULevel.Tick sets native +108 in-tick to1 at103A5271.
Whole-level time dilation/clamping/network/paused-mode work precedes the actor
loop; the literal delta passed to actors is not assumed to be real VR frame
elapsed time without that caller contract.

The normal actor loop103A5698..103A56CB:

1. Load FirstDynamic (+110) into the ascending slot index, set a local signed
   sum of Actor.Tick returns to0, and clear native pending head+104.
2. Before **each** iteration, compare current index against the **live**
   Actors.Num (+30); then read Actors.Data (+2C) and that live slot. None skips
   the call. There is no frozen array snapshot or initial fixed end bound.
3. A nonnull actor gets actual virtual Tick+64(deltaSeconds,ELevelTick). Add
   its integer return to the pass sum; increment index and repeat. The loop
   itself has no bTicked, bDeleteMe or class-name filter.

Native slot removal can make a future slot null; native Spawn can append actors
that the later live loop reaches in this very frame. Sorting the cohort by
object path or a detached renderer inventory is not this ordering. A bounded
explicit diagnostic can deliberately observe a selected native-order subsequence
but must identify that narrower scope honestly, not call it complete Level.Tick.

After the ascending loop,103A56CD..103A570E consumes the shared pending chain:

1. Capture current Level+104 head. Stop this phase if head is None **or if the
   preceding pass's sum of Tick return values is0**.
2. Clear actual Level+104, reset the new pass sum to0 and walk the captured
   `{Actor*,next*}` chain in stored order. This is naturally LIFO for records
   pushed at the head; it is not sorted or deduplicated.
3. Per record, read its actor's live bTicked and compare to current Level parity.
   Equal skips Tick. Unequal calls actual actor Tick+64 and accumulates return.
   Advance using this record's next pointer; callbacks may enqueue a new batch
   at the now-independent live Level head.
4. When the captured batch ends, repeat from the new live head with the new
   return sum. Owner dependency cycles/all-deferred passes can therefore stop
   without a successful Tick; do not invent recursive forced owner execution or
   drain the list until empty regardless of progress.

The shared pending list is **not just newborns**: AActor.Tick's owner-defer
path uses it too. Its native GMem lifetime and current-level ownership must be
represented or explicitly refused before exposing an automatic scheduler.

### Newborn parity and same-frame reachability

At103952FD..1039531C SpawnActor reads Level parity and writes the new actor's
bTicked to its logical **opposite**, preserving other flags. That allows a
genuine new actor to be eligible during the current frame; it does not inherit
the currently ticked parity from its owner/class template.

At103956B9..103956F3, when and only when native Level+108 in-tick is nonzero,
SpawnActor allocates an8-byte aligned8 GMem `{birth, oldPendingHead}` record
and publishes it as head+104. The source birth also has its genuine appended
live world slot. If the ascending loop already ticks that appended birth,
later pending processing sees matching bTicked and skips it. If it was deferred
because its owner had not ticked, a later successful batch can retry it.

A portable isolated startup/root diagnostic that is explicitly **not inside
Level.Tick** cannot pretend this queue ran; its inactive +108 branch must be
honestly scoped. A future active-tick birth must not silently omit the queue,
flatten the order into a snapshot or reset newborn parity to mark it complete.

## Lifetime, pause scope and final boundary

The normal dynamic loop does not prefilter bDeleteMe, and the audited ordinary
AActor.Tick suffix contains no new bDeleteMe check between selected Tick,
ProcessState, Timer and performPhysics. Individual ProcessEvent and physics
methods have their own actual gates. The original DestroyActor removes a world
slot and retains deferred UObject identity; a deleted actor retained by a local
or pending record is not automatically a retired pointer. Do not turn renderer
`active=false`, world removal or bDeleteMe into a new native callee early exit.

This document does **not** recover every level pause/network/player branch.
The ordinary full loop is reached through the native ELevelTick/pause/network
conditions around103A54FF..103A55CE. A separate path103A578C..103A58CF scans
dynamic slots and conditionally calls Tick for Actor+1A0 bit0x20, associated by
the surrounding native code with a separate eligibility test; this audit does
not establish its complete declaring-field/paused-mode contract. Its surrounding
player/input work is not a substitute normal scene simulation. Do not broaden
this normal-loop audit into a claim about pause or players-only scheduling.

After level work,103A5B35..103A5B50 toggles native Level parity using logical
not and clears native +108 in-tick. GMem/global temporary state is restored,
then real Level.CleanupDestroyed(force0) is called at103A5B91..103A5B99. That
cleanup has its own established threshold/native-service contract and is not
an immediate scalar destructor to add inside every crate destruction.

## Consequences for the next first-Training diagnostic

The next evidence should advance the real upper crate, Lockpick and six genuine
WoodFragments **frame-major**, preserving actual native actor order, actual
physics/role eligibility and synchronous movement/support/event effects.
It must not erase debris, freeze loot while advancing only the upper crate,
or replace inventory by a fabricated grant. If the diagnostic intentionally
calls only the per-actor physics phase, it must state that it does not execute
the complete preceding animation/Tick/state/timer/lifespan/owner scheduler.

Acceptance of a complete world Tick additionally needs the native parity,
deferred batch and active-spawn queue ownership/journal; real entry stasis;
all reached earlier callbacks and late destruction; live slots/births/removals;
actual native method selection; and real level time/pause services. Work,
write-cutoff, reentry and whole-root rollback controls must include those
fields/queues/clocks/identities rather than treating a successful detached
cohort as a committed begun world. Original source bytes remain immutable.

Only this new documentation file was written by this audit. No compiler input,
build, runtime test binary, commercial executable or commercial DLL was changed
or executed.
