# Original ScriptedPawn Tick: a genuine native override

Read-only prerequisite audit, 2026-10-26. The immediate target remains the
entire first `Maps/00_Training.dx`, not other-map integration. The current
unfiltered Training diagnostic reaches the exact native-class boundary
`DeusEx.ScriptedPawn` after the separate scalar Float implementation passes
`TriggerLight.Tick`. That observation is not proof that NPC Tick, AI or Pawn
physics works.

This audit does **not** authorize adding ScriptedPawn to the exact-AActor
native-virtual allowlist. Its original Tick and physics slots are different.
No compiler input is changed by this document.

## Evidence and scope

No commercial DLL or EXE was loaded or executed. External objdump reads owned
PE32 bytes; an independent read-only package-table parser reads the owned
`.u` files. Addresses are preferred-image VAs, not portable layouts or live
process pointers. Owned inputs are the same GOTY installation as the preceding
Actor/Level audits:

| File | SHA256 |
| --- | --- |
| `System/DeusEx.dll` | `B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04` |
| `System/DeusEx.u` | `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480` |
| `System/Engine.dll` | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| `System/Engine.u` | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Read alongside `ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md`,
`ORIGINAL-LEVEL-DYNAMIC-TICK-PHASE.md`, `ORIGINAL-NONPAWN-TICK-TAIL.md`,
`ORIGINAL-TOWORLD-VTABLES.md` and `AI-NATIVE-AUDIT.md`. In particular, the older
ToWorld slot census does not prove Tick-slot equivalence.

## Actual Class ancestry, constructor and vtable identity

The original package export records independently establish the native
boundary, not a class-name guess:

| Owned package | Object ref | Class export | Super ref/path | UObject flags | Serial offset/size |
| --- | --- | --- | --- | --- | --- |
| `DeusEx.u` | 5 | `ScriptedPawn` | -8 / `Engine.Pawn` | `040F0004` | 269857 / 6966 |
| `Engine.u` | 1 | `Pawn` | 2 / `Engine.Actor` | `040F0004` | 58581 / 2644 |
| `Engine.u` | 2 | `Actor` | -3 / `Core.Object` | `040F0004` | 61225 / 1236 |

Each selected export has Class reference zero and the native UObject flag
`04000000`. The first five raw compact links in those Class payloads are,
respectively, `-8,0,18725,18958,209`, `2,0,5397,3573,12`, and
`-3,0,5536,4084,456`. Class payloads start with the UField links; they must not
be parsed as if they started with a property-stream `None` terminator.

DeusEx.dll export-name index10 identifies the default AScriptedPawn
constructor, RVA44D0. Its body is `100044D0..100045B0`, and the literal store
at `1000459B` installs vptr `10027624`. Export-name index176 identifies
InternalConstructor at `100044C0`; its non-null path jumps to that same default
constructor at `100044C8`. The exported vtable itself is export-name index101,
RVA27624. The corresponding exported PrivateStaticClass is index197,
RVA3C020. These are zero-based export-name indexes, not ordinals.

## The four requested native slots

| Service / offset | Actual ScriptedPawn vtable cell | Stored target and authenticated destination |
| --- | --- | --- |
| ProcessEvent +10 | `10027634` | `1001AF98` -> IAT `10027268`, imported `AActor::ProcessEvent` |
| ProcessState +14 | `10027638` | `1001AF92` -> IAT `10027264`, imported `AActor::ProcessState` |
| Tick +64 | `10027688` | **`100195A0`, local `AScriptedPawn::Tick`** |
| performPhysics +8C | `100276B0` | **`1001AF14` -> IAT `10027210`, imported `APawn::performPhysics`** |

The actual import descriptors authenticate the destinations rather than relying
on disassembler nearest-export names:

| IAT VA | Hint/name RVA | Engine import hint | Decorated symbol |
| --- | --- | --- | --- |
| `10027268` | 2CFC6 | 1441 | `?ProcessEvent@AActor@@UAEXPAVUFunction@@PAX1@Z` |
| `10027264` | 2CFA6 | 1443 | `?ProcessState@AActor@@UAEXM@Z` |
| `10027210` | 2CC6A | 2057 | `?performPhysics@APawn@@UAEXM@Z` |
| `10027238` | 2CDBA | 1667 | `?Tick@AActor@@UAEHMW4ELevelTick@@@Z` |
| `100273E4` | 2DD36 | 1131 | `?InStasis@AActor@@QAEHXZ` |
| `100273E8` | 2DD52 | 1378 | `?PrivateStaticClass@APlayerPawn@@0VUClass@@A` |

Engine.dll's actual APawn vtable is `10426994`, exported name662. Its Tick
slot +64 is `10303DAA`, the ordinary AActor.Tick thunk; its physics slot +8C
is `10302860`, exported `APawn::performPhysics`, jumping to body `103C9A70`.
That is **not** the AActor physics thunk/body `10303242 -> 103C9770`.

For comparison, the authenticated AActor event/state bodies are
`10302D74 -> 10369100` and `10301F00 -> 103E5B60`, and Actor Tick is
`10303DAA -> 103A1AA0`. Therefore event/state aliases do not prove either
ScriptedPawn Tick or Pawn physics equivalence.

## Bounded native Tick control-flow findings

Export-name index235 gives `?Tick@AScriptedPawn@@UAEHMW4ELevelTick@@@Z`,
RVA195A0. Its ordinary body is `100195A0..10019DF5`, not an empty forwarding
thunk. The following observed instructions are sufficient to reject an Actor
substitution; this is **not** a full portable implementation contract for every
branch of that body.

- The prefix reads original RemoteRole at +32, Role at +31 and the caller's
  tick type. If RemoteRole equals3, Role is below2, or tick type equals1, it
  tests bit40 of byte +30C. With that bit clear it jumps to the base-call tail
  at `10019DD5`; other paths enter its custom work at `100195E8`. No reflected
  field name is assigned here to the unverified +30C bit.
- Custom work loads native XLevel +AC, uses its Double TimeSeconds +DC and
  Float LastRenderTime +34, and stores/clamps a local elapsed-since-render
  Float. It also performs actual Class/Super traversal, including the imported
  APlayerPawn Class identity. A type check here is not permission to replace
  the original class relation with a display-name predicate.
- A branch testing actor byte +788 bit1 calls original `AActor::InStasis`
  through IAT `100273E4` at `10019659`. Depending on that result or a native
  elapsed-time comparison, it invokes XLevel's DestroyActor virtual +94 with
  force argument0 and returns1 directly (`10019683..10019698`). This work
  occurs before the eventual base Actor Tick; returning1 is not proof that
  Actor's bTicked marker was assigned.
- Other branches update native Floats, including a timed interpolation from
  target +69C/+6A0/+6A4 into +180/+184/+188 under remaining time +6A8. The
  body invokes local routines `10019E30` and `10019F60` with the original delta
  at `100197C3` and `100197CB`, and later uses virtual ProcessEvent +10
  (one confirmed call at `10019D91`). These are observable writes/callbacks,
  not an ignorable preamble.
- The common tail at `10019DD5` pushes the original tick type and original
  Float delta, then calls genuine `AActor::Tick` through IAT `10027238` at
  `10019DDF`. It returns that call's EAX unchanged through `10019DF5`; this
  preserves Actor's deferred-owner return0 instead of forcing success1.

Thus neither a local stasis assumption nor an idle-pose observation permits
skipping the override. Its native work can precede Actor's own stasis test and
owner-defer logic. Capturing selected authored fields from a real Training NPC
will be necessary to determine the actual first-frame branch; this audit does
not claim that any particular NPC takes the custom or direct-base branch.

## Separate Pawn preprocessing and suffix prerequisites

Even implementing the ScriptedPawn wrapper does not by itself make the base
Actor path complete for these instances. The preceding Actor audit already
records bIsPawn at actor +28 bit800000. Static bytes
`103A1BA8..103A1BF0` independently show that the original base routine tests
that flag, traverses actual Class/Super ancestry against native APawn identity
`1058C660`, retains the actual Pawn pointer in a local and derives role-related
context before entering the shared animation body at `103A1BF0`.

The current portable driver deliberately throws
`Original Actor Tick Pawn preprocessing is not implemented` after the marker
when bIsPawn or actual Engine.Pawn ancestry is present. The separately
implemented non-Pawn tail also explicitly refuses Pawn substitution. Those
guards expose missing work; removing them is not an implementation of the
original Pawn/player/simulated-proxy paths documented as separate in the Actor
Tick audit. Reaching a PhysicsNone value in one snapshot would likewise not
prove that later callbacks cannot select Pawn physics.

An eventual implementation needs, in original order and within the same caller
transaction, the actual selected ScriptedPawn Tick wrapper, its own native
fields/AI/callback/lifetime boundaries, the real Pawn context and role paths in
AActor.Tick, and APawn.performPhysics when selected. Genuine failure must undo
the entire frame root; native callbacks must not be replaced with success/no-op
results merely to advance the map scan. Unknown nearer native subclasses
remain unsupported until independently authenticated.

## Deliberate limits

No native admission, NPC skip, fabricated AI result, generic Pawn allowance or
compiler-source change is made. This document authenticates the exact next
boundary and several required orders; it does not complete the full
ScriptedPawn AI Tick body, APawn physics, world perception/navigation,
state-machine callbacks, a working NPC simulation, or headset acceptance.
