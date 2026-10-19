# Original ULevel.CheckEncroachment

Static source audit, 2026-10-19, limited to prerequisites for the first
`00_Training` movement/startup path. No commercial DLL/EXE was loaded or
executed. This is a contract audit, not a claim that Move, gravity, crates,
touch relationships or playable Training are complete.

Owned source hashes:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

## Identity, complete body and evidence

Export ordinal830 is
`ULevel::CheckEncroachment(AActor*,FVector,FRotator,int)`. Thunk `10302536`
jumps to `1039A350`. All normal branches were read through the false return
at `1039A90D`; the true return is `1039A7E4`. Both are `ret 20h`. The separate
catch fragments occupy `1039A910..1039A93F`. Addresses below are preferred
PE32 virtual addresses, not portable object offsets.

The old `artifacts/world-native-startup-20261011/`
`original-checkencroachment-early-body.asm` ends at `1039A4FA` and is NOT the
whole contract. Newly generated, ignored static evidence is in
`artifacts/training-level-encroachment-20261019/`:

- `level-check-encroachment-body.asm`: entire normal body and catch tail.
- `thunk-levelcheck.asm`, `engine-export-import-identities.log`: export,
  event-name globals, imported functions and native class identities.
- `actor-relations.asm`: complete local GetPlayerPawn and IsBlockedBy bodies.
- `touching-body.asm`: entire IsOverlapping, BeginTouch, its one-sided helper,
  and EndTouch bodies including all normal returns.
- `thunk-begintouch.asm`, `thunk-endtouch.asm`, `thunk-overlap.asm`,
  `thunk-getplayer.asm`, `thunk-movesmooth.asm`: exact exported jumps.
- `movesmooth-body.asm`: complete moveSmooth body, including its return;
  captured to identify the required dependency, not an independent numeric
  audit of sliding/TwoWallAdjust.
- `original-callback-schemas.log`: four original Engine.u event declarations,
  read by the existing portable package inspector. That executable reads
  Core/Engine package bytes only; it does not execute their bytecode or load
  a map or commercial library.
- `exception-handler.asm`, `exception-function-info.asm`: exact function EH
  stub and metadata. The superseded `actor-relations-tail.asm` starts inside
  an instruction and is not an authority; use complete `actor-relations.asm`.

Reproduce the main body with NDK27 LLVM18:

```
llvm-objdump --disassemble --x86-asm-syntax=intel --no-show-raw-insn \
  --start-address=0x1039a350 --stop-address=0x1039a940 Engine.dll
```

## Arguments and early admission

This/ECX is ULevel. Stack `+08` is source S, `+0C..14` proposed Location P,
`+18..20` proposed Rotation R, and `+24` the integer touch-notification flag T.
S must be nonnull (assert at `1039A375..397`). T is tested for zero/nonzero,
not as a mask; it does not suppress encroachment checks or blocking events.

At `1039A39A..3D9`, enter the main work if **any** of S.bCollideActors,
S.bBlockActors or S.bBlockPlayers is set (native `+1DC`, mask `0D`). Otherwise
enter only if S is an original moving brush: nonnull Brush, actual Brush
ancestry, and !bStatic. All other cases immediately return false. Thus
bCollideWorld alone is not an admission bit; static brushes are not globally
excluded when one of mask0D's bits is set. This is the same narrow dormant
GameInfo false path already pinned by `WORLD-NATIVE-STARTUP.md`.

Once admitted, `1039A3DF..3F9` captures a GMem mark: imported GMem at IAT
`10599BD8`, its current Top word and current chunk word `+0C`. If the actual
ULevel.Hash at `+F8` is nonnull, call its vtable `+1C` ActorEncroachmentCheck
with `(GMem,S,P,R,0)` at `1039A42F`. Otherwise the original hit list is None.
The literal0 is the hash operation's unused final DWORD, NOT T.

The lower operation is exactly `ORIGINAL-ACTOR-ENCROACHMENT-CHECK.md`: source
pose is temporary for bounds and source PointCheck, tags are shared, and the
result is the original tail-ordered linked hit list. The higher function does
not leave S at P/R while processing the list. Do not wrap this whole higher
operation in the proposed-pose scope.

## First pass: blockers, pushing, recheck, veto

Traverse the ORIGINAL hit chain in order at `1039A43A..7B0`, following its Next
after processing each record. Neither a push requery nor later callbacks
replace that original list. For each C = hit.Actor:

1. Skip pointer-identical S (`1039A448`). Assert this ULevel's Actors[0] is
   nonnull and actual LevelInfo, then skip that exact Actors[0] identity
   (`1039A451..4C0`). It is not a renderer/census fallback.
2. Evaluate the source-local **live** IsBlockedBy(C) matrix, inlined at
   `1039A4C6..4DF` and `1039A5DF..6E9`. Nonblockers go to the next hit with no
   EncroachingOn or push. The complete lazy matrix is independently pinned in
   `ORIGINAL-ACTOR-BLOCKING-PREDICATES.md` and matches the exported helper.
3. A blocking C goes directly to the veto event unless S is currently a moving
   brush and C is NOT currently a moving brush (`1039A4E5..534`). These tests
   inspect raw Brush pointer/class/bStatic only, not GetPrimitive or its asset
   payload. No class-leaf-name, bMovable, Owner/Base or deletion shortcut exists.
4. For that movable-brush push case, compute each delta component as
   `Float(P.component - S.Location.component)` from the **current live** source
   Location. The code evaluates Z,Y,X and stores X,Y,Z Float words
   (`1039A53A..571`). Call `C.moveSmooth(delta)` at `1039A577`; its return is
   ignored. Rotation delta is not used to push the candidate.
5. Re-read actual Level.Hash, call the SAME hash source query `(GMem,S,P,R,0)`
   at `1039A5BD`, and scan the new list for the current original hit's C
   (`1039A5C0..5DD`). Ignore all other requery candidates. If C is absent,
   continue the original list: no reverse push and no EncroachingOn for C.
6. If C remains, compute Float `-delta` using the exact Float -1 constant at
   `104298F0`. Save the current source Location's raw three words, temporarily
   replace only Location with P, call `C.moveSmooth(-delta)` at `1039A75D`, then
   restore those raw words (`1039A6F6..770`). Rotation is not swapped here.
   Again the result is ignored. This is a second collision-aware movement
   attempt, NOT restoring C's old coordinates or an exact physics undo.
7. Direct blockers and still-blocked push candidates call
   `S.EncroachingOn(C)` (`1039A77F..7AB`). The native parameter/result block
   seeds its result DWORD to0. If the event's Bool result is nonzero, pop the
   GMem mark and immediately return true. Otherwise continue to the next hit.

Important: ordinary native return true is a **veto result**, not exception
rollback. Earlier successfully pushed actors and event side effects are not
undone by this body when a later candidate vetoes. Only the current still-hit
candidate received the reverse movement attempt. A portable enclosing root
may roll back on unsupported work/errors, but must not erase these ordinary
native side effects merely because the original returned true.

The source hash must still be real for rechecks. Original code dereferences
it after moveSmooth without an absent-hash recovery branch. A portable stale
generation/hash/object refusal is a safety boundary, not retail false.

## Blocking matrix: exact inputs, not a generic collide bit

Let IsBrush(A) = nonnull raw A.Brush && actual Brush ancestry. LocalPlayer(A)
means actual PlayerPawn ancestry followed by its nonnull native Player pointer;
it neither scans the world nor reads bIsPlayer. The ordered inline matrix is:

| First applicable case | Required live result |
| --- | --- |
| C == S.Level (`+A8`) | S.bCollideWorld |
| C.IsBrush | S.bCollideWorld && (LocalPlayer(S) ? C.bBlockPlayers : C.bBlockActors) |
| S.IsBrush | C.bCollideWorld && (LocalPlayer(C) ? S.bBlockPlayers : S.bBlockActors) |
| ordinary pair | selected C block bit && selected S block bit; choose BlockPlayers for a local player OR Projectile on the opposite side, else BlockActors |

Preserve lazy read order and both-brush precedence from the helper audit.
Native class objects are Brush `10582A28`, PlayerPawn `1058C160`, Projectile
`10578850`, LevelInfo `1057BF00`. `S.Level` is NOT S.Base (`+C4`). First-pass
Actors[0] exclusion precedes this matrix; it does not authorize changing the
matrix's own Level branch. There is no bCollideActors test inside the matrix.

## Optional stale-touch cleanup and mandatory second pass

After the first pass finishes without veto, if T !=0, scan source Touching[0..3]
in ascending index (`1039A7E7..81D`). Read each slot live. For each nonnull peer,
call `S.IsOverlapping(peer)`. If false, reread that same current slot and call
`S.EndTouch(currentSlot,0)`. The slot index then advances even if callbacks
alter other entries; this is not a restart-until-clean loop.

At `1039A81F`, traverse the ORIGINAL first query's hit chain again. Skip S and
actual ULevel.Actors[0], with the same LevelInfo assertions. Re-evaluate live
`S.IsBlockedBy(C)` through the exported helper at `1039A891`; do not reuse the
first-pass result, since push/event work can change fields.

- If blocking, call **C.EncroachedBy(S)** (`1039A89A..8BC`), even when T ==0.
- If nonblocking and T !=0, call **S.BeginTouch(C)** (`1039A8C9..8D6`).
- If nonblocking and T ==0, do nothing for that record.

There is no new overlap recheck on this second pass and no special exclusion
of a candidate successfully pushed out on the first pass. The first original
hit list remains authoritative for this event pass. Follow its live Next after
each callback. Finally pop the mark and return false (`1039A8E2..90D`).

## Original event schemas and dispatch order

All four original Engine.Actor declarations below have nativeIndex0,
functionFlags `0800`, scalar `Other` ObjectProperty constrained to Engine.Actor,
parameter flags `0080`. Only EncroachingOn has a scalar Bool ReturnValue,
flags `0580`. Their base Actor bodies are `Return Nothing`; that does not
permit skipping derived/state overrides.

| Event | Receiver and Other | Native caller result seed |
| --- | --- | --- |
| EncroachingOn | source S, candidate C | DWORD0; nonzero vetoes |
| EncroachedBy | candidate C, source S | no return |
| Touch | the side whose Touching slot was just inserted, opposite actor | no return |
| UnTouch | the side whose matching Touching slot was just cleared, opposite actor | no return |

Main event globals are exported `ENGINE_EncroachingOn=105836D4` (ordinal929),
`ENGINE_EncroachedBy=105836D0` (ordinal928), `ENGINE_Touch=10583704` (ordinal992)
and `ENGINE_UnTouch=10583700` (ordinal997). Calls use imported
FindFunctionChecked at `10599A88`, then receiver virtual ProcessEvent slot10h,
not a pinned Engine.Actor body or direct class-name callback.

## Touch relationship dependencies: do not substitute the old host UnTouch

The original Actor.Touching declaration is ObjectProperty[4], flags2,
constrained to Actor. Slots reside at native Actor `+E8..F4`.

IsOverlapping thunk `103029BE` reaches `10369430..10369521`. It returns false
if either actor IsBrush, or peer == receiver.Level. Otherwise it requires both
strict cylinder inequalities: horizontal squared separation < squared summed
radii, and squared Z separation < squared summed heights. Tangency is false;
both comparisons use the recovered retained x87 operations, not a newly
introduced epsilon, absolute-Z approximation or brush PointCheck. No collide,
delete, Owner/Base or self gate is present in this helper. The separately
implemented retained-arithmetic kernel and actual first-map typed provider
are documented in ORIGINAL-ACTOR-OVERLAP.md; these equations alone are not an
ARM rounding oracle.

BeginTouch thunk `1030177B` reaches `10369570..103695C5`. It first executes the
one-sided helper `(S,C)` at `10369630..103698B4`, and only if that returns true
executes `(C,S)`. That helper:

- Scans all four live slots, remembers the LAST empty slot, and immediately
  succeeds if C is already present.
- When full, first evicts Role0 peers via EndTouch(peer,0); if still full and
  incoming C has Pawn ancestry, it evicts non-Pawn peers. If still full and C's
  native Pawn `+318` mask02 is set, it prefers eviction of a peer that is not
  likewise Pawn with that mask. This bIsPlayer priority is NOT LocalPlayer.
- With a selected nonnegative slot index, writes C, emits S.Touch(C), then rereads that
  inserted slot. If the callback changed it away from C, returns false so the
  reciprocal helper is not attempted. A full/no-replacement path or duplicate
  returns true; do not fabricate a guaranteed symmetric relation. The selected
  replacement index is not revalidated as empty after EndTouch callbacks.

EndTouch thunk `10302365` reaches `10369970..10369A48`. It clears the FIRST
matching S slot, then immediately emits S.UnTouch(C) unless its integer second
argument is nonzero. It then searches C's current four slots, clears the FIRST
matching reciprocal slot, and emits C.UnTouch(S), regardless of the suppression
flag and regardless of whether a first-side match existed. No bDeleteMe gate,
touchEventSent bit, all-duplicates loop or automatic slot compaction occurs.
The older portable host UnTouch has different guards/history semantics; it is
not this original native helper and cannot be reused without an exact adapter.

The separate complete protocol and qualified-slot/event runtime adapter are
now implemented; see ORIGINAL-ACTOR-TOUCH.md. Their actual first-map entry is
rollback-only and respects cold-world event suppression. They are not yet a
publishing integration of this higher Level encroachment operation.

## Memory marks, exceptions and smallest faithful remaining integration

Both normal return paths restore GMem Top and, if the chunk changed, call
imported FMemStack.FreeChunks at `10599CBC` first. The mark covers the original
hit list AND all push requery allocations. It does not snapshot actors, tags,
script state or RNG. The early noncolliding return allocates no mark.

EH stub `10420510` selects metadata `104474F0`. The recorded unwind map has
two entries whose action words are zero. The observed catch tail logs/rethrows;
there is no generated pose-restore or GMem-pop cleanup action to cite for
exceptional exits. Ordinary reverse-push Location restoration is explicit
after moveSmooth returns. Portable RAII pose restoration and outer transaction
rollback on exceptions/refusals are deliberate stronger safety policies, not
a claim about retail EH atomicity.

The smallest complete next integration must combine: the actual first-Training
Level.Hash/root; the implemented lower source-pose/query adapter; exact lazy
blocking helpers; authentic collision-aware moveSmooth/MoveActor and its
sliding/TwoWallAdjust dependencies; original event schemas/override dispatch;
the original touching helpers/numeric IsOverlapping; and shared bounded work,
memory admission, generation guards and exception rollback. Rechecks, pushes,
callbacks and ordinary veto side effects must stay in that same root. Current
Location/Rotation/block fields, Brush pointers, Player binding and touch slots
must be read live after each relevant callback, not cached at entry.

Neither returning false for zero delta nor refusing only nonzero translation
provides this contract. The full native Move/SetRotation callers and remaining
movement dependencies require their own source audit/integration. No other map
was loaded or integrated by this audit.

The subsequent complete detached higher protocol now exists in
`native/quest_level_encroachment.h`, with independent control-flow/provider/
callback/refusal tests. It does not supply the real collision-aware MoveActor
provider or a publishing runtime entry. See
ORIGINAL-LEVEL-ENCROACHMENT-PROTOCOL.md. The separate complete MoveActor audit
is ORIGINAL-LEVEL-MOVE-ACTOR.md; its numeric prequery/backoff value service is
ORIGINAL-ACTOR-MOVE-GEOMETRY.md. None of those standalone results establishes
first-map support/falling, live destruction or surviving loot.
