# Original Actor blocking prerequisites

Static read-only audit, 2026-10-19, for the first `00_Training` movement path.
No commercial DLL or EXE was loaded or executed. Owned `Engine.dll` SHA256,
checked before and after inspection:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
This establishes the three helpers below, not complete Level encroachment,
Move266/SetRotation299, world startup, or playable crate physics.

## Exact identities and reproducible evidence

The complete normal bodies, including all returns, were read from static PE32
disassembly. Addresses are preferred virtual addresses, not portable offsets.

| Export ordinal and identity | Thunk | Complete normal body |
| --- | --- | --- |
| 1244, `AActor::IsBlockedBy(AActor const*) const` | `10301785` | `1034C5E0..1034C82A` |
| 1243, `AActor::IsBasedOn(AActor const*) const` | `103020DB` | `1034C8D0..1034C8F2` |
| 1105, `AActor::GetPlayerPawn() const` | `10302E4B` | `1034C540..1034C577` |

Each thunk is a five-byte relative JMP to the stated body. The existing
`artifacts/world-native-startup-20261011/engine-exports-imports.log` pins the
export ordinals, thunk RVAs and native class symbols. The native class objects
used by these bodies are `ABrush::PrivateStaticClass` at `10582A28` (ordinal1353),
`APlayerPawn::PrivateStaticClass` at `1058C160` (ordinal1379), and
`AProjectile::PrivateStaticClass` at `10578850` (ordinal1382). GNU objdump's
bracketed export-name index is ordinal minus the PE export base1, not the ordinal.

For example, bounded static reproduction is:

```
objdump -d -M intel --start-address=0x1034c5e0 --stop-address=0x1034c82b Engine.dll
objdump -d -M intel --start-address=0x1034c8d0 --stop-address=0x1034c8f3 Engine.dll
objdump -d -M intel --start-address=0x1034c540 --stop-address=0x1034c578 Engine.dll
```

`Engine.dll` above means the owned installation's file, not a code module to load.
No pinned replacement-engine implementation was used as the behavioral authority.

## Receiver-local native GetPlayerPawn

`1034C541..565` follows the receiver's actual UClass/Super chain, beginning at
native Actor `+24`, until matching the exact PlayerPawn class object or None.
On no match it returns None without reading the Player field. On match,
`1034C56A..577` reads the receiver's native `Player` pointer at `+5D4`.
Any nonzero Player pointer returns the receiver itself; zero returns None.
The `NEG; SBB; AND ECX` sequence is a nonzero-pointer mask, not a signed test.
There is no player-object dereference or Player ownership/liveness check here.

This helper is **not script native720's current-Level fallback**. The latter's
existing portable implementation scans Level actors. Using that scan in the
blocking matrix would classify a crate/NPC as a player when some unrelated
local PlayerPawn exists. The matrix inlines this receiver-local helper; it
does not dispatch an event, a virtual override, or native720.

## IsBlockedBy: complete ordered matrix

Let `S` be this/source (`ECX`) and `C` the argument/candidate (`ESI`). The order
below is observable provider laziness, not an unordered symmetric truth table.
`IsBrush(A)` here means nonnull raw `A.Brush` at `+178`, then actual Brush
ancestry. It does **not** test bStatic, bMovable, or the Brush payload/type.

1. `1034C5E0..5FB`: read `S.Level` at `+A8`. If pointer-identical to C,
   return S.bCollideWorld immediately. No candidate fields/class are read.
2. `1034C5FE..698`: test C.IsBrush, reading Brush before class ancestry. If
   true, first require S.bCollideWorld. If false, return false without a player
   test or candidate block read. Otherwise choose C.bBlockPlayers when the
   **source's receiver-local** GetPlayerPawn is nonnull, else C.bBlockActors.
   Return that bit. Projectile ancestry is never inspected in this branch.
3. `1034C699..733`: only when C was not a brush, test S.IsBrush. If true,
   first require C.bCollideWorld, then choose S.bBlockPlayers when C's local
   GetPlayerPawn is nonnull, else S.bBlockActors. Return the bit. Again no
   Projectile check. When both are brushes, step2 has precedence.
4. `1034C734..7A6`: ordinary pair. Determine whether S has receiver-local
   GetPlayerPawn, else whether S has Projectile ancestry. The Projectile
   traversal is short-circuited by a nonnull local player. Read C.bBlockPlayers
   for that player/projectile case, else C.bBlockActors. If the selected bit
   is false, return false immediately: no C player/Projectile classification
   and no source block bit are read.
5. `1034C7A8..82A`: only after the first selected bit was true, determine C's
   local player or Projectile status in the same order. Return S.bBlockPlayers
   for that case, else S.bBlockActors.

The native `+1DC` bitfield is read with SHR/AND or TEST: masks2/4/8 are
bCollideWorld/bBlockActors/bBlockPlayers. All normal returns are integer0 or1.
No branch treats a whole signed flag word as `>0`; unrelated sign/high bits do
not affect the selected Bool. No bCollideActors, Owner/Base, self, deletion,
hidden, pending-kill, same-Level, or source-family exclusion exists here.
Such query/movement filters belong to their separate callers.

Null S is not admitted by the original body: its Level is dereferenced first.
Null C is safe only if equal to the raw S.Level in step1; otherwise C.Brush is
dereferenced. A portable actual-Level service may conservatively reject null
actors instead of pretending a failed native dereference returned false.

## IsBasedOn: direction, None and cycles

`1034C8D0..8F2` checks this/source for None first. A null receiver returns false,
even if the argument is also None. For a nonnull receiver it compares **self
identity before following Base** at `+C4`, then repeats while the current node
is nonnull. Thus `A.IsBasedOn(A)` is true without any Base read; the predicate
asks whether the argument occurs in the receiver's self-plus-Base chain.
A None argument never matches the terminating None and returns false for an
acyclic chain. No class, flags, Level, or target-object fields are inspected.

These bodies and their inlined class traversals have no visited set or work
limit. A cycle not containing the requested identity can loop forever. An
early class/actor match returns immediately without validating later links.
A portable shared work/depth/cycle refusal is a deliberate safety admission
policy, not retail false or proof the original inspected the whole chain.
Do not accidentally reverse `source.IsBasedOn(candidate)` and
`candidate.IsBasedOn(source)` when applying caller-specific filters.

## Minimal reusable provider seam

An implementation can preserve all lazy reads with three typed providers:

```
pointer(actor, Level | Brush | Player | Base) -> opaque pointer/None
isA(actor, Brush | PlayerPawn | Projectile) -> actual ancestry membership
flag(actor, CollideWorld | BlockActors | BlockPlayers) -> selected Bool
work() -> charge the enclosing root, optionally provided by the caller
```

The kernel itself composes local GetPlayerPawn as
`isA(PlayerPawn) && pointer(Player)!=None`, and brush as
`pointer(Brush)!=None && isA(Brush)`. Alternatively these two composites may
be providers if their internal read order is independently authenticated and
tested; neither may call script native720. Base traversal must retain its
query direction and join the same work budget. Runtime providers must read
the genuine declared fields/native Player authority and actual class identity,
not renderer state, bIsPlayer, class leaf names, primitive selection or copied
aggregate collision snapshots that eagerly read inactive fields.

Independent tests should record exact provider call order for every early
return, both-brush precedence, player-vs-projectile short circuits, ordinary
first-bit rejection, signed/unrelated bitfield values, self/None/base-chain
directions, and cycles/failed providers poisoning the same enclosing root.

## Portable implementation and admission boundary (2026-10-19)

`native/quest_actor_blocking.h` implements the complete ordered IsBlockedBy,
receiver-local GetPlayerPawn and iterative IsBasedOn helpers. The separate
`native/portable_retail_actor_blocking.inl` supplies actual current-generation
UObject identities, exact declaring fields and genuine UClass ancestry to the
same VM host. Reads stay lazy and use live values, not collision snapshots,
renderer identities or a second actor registry. Brush/Player are presence-only
providers: no primitive selection or referenced asset/player payload read.
The native-bound Player branch tests raw `nativePlayer != nullptr` without
using Read's potentially masked public view. An unbound field follows its
ordinary exact declared Object read. This does not establish possession.

Exact object declarations additionally checked from the original Engine.u are:
PlayerPawn.Player export69, flags2, dimension1, base0, ref Engine.Player;
Actor.Level export4, flags2/ref Engine.LevelInfo; Actor.Base export60,
flags22/ref Engine.Actor; Actor.Brush export1380, flags2A/ref Engine.Model.
The three selected collision Bools have flags21, dimension1 and base0. Raw
Object fields and Bool flags must have their genuine Core property types.
The class provider returns at the first actual target-class identity before
following inactive Super links. Class and Base traversal use the same native
host lifecycle-work allowance; no predicate resets it. Provider failures and
portable cycle/work limits throw, never produce a fabricated false.

Current-generation/lookup/actor ownership, authentic metadata and bounded work
are deliberate portable admission requirements. They are not new retail
blocking bits or exclusions. IsBasedOn's target is only compared; its fields
are not sampled. The kernel allows a null Base-chain source and target exactly
as audited, but conservatively refuses null IsBlockedBy/local-player receivers.
Ordinary script permissions, active-hash mutation guards and class metadata are
unchanged. Public rollback-only commands exercise these same providers without
requiring a hash or starting the world; they do not expose Move or higher Level
encroachment as implemented natives.

Standalone tests currently pass1900811 checks/127 explicit refusals:1179648
factorized class/pointer/bitfield/Level cases,655360 full-byte pairs and64000
high-bit pairs, with48 local-player controls,32 complete lazy transcripts,
113 active-provider failure prefixes,14 Base controls and four live mutations.
The independent class/bitmask oracle is not the production helper. These are
predicate tests, not evidence of playable crate physics.

The actual exact38-source first-Training integration passes883599 checks/55
explicit refusals. New coverage:1308 local-player census controls; nine real
crate/mover/NPC/Camera/LevelInfo representatives with81 directional pairs,
66 blocked/15 clear; matrix branch counts9/18/12/42;90 Base/self/None controls;
93 uninitialized-hash rollback-only batches. No forced possession or authored
Base edge is invented. Actual PlayerPawn ancestry count is four with zero
nonnull Player; all authored Base links are None. Consequently this map does
NOT exercise the nativePlayerBinding=true positive branch or a nontrivial real
Base chain. The standalone provider tests cover those semantics synthetically.
All actual pose/mirror/save/revision/lookup/client/GC/RNG state and original
source files remain unchanged. The strict original startup boundary is still
DeusExMover16 BeginPlay PC166, unsupported native266. Strict ARM64 runtime/test
syntax, Android native link and coherent desktop CTest pass:139 executed/163
registered,24 explicit owned-data skips, zero failures in52.28s. The separate
Engine42647/31 and isolated crate347/two regressions pass; other skipped owned
cases are not claimed as executed. No APK or on-headset physics claim.
Evidence: `artifacts/training-blocking-*20261019.log`.
