# Original ToggleWindow setter and notifications

2026-10-22. Static audit for the actual first `Maps/00_Training.dx` startup
boundary, not a completed implementation. Native1611 is **not implemented**
at this checkpoint. Original Windows executables/DLLs were never executed or
loaded; the owned DLL was read as bytes/disassembled, and our package inspector
read original `.u` metadata/bytecode.

## Owned source and declaration

`System/Extension.dll`, SHA-256:
`93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C`.
Addresses below are original image virtual addresses, image base `10000000`.

Original `Extension.u` declares `Extension.ToggleWindow.SetToggle`, native1611,
function flags`0x401`, with one required `BoolProperty` parameter
`Extension.ToggleWindow.SetToggle.bNewToggle`, dimension1, flags`0x80`, and no
return parameter. Do not confuse a PE export ordinal with the script native ID.

`execSetToggle` is PE export1869/RVA`49D10`, body`10049D10..10049D53`.
It initializes its four-byte input temporary to0 before evaluating the argument,
consumes EndFunctionParms, and calls `SetToggle100499B0`. This initialized seed
is authority for a no-result argument; an arbitrary reflected zero is not.

## Setter: equality precedes every side effect

`SetToggle` is PE export1309/RVA`499B0`, body`100499B0..10049A19`:

1. Read bit0 of the Button pressed flag word at`this+174` and compare it with
   the incoming native INT Bool.
2. Equal: return without writes, appearance refresh, callbacks or sounds.
3. Different: replace only bit0, preserving the other packed flags.
4. Call `XButtonWindow.ChangeButtonAppearance10008380`.
5. Call `XToggleWindow.ChangeToggle10049910`.

The typed reflected flag is `Extension.ButtonWindow.bButtonPressed`
(`BoolProperty`, dimension1, Const flags2); it is shared by ToggleWindow, not a
separate fabricated `bToggle`. The existing constructor clears it, along with
mouse-pressed/auto-repeat flags. The setter does not change those other flags.

Appearance uses existing Button `Info[6]`, effective sensitivity and root focus,
with pressed/normal texture fallback. Original final stores at
`1000841E..1000842D` are `curTextColor`, `curTexture`, then `curTileColor`.
The port already has `ChangeNativeButtonAppearance`; that does not implement
the missing toggle setter or notification chain. Its current reflected-store
order is texture/tile/text, so compare/correct that order against the audited
text/texture/tile stores when implementing this native. This setter plays no
sound.

## Reentrant notification chain

`ChangeToggle10049910..100499A1` starts with the Toggle itself. For every
receiver it rereads the **original Toggle's current pressed bit**, then invokes
the receiver's virtual`+F0`, `ToggleChanged(originalToggle,currentBool)`.
Any nonzero return stops propagation. Otherwise it reads that receiver's
**live** `winParent` at`+138` after the callback and continues. Do not freeze the
initial Bool or ancestor list across callbacks.

`XWindow.ToggleChanged10003D80..10003DB5` invokes the optional original script
event through`1004DED0`, with an initialized false return. Original declaration
`Extension.Window.ToggleChanged` is event flags`0x802`: `Button` is an
`ObjectProperty` constrained to `Extension.Window`, `bNewToggle` is Bool, both
dimension1/flags`0x80`; Bool `ReturnValue` has flags`0x580`. Its default script
returns false. Most-derived/state-aware script dispatch must remain in the
same VM/root transaction, not a detached callback runner.

### Native RadioBox override

`XRadioBoxWindow.ToggleChanged10038A60..10038B5C` first handles actual Toggle
sources and its registered native `toggleButtons` list:

- True: find the source in the list. If already `currentSelection`, return1.
  Otherwise store the new `currentSelection` **before** calling the former
  selection's `SetToggle(false)`, if nonnull; then invoke the Radio script event.
  A source absent from the list falls through to the script event.
- False, `bOneCheck=true`: if the source is current, call its `SetToggle(true)`;
  return1 whether or not it was current.
- False, `bOneCheck=false`: if the source is current, clear `currentSelection`
  and invoke the Radio script event; otherwise return1.
- Other/unhandled cases invoke the Radio's optional script event, returning its
  initialized-false result. A handled native branch skips that script callback.

Typed existing fields are `Extension.RadioBoxWindow.bOneCheck` (Bool) and
`Extension.RadioBoxWindow.currentSelection` (Object constrained to ToggleWindow).
The native list is owned/journaled storage, not raw reflected pointer bytes.
Original belt initialization sets `bOneCheck=false` after the Radio constructor.

## Actual Training dependency and verification plan

See [TRAINING-RUNTIME-MEMORY.md](TRAINING-RUNTIME-MEMORY.md) for the preceding
live/save budget separation. The actual first Training probe now passes Ammo
construction and stops in `DeusEx.HUDObjectSlot.SetItem`, PC69/opcode102,
`Unsupported runtime native 1611`, after209469 instructions/20928 writes.
Evidence: `artifacts/training-runtime-budget-startup-20261022.log`.
Call stack includes Possess, root/HUD initialization, `HUDObjectBelt.PopulateBelt`,
`AddObjectToBelt`, `ClearPosition`, then `SetItem`.

`SetItem(None)` first calls `HighlightSelect(false)`, then `SetToggle(false)`.
A freshly initialized unpressed slot should take the authentic equality no-op.
That observation must not become a global false/no-op implementation: actual
`HUDObjectSlot.ToggleChanged` reentrantly calls `SetToggle(false)` and returns
true when an empty slot is selected.

Focused behavior tests should establish unchanged true/false no-op; changed bit
and appearance-before-Self ordering; empty-slot reentrant rejection; refreshed
Bool/live-parent propagation and handled stops; Radio replacement-before-old
reset and both `bOneCheck` branches; shared work/write limits; full pressed,
appearance, selection, native-list and root rollback after a later failure.
Then repeat the untouched actual first Training startup and report its next
precise boundary. These are proposed checks, not passing-test claims.

No new APK, live possession/HUD acceptance, crate damage, usable loot or gravity
is established by this static audit. Nested DamageHUD struct arrays are a
different later dependency and are not the current observed boundary.
