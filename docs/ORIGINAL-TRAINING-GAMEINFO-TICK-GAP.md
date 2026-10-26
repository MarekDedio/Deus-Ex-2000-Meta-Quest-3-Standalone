# First Training: inherited GameInfo/Mutator native updates

Implementation and independent source evidence, 2026-10-26. Integration/test
content remains only the original first
`Maps/00_Training.dx`; live startup, automatic gravity and collectible loot
are not completed by an expected-refusal diagnostic.

## Historical gate reached after immutable validation reuse

Cold and warm first-map updates both get beyond the previous Model validation
cap and reach `00_Training.DeusExGameInfo0`, concrete `DeusEx.DeusExGameInfo`,
nearest native `Engine.GameInfo`. Both retain237,922 script instructions /
19,289 writes,121 visited slots /120 calls,13 stasis returns,12 deferred entries
and zero completed pending passes. The exact refusal is:

`Original Actor Tick reached an unimplemented native virtual override: Engine.GameInfo`

This is the conservative native-class admission gate. It is not evidence
that GameInfo necessarily has a different Tick algorithm. GameInfo is the
world's game-rules object, not an account/login or character-creation UI.

## Independent static slots

GNU objdump reads the owned PE32 as bytes; no commercial DLL/EXE is executed,
loaded, linked, patched or rewritten. Owned Engine.dll SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.

Engine export-name/EAT index644 (ordinal645) is `??_7AGameInfo@@6B@`,
RVA127AE0, preferred-image vtable10427AE0. Raw .rdata words show:

| Slot | Cell | Pointer / already authenticated Actor implementation |
| --- | --- | --- |
| +10 |10427AF0 |10302D74 ProcessEvent |
| +14 |10427AF4 |10301F00 ProcessState |
| +2C |10427B0C |10302167 IsPendingKill |
| +64 |10427B44 |10303DAA Actor.Tick thunk to103A1AA0 |
| +8C |10427B6C |10303242 Actor.performPhysics thunk to103C9770 |

The prior Actor/Weapon audits establish these shared thunk bodies. Matching
these particular slots does not authorize GameInfo scalar teardown, arbitrary
other native Info subclasses, a broad inherited-ClassFlags test or any
unvisited native service.

## Canonical original metadata, checked 2026-10-26

Owned Engine.u SHA256:
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
An independent bounded read of the v68/licensee0 name/export tables and each
Class payload's first UField superclass reference gives:

| Actual top-level Class | Reference | Table/payload superclass | Object flags | Payload offset/length |
| --- | --- | --- | --- | --- |
| Engine.GameInfo |13 |91 (Info) |040F0004 |73129 /4678 |
| Engine.Mutator |46 |91 (Info) |040F0004 |83632 /405 |
| Engine.Info |91 |2 (Actor) |040F0004 |85749 /91 |
| Engine.Actor |2 |-3 (Core.Object import) |040F0004 |61225 /1236 |

All have ObjClass0/ObjOuter0 and matching table/payload superclass links;
neither GameInfo nor Info carries HasStack. The independent reader also
reproduces the previously audited Inventory ref6/base2 and Weapon ref9/base6,
including their original offsets/lengths. These RF_Native object flags, not
inherited ClassFlags or an English class name, define the native boundaries.

### Next reached rule object: Mutator

The first implementation/controls admit exact GameInfo successfully. Actual
Training repeat2 then reaches `00_Training.Mutator0`, exact `Engine.Mutator`,
at237,922 instructions /19,290 writes,122 slots /121 calls. The source is not
skipped: the next conservative class gate reports Engine.Mutator. The same
canonical native Mutator -> Info -> Actor chain is independently confirmed
in the table above, not inferred merely from its English name.

Owned Engine.dll export-name/EAT index659 (ordinal660), `??_7AMutator@@6B@`,
RVA127B8C gives native vtable10427B8C. Independent .rdata bytes give +10 at
10427B9C ->10302D74, +14 at10427BA0 ->10301F00, +2C at10427BB8 ->10302167,
+64 at10427BF0 ->10303DAA and +8C at10427C18 ->10303242. These are the same
five audited Actor services, not a new Mutator algorithm or scalar teardown.
Exact Mutator support is therefore included in the same implementation batch,
with its own canonical identity and the same generated control matrix.

The project's existing read-only script inspector finds no direct exported
DeusExGameInfo.Tick, GameInfo.Tick or Info.Tick function. Actual Actor.Tick
is its original nativeIndex0/flags800/one Float DeltaTime event with two-byte
Return/Nothing body. This does not authorize substituting empty callbacks:
production must still resolve and execute the genuinely selected event.

That event dispatch remains conditional on the original native probing gates;
absence of a local Tick override does not justify forcing an event or skipping
the native state/timer/lifespan/physics tail.

## Narrow implementation and preserved requirements

`portable_actor_tick_tail.inl` now admits exact canonical GameInfo and Mutator
separately, requiring each actual native Class record and immediate native
Info -> canonical Actor chain. Unknown nearer native classes, missing/non-native
Info and direct-to-Actor substitutions still refuse. The shared ancestry helper
is called only by these two separately audited wrappers; it is not permission
for all Info subclasses or base Info itself.

Preserve real stasis/owner/parity/animation, selected script Tick,
state/timer/lifespan/physics and all later dependency refusals. Do not skip the
game-rules object, force Physics0 or fabricate successful startup.

`OriginalAnimationActorVirtuals` now includes these exact GameInfo/Mutator
+10/+2C implementations and invokes the same canonical-ancestry checks. The
proof supports only those matching slots, not a blanket Info allowance.

One generated production matrix runs independently for both native classes:
actual serialized cross-package metadata, selected Float Tick/Physics0,
owner-first/deferred LIFO and parity/checkpoint continuation, malformed/
nearer-native/base-Info refusals and whole-root late/write/work undo. A genuine
MAIN tween crosses Frame0, writes bAnimFinished and dispatches the checked
selected no-input AnimEnd through ProcessEvent/IsPendingKill, without a fake
Mesh or callback provider. Exact AnimEnd/native999 failure, later Tick failure
after that event, all animated write/work cuts and checkpoint non-replay are
covered as well. Generated Physics0 does not prove active rule-object physics.

Repeat the actual unfiltered first map, paired doors and two real crowbar attacks.
Retain cold/warm immutable validation and map-generation lifetime regressions.

## Verified implementation outcome

Both GameInfo and Mutator now execute the exact inherited Actor paths.
Generated controls add3,630 checks /82 deliberate refusals: GameInfo reaches
5,766 cumulative /108, Mutator7,583 /149. The complete production driver
reports12,455 /245 before its final malformed-schema control and exits0.
These include both real MAIN tween/AnimEnd, selected event packet, owner/LIFO,
checkpoint non-replay and all ordinary/animated rollback-prefix controls.

Full desktop rebuild, Android-native link, strict GNU generated-driver syntax
and strict ARM64 runtime syntax pass. CTest217 executed /256 registered /
39 explicit owned-data skips /zero failures71.62s. Separate actual crate415/5,
deferred429/21, animation588/24 and original FastTrace252/0 regressions pass;
FastTrace retains16 clear /32 blocked supplied first-map rays.

Actual cold/warm first-Training repeat2 passes both rule objects and reaches
Shadow0's native Engine.Decal gate at237,922 instructions /19,290 writes,
123 slots /122 calls. Paired doors retain both56-unit moves/two completion
callbacks before the same gate at238,258 /19,358. Two original crowbar Fire
cycles repeat2 retain228,837 /26,047 and48 observations/two cycles/two notifies/
four traces/two crate changes/nine births before later FExec. Every diagnostic
verifies whole gameplay undo and cold same-filename generation replacement.

All45 original baseline files and5 frozen compiler inputs retain exact
size/timestamp/SHA256; every build/test/probe job is terminal. No APK was
packaged. The next measured integration is recorded in
[the shadow Decal gate](ORIGINAL-TRAINING-DECAL-TICK-GAP.md); this is not a
completed world frame, live loot/gravity, headset acceptance or account UI.
