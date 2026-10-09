# Original AI event-manager boundary

This began as read-only characterization, not working AI. The 2026-10-10 event
state implementation uses the established initialization/mutation contracts;
processing, perception and deferred cleanup remain unfinished. See
[AI-EVENT-STATE.md](AI-EVENT-STATE.md). The pinned SurrealEngine
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
| UEventManager.AIClearEventCallback | 103015e6 | 10382b90 |
| UEventManager.AISendEvent | 1030218a | 10382c60 |
| UEventManager.AIStartEvent | 10301e24 | 10382fe0 |
| UEventManager.AIEndEvent | 10302a5e | 103831e0 |
| UEventManager.AIClearEvent | 10301dd9 | 103832c0 |
| UEventManager.DestroyActor | 10301032 | 10382760 |
| UEventManager.Serialize | 10303189 | 103825f0 |
| UEventManager.AIProcess | 10303959 | 10384080 |
| UEventManager.Tick | 10301843 | 103828f0 |
| UEventManager.CleanupEvents | 10303940 | 10383d80 |
| UEventManager.CleanupSlot | 10303de1 | 10383cb0 |

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
- Per-event node lists append, including tombstones. New receivers insert before
  the fixed head of the circular processing ring; registration does not advance
  it. New receiver history cursors use the manager's current cursor.
- IsPendingKill at 1030df80 reads actor+0x28 bit7. The cleanup assertions at
  103966bd/1039668e name this exact bit `bDeleteMe`. Start/Send retain sender
  identity but force intensity and radius to zero for a pending-kill actor.
- Native716 zeroes all current sender channels without deleting its node or
  erasing history; native711 instead marks a receiver for deferred deletion.
- The manager's DestroyActor notification is called at 1039679a by
  ULevel::CleanupDestroyed (body103965a0), just before actor destruction. It is
  not a synchronous hook in ULevel::DestroyActor (body10395ba0). The portable
  Destroy implementation must not invent that timing.
- Core.dll IsSaving thunk101013a2 resolves to 10106be0, reading archive+0x10.
  Manager Serialize uses this gate to clean pending deletions on save, then
  asserts zero processing depth/deletion count. Portable codec6 deliberately
  preserves graph snapshots, including tombstones; it is not original archive
  normalization or binary save compatibility.
- CleanupEvents runs only at zero processing depth with pending deletions.
  It repairs the receiver ring before freeing nodes: a live head remains,
  otherwise the first forward live successor becomes head (or null if none).
  Buckets and event chains retain native order; each event's sender list is
  cleaned before its receiver list. CleanupSlot unlinks a deleted node before
  invoking its deleting destructor, then decrements the pending count. Empty
  event types persist and live histories/detection/cursors are unchanged.
  Engine's two direct cleanup callers are Serialize1038264c (saving) and
  Tick10382921 (after AIProcess); this does not exclude indirect/foreign calls.
  Node destructors do not recursively delete referenced actors or peers.
  This cleanup contract is characterized but not yet implemented in the port.
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
behavior and level-tick gates remain unresolved. Portable native-graph
snapshots are implemented separately from original archive normalization.
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
and disassembly excerpts are not included in Git. Registration alone must not be
reported as working AI. Remaining work includes actual level initialization/
ticking, processing/perception/callback delivery, native deferred cleanup and
campaign-compatible save/travel integration.
