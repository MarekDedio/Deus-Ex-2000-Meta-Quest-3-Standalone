# First Training: next reached GameReplicationInfo gate

Read-only next-step evidence, 2026-10-26. No GameReplicationInfo admission is
implemented here. Only first `Maps/00_Training.dx` is integrated/tested.

After exact Decal admission and the original native721 script adapter, cold
and warm Training updates reach `00_Training.GameReplicationInfo0`, exact
native `Engine.GameReplicationInfo`, at239,200 instructions /19,362 writes,
141 slots /140 calls,13 stasis returns and12 deferred entries. The conservative
native-class guard refuses Engine.GameReplicationInfo. Full gameplay rollback
and cold same-filename map replacement pass; the frame still does not complete.

## Independent owned source proof

Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Engine.u SHA256:
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
Original binaries are inspected only as data, never executed or loaded.

Actual top-level native Class metadata, including matching first payload
superclass references and ObjClass0/ObjOuter0:

| Class | Ref/base | Object flags | Payload offset/length |
| --- | --- | --- | --- |
| GameReplicationInfo |108/805 |040F0004 |86078/262 |
| ReplicationInfo |805/91 |040F0004 |106561/93 |
| Info |91/2 |040F0004 |85749/91 |
| Actor |2/-3 |040F0004 |61225/1236 |

The extra native ReplicationInfo link is required. Neither the two-link
GameInfo/Info guard nor direct Decal/Actor guard proves this ancestry.

Engine.dll export-name/EAT645 (ordinal646),
`??_7AGameReplicationInfo@@6B@`, RVA127428 gives vtable10427428:

| Slot/cell | Pointer / original Actor service |
| --- | --- |
| +10/10427438 |10302D74 ProcessEvent |
| +14/1042743C |10301F00 ProcessState ->103E5B60 |
| +2C/10427454 |10302167 IsPendingKill |
| +64/1042748C |10303DAA Tick ->103A1AA0 |
| +8C/104274B4 |10303242 performPhysics ->103C9770 |

Fresh thunk bytes match prior Actor audits: ProcessState E95B3C0E00,
Tick E9F1DC0900, physics E929650C00. These slots do not authorize arbitrary
native ReplicationInfo descendants, scalar teardown or skipped callbacks.

## Next implementation requirements

Authenticate the full canonical native GameReplicationInfo -> ReplicationInfo
-> Info -> Actor chain, preserve selected script/state/timer/lifespan/physics,
and separately admit the proven animation callbacks only for that exact
native class. Keep base ReplicationInfo/Info and unknown nearer native classes
closed. Do not skip this object because the target is standalone; standalone
still reaches it in the actual original first-map update.

Add generated cross-package metadata, Float event, owner/deferred/checkpoint,
actual animation callback and whole-root late/write/work undo controls with
malformed intermediate chains. Repeat unfiltered Training, paired doors and
actual crowbar attacks, preserving cold/warm model-proof lifetime checks.
This is not a completed startup, collectible loot, gravity or headset claim.
