# First Training: next reached GameInfo native gate

Read-only next-step evidence, 2026-10-26. No GameInfo admission is implemented
by this audit. Integration/test content remains only the original first
`Maps/00_Training.dx`; live startup, automatic gravity and collectible loot
are not completed by an expected-refusal diagnostic.

## Reached after immutable validation reuse

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

## Required implementation evidence

Authenticate the actual Engine.u GameInfo Class record, canonical identity
and complete immediate superclass chain before adding its exact nearest-native
admission. Preserve real stasis/owner/parity/animation, selected script Tick,
state/timer/lifespan/physics and all later dependency refusals. Do not skip the
game-rules object, force Physics0 or fabricate successful startup.

Audit `OriginalAnimationActorVirtuals` separately: its current exact list does
not include GameInfo. The +10/+2C proof above supports only those matching
native slots, not a blanket Info allowance. Add genuine generated cross-package
metadata/selected Float Tick, native owner/deferred/checkpoint, malformed/
nearer-native and whole-root late/write/work undo controls, then repeat the
actual unfiltered first map, paired doors and two real crowbar attacks.
Retain cold/warm immutable validation and map-generation lifetime regressions.
