# ClassContext: next original startup boundary

Read-only audit, 2026-10-13. No ClassContext implementation or successful original
world startup is claimed. Original Windows executables/DLLs were not executed.

## Actual original expression

The fresh non-publishing Training probe now passes native280 and stops at
PreBeginPlay slot1331, `00_Training.Camera4`, actual
`Engine.Pawn.InitPlayerReplicationInfo` PC36/opcode18. Original bytecode is:

```text
PC21 Let
PC22 Context PlayerReplicationInfo
PC31 InstanceVariable PlayerReplicationInfo.PlayerName
PC36 ClassContext
PC37 ObjectConst Engine.GameInfo
     nullSkip=5, zeroSize=0
PC45 DefaultVariable GameInfo.DefaultPlayerName
PC50 Return
```

The selector is the actual imported Engine.GameInfo UClass, not the active
DeusExGameInfo instance or its derived class defaults. Current VM execution
unconditionally refuses opcode0x12 with
`ClassContext requires class-default object identity`. This does not prove that
the class/default-value storage is missing: `DefaultClass`, `Read` and
`classDefaultValues` already represent this exact qualified reflected field.

Existing `ReferenceFor` validates qualified property ownership on writes, not
reads. New class-context reads must validate actual class/property ownership and
scalar/fixed-array bounds, preventing foreign fields from falling through by
unqualified name or default-zero. Class-default persistence and journal semantics
must remain concrete-class isolated.

## Owned retail Core.dll evidence

SHA256: `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Named export thunks were followed through bounded static disassembly:

- execClassContext thunk10101091, body1012edb0. Selector executes once against
  FFrame.Object (`[ESI+8]`) at1012eddb..1012edef. A valid class supplies default
  storage as child receiver ECX at1012ee37..1012ee47 without replacing FFrame.
- Null selector warns, clears property/lvalue globals, skips `skip+3`, and
  zero-fills the encoded result byte count at1012ee61..1012eeb7. It does not throw.
- execDefaultVariable thunk10101690, body1012ed00 uses receiver's class default
  storage and actual reflected property offset.
- execSelf thunk101013ca, body10130060 returns receiver ECX directly. Thus child
  Self sees the default-object context; preserving FFrame does not imply Self
  stays the caller.

Pinned SurrealEngine differs in null handling and Self context, so it is useful
comparison evidence, not retail authority for these branches. The existing VM
hardcodes `frame.self` and ordinary child evaluation carries an outer context;
merely replacing the opcode refusal with `Eval(child,classPath)` would silently
misrepresent broader behavior. A UClass path is not a generic default-object
identity for casts, instance fields, function/native calls or event dispatch.
The parser currently discards ZeroFillSize; that metadata needs preservation and
validation before any null/result-layout behavior can be claimed.

## Implementation and proof requirements

The exact original property path can use a validated non-null loaded script
Actor UClass, direct DefaultVariable child and existing defaults/reference
journal, without inventing a generic CDO identity. Unsupported shapes must stay
explicitly guarded while broader identity/context semantics are implemented.
This bounded increment alone does not finish ClassContext or campaign startup.

Required controls include exact Engine.GameInfo.DefaultPlayerName versus derived
defaults; one selector evaluation; foreign/non-class/inactive identities;
qualified inherited field and fixed-array ownership; default writes and complete
later-failure/budget rollback; cold defaults persistence and concrete-class
isolation. General child Self, nested contexts and calls require their own
retail identity audit. Null behavior must follow actual zero-fill/result metadata,
not a guessed exception policy described as original engine behavior.

Then rerun the unchanged fresh-world probe, deliberately update its exact next
boundary, and retain complete original save/GC/actor/revision rollback controls.
No guessed player name or skipped original callback is an acceptable substitute.
