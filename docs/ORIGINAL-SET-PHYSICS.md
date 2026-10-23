# Original Actor.SetPhysics3970

Read-only audit, 2026-10-23. Only original Engine.u/Engine.dll and current
port/pinned source were inspected. No commercial binary was loaded/executed;
our package inspector, objdump, strings and byte reads are the evidence.
This audit did not change runtime providers or execute commercial code.

## Authority

Owned root: `D:\Steam\steamapps\common\Deus Ex`.
Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.

Our script_bytecode_inspect reports original Engine.Actor.SetPhysics:

- native3970, flags0x401; native-parameter body10 logical bytes;
- newPhysics: required ByteProperty, dimension1, flags0x80,
  constrained to Engine.Actor.EPhysics;
- newFloor: optional ObjectProperty, dimension1, flags0x90,
  constrained to Engine.Actor; no result field.

Original exported execSetPhysics thunk10302EAA reaches103C8EA0..103C8F09.
It initializes the byte destination to0, evaluates required newPhysics,
initializes the floor pointer toNone, evaluates optional newFloor, finishes
the parameter stream, then calls exported setPhysics10303F71. Argument
expressions therefore finish before setPhysics samples current Physics/Base.
No simulated result/Bool is returned.

setPhysics10303F71 jumps to103C95F0..103C96E9. The following nearby bytes are
exception diagnostics/padding, not another normal branch.

## Complete normal branch/control order

1. Capture this actor; read its current Physics byte at actor+0x30.
2. Compare current byte with supplied newPhysics byte (103C9620).
   Equality returns immediately at103C96D9. Even a different newFloor is
   ignored in this branch; do not clear vectors or run support queries.
3. Store the supplied byte at actor+0x30 (103C962A).
4. Dispatch using the supplied byte:
   - newPhysics in {1,0,7,5,10}: support branch103C9656.
     Read supplied newFloor. If nonnull, compare it with the actor's live
     Base pointer (+0xC4). Equal means no callback/query. Different means
     call this.SetSupportBase(newFloor), thunk1030146F, at103C9668.
     If supplied floor isNone, unconditionally call this.FindBase(),
     thunk10301749, at103C9671. Base alreadyNone does NOT skip FindBase.
   - all other bytes, including Falling2: read live Base (+0xC4).
     If nonnull, call this.SetBase(None,true), thunk10301285, at103C964F.
     If Base isNone, no support callback/query.
5. After the selected synchronous callback/query, reread the actor's
   CURRENT Physics byte at103C9676. Never use only the requested byte here:
   support/BaseChange callbacks can reenter and replace Physics.
6. If that current byte is neither0 nor5, return. If it is0 or5, zero both
   Velocity and Acceleration with literal positive-zero raw words.
7. Return. There is no extra bDeleteMe check, forced final Base, delayed
   support event, falling simulation, old-Location write or head-pose change.

The numeric support-mode list corresponds to None0, Walking1, Rotating5,
Rolling7 and Spider10 in the pinned enum. The literal original byte tests,
not those names or an assumed contiguous range, are authority.

## Independent native field-identity proof

The six zeroed words are demonstrably Velocity and Acceleration, not guessed
from the pinned C++ layout:

- UTF16 `Velocity` is at VA1047A974 (raw file17A974).
- UTF16 `Acceleration` is at VA1047AA3C (raw file17AA3C).
- Actor optimized replication implementation (export thunk10301B9A ->
  body103618C0) compares current/default words actor+144/+148/+14C at
  103633F2..10363421; its changed branch pushes the actual `Velocity`
  name pointer at10363433, looks up that property on actual Actor UClass
  1057CDF0, and appends the property's replication index at10363462..66.
- The same implementation compares actor+150/+154/+158 at
  103632F0..1036331E; its changed branch pushes `Acceleration` at10363332
  and appends that property's replication index at10363368..6C.
- A second independent Acceleration compare/name association appears at
  10362FEF..10363067.
- Physics+30 is likewise compared and associated with actual UTF16
  `Physics` VA1047AA5C in1036329D..103632ED.
- Actor PrivateStaticClass export RVA27CDF0 is VA1057CDF0, matching the
  actual class used for those property lookups.

The exact original final scalar store order is interleaved:

| Address | Store |
| --- | --- |
| 103C96A2 | Velocity.X = raw0 |
| 103C96B4 | Velocity.Y = raw0 |
| 103C96C1 | Acceleration.X = raw0 |
| 103C96C3 | Velocity.Z = raw0 |
| 103C96D3 | Acceleration.Y = raw0 |
| 103C96D6 | Acceleration.Z = raw0 |

There are no script callbacks between those six stores. Implementing writes
must retain same-root undo/budgets and exact declared Vector storage, without
animation-clock resets or replacement leaf-name fields. Do not rely on a
caller-supplied floor being unused or silently omit support side effects.
The original zero block never reads old Velocity/Acceleration components.
Nonfinite old component bits being overwritten are not active arithmetic and
must not be rejected merely because a convenience vector reader validates
them. Same-Physics early return does not read or normalize those vectors either.
There is no original range rejection for an otherwise supplied raw byte outside
the named enum set: it follows the literal other-byte/detach branch.

## Existing real providers to reuse

- `PortableActorVmHost::SetBase(actor,nullptr,true,execution)` in
  native/portable_unreal_runtime.cpp already connects the actual original
  Base/count/callback coordinator. Original thunk10301285 ->10369C80.
- `RetailSetSupportBase(actor,floor,execution)` in
  native/portable_actor_find_base.inl corresponds to original
  thunk1030146F ->103C9390..103C940C. A nonnull floor dynamically resolves
  floor.SupportActor(actor), then uses real ProcessEvent gates on the SAME
  execution. It must not be replaced by actor.SetBase(floor,true).
- `RetailFindBase(actor,execution)` in the same inl corresponds to
  thunk10301749 ->103C9470..103C9566. It runs the actual native-level
  SingleLine query (end Location.Z-8, real radius/height extent,
  flags0x17/extra0), then selected SupportActor/SetBase logic. Keep its
  nearest-hit/native mark semantics. No extra LoadMap-only LevelInfo Base
  clearing belongs inside generic SetPhysics.

These providers are already same-root and can themselves refuse missing
actual native Level/Model/Hash/Engine identity in an unprepared fixture.
Such a refusal must roll the entire caller back; do not invent an empty
FindBase result or add a new nested transaction to get old fixtures green.
Reentrant support overrides can change Base, Physics or deletion state;
preserve their effects until a genuine shared-root error requires rollback.

## Current port / pinned-source distinction

Current native3970 in portable_unreal_runtime.cpp:3297 validates a numeric
input, accepts an optional Object floor, then only writes Physics. Its comment
explicitly follows the pinned NActor::SetPhysics_Deus TODO. It currently lacks
the equality early return, real support/detach branch, post-callback Physics
reread, and original vector resets above. Those are concrete missing original
semantics, not a request to invent physics as a setter side effect.

The pinned UActor_Phys.cpp / UActor_PhysFalling.cpp outline a broader
TickPhysics -> TickFalling -> PhysLanded path, but their SetPhysics is only an
assignment and the Deus floor adapter is TODO. Their algorithms/order are not
automatically an oracle for the owned GOTY engine (other audited native paths
already demonstrate differences). In particular, do not implement a broad
whole-world physics system simply by calling or copying those C++ routines.

The portable runtime currently has explicit clock/state/timer phases and
real native movement/support services. It has NO actor falling-phase API or
falling integration. quest_main calls only the separate demo mover-motion
publication; that is not general Actor physics. Setting Physics2 and retaining
spawned velocities cannot settle either first-Training crate.

## Recommended next bounded increment, not a completion claim

First recover/implement this complete small original3970/setPhysics contract
using the existing support providers. Exercise same-mode/different-floor,
None/new/same floor, all exact mode predicates, reentrant Physics replacement,
old+new Base/StandingCount, six exact zero words, and every shared-write/work
failure prefix. Test the actual first-Training crate GameInfo/startup root,
not forced begun/Active fixtures; leave later Possess/refusal unchanged.

Only afterwards audit/add a genuine explicit actor physics phase usable in
that same root; it is not permission to publish partial startup or claim a
world scheduler. Relevant owned exports are located but NOT yet fully audited:

- AActor.performPhysics thunk10303242 ->103C9770.
- AActor.physFalling thunk10302FEF ->103D0A50.
- AActor.processLanded thunk10301B40 ->103CEF60.

That future work must use actual swept native Move, original Landed callbacks,
support relationships and real saved Velocity/Acceleration/zone state. The
first Training pair still requires initial falling/settling, the upper crate
being supported by the lower one, and support loss after real lower-crate
destruction. Current DropThings changes supported actors to Falling2 and
native Destroy detaches Base children, but no automatic falling step consumes
that state. Do not substitute hardcoded crate positions, a floor snap or
manual Landed/SupportActor calls for the missing native phase.



## Detached implementation checkpoint

The new asset-free coordinator in quest_actor_set_physics.h passes3112 checks/
21 refusal controls in an isolated strict-g++ test. It covers every byte mode,
same/new/None floors, exact interleaved raw zero stores, callback replacement/
nested reentry, inactive nonfinite words, missing-provider propagation and
caller-owned partial work/write undo. It is now registered in the shared build,
but NOT bound to native3970 yet; runtime correction and actual-map checks remain.
