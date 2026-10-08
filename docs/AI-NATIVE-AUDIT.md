# Original AI event-manager boundary

This is a read-only characterization for future implementation, not working AI.
No AI native was enabled by the persistent-state batch. The pinned SurrealEngine
Actor AI methods log unimplemented, so their empty bodies cannot justify
successful no-op replacements.

The user's original `System/Engine.dll` was statically inspected without loading
or modifying it. Its SHA-256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Addresses below are preferred-image VAs for that PE32 binary (base `10300000`),
not portable memory layouts or live process addresses. Exported E9 thunks were
resolved before interpreting the real bodies; nearest-export labels emitted by
the disassembler are not reliable function names.

| Original function | Export thunk | Body |
| --- | --- | --- |
| LevelInfo.execInitEventManager | 103023b0 | 10384a90 |
| UEventManager.AISetEventCallback | 10303d0a | 10382990 |
| UEventManager.AIStartEvent | 10301e24 | 10382fe0 |
| UEventManager.AIEndEvent | 10302a5e | 103831e0 |
| UEventManager.AIProcess | 10303959 | 10384080 |
| UEventManager.Tick | 10301843 | 103828f0 |

## Established constraints

- Original `Engine.LevelInfo.PreBeginPlay` bytecode invokes InitEventManager
  (650) at logical PC 6. The native constructs the manager only if absent.
  Leaving it null to avoid later AI dependencies would omit authored startup.
- Actor wrappers obtain the manager through XLevel's Actors(0) LevelInfo.
  They forward when present and return when the manager is null. That real
  branch is not proof that every AI invocation can be treated as a no-op.
- Registration owns sender/receiver identity, callback/score names, perception
  flags and processing-ring state. Updating registration does not reset prior
  detection; deletion and cleanup are deferred.
- Start/Send/End mutate sender sensory channels and sixteen history slots.
  End returns unchanged if the actor/name sender is absent; otherwise it zeroes
  the current channel but retains recent slot maxima. Send records a pulse
  without enabling a persistent current emission. A name-to-bool map is not
  equivalent.
- Start/End do not deliver script callbacks synchronously. Manager Tick calls
  AIProcess, then cleanup. AIProcess scores candidates and computes perception,
  then a later callback pass calls AIEvent/ProcessEvent. Reentrant script/native
  mutations must therefore share safe lifecycle and rollback boundaries.
- Fresh original reflection shows DistressScore's receiver, Sender and Score
  are ordinary parameters (`0x80`); ReturnValue is a returned Float (`0x580`).
  Score is not an out parameter. Dynamic casts, alliance queries and a typed
  XAIParams value are further execution dependencies.

## Unresolved scope and reproduction

Scheduler time-budget/fairness, comparator/tie policy, full perception/trace
behavior, level-tick gates and native graph persistence remain unresolved.
Static inspection is not dynamic equivalence or complete NPC startup.

Read-only export/disassembly tools available locally are `llvm-readobj.exe
--coff-exports` and `llvm-objdump.exe -d --x86-asm-syntax=intel
--start-address=... --stop-address=...` from Android NDK 27.0.12077973.
Original script declarations/calls can be inspected with the built host tool:

```powershell
.\desktop\build\script_bytecode_inspect.exe 'D:\Steam\steamapps\common\Deus Ex' Engine.LevelInfo.PreBeginPlay
.\desktop\build\script_bytecode_inspect.exe 'D:\Steam\steamapps\common\Deus Ex' DeusEx.ScriptedPawn.DistressScore
```

Local generated primary excerpts and the detailed address/confidence report are
in ignored `artifacts/state-execution-20261008/native-audit/`. Commercial binaries
and disassembly excerpts are not included in Git. The next explicit vertical
slice needs real manager initialization, typed/history-preserving mutations,
lifecycle journaling and rejection at unsupported processing; registration alone
must not be reported as working AI.
