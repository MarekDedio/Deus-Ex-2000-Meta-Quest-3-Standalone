# Original Window.Show and the next HUD sizing call (2026-10-27)

First-Training native1413 ingress/hidden-parent implementation. Verified
checkpoint; not completed root visibility, startup, live VR crates,
loot/gravity or a packaged APK. No accepted VR HUD layout, budget or save format
was changed. Commercial originals were inspected only as static data.

## Exact ingress and reused visibility authority

Extension.Window.Show is native1413, flags0x401. Its optional bShow is a
dimension1 BoolProperty flags0x90. Exec10051960..100519A3 initializes INT1,
Steps the argument in the caller frame, advances EndFunctionParms and calls
SetVisibility1004C7D0. It is not the separate no-argument XWindow.Show export.
The VM copies ordinary Bool inputs and materializes explicit omission/NoWrite
using the true seed, not the declaration's false zero. Malformed packets,
foreign function/source, non-Window receiver and excess inputs fail explicitly.

SetVisibility1004C7D0..1004C85B first compares the local canonical visible bit.
Equality returns without inspecting the parent or issuing callbacks/writes.
A change dispatches the actual parent virtual+0x84 under its callback context,
or calls direct1004EE20 when unparented. The port reuses its existing audited
typed visibility/parent implementation rather than a direct unconditional flag
assignment. Native Modal/TabGroup/Radio hide paths reconfigure the actual parent;
Tile reconfigures for either direction. Window-derived parents retain their
actual most-derived ChildRequestedVisibilityChange script dispatch, including
DeusExHUD.ConfigurationChanged. A callback need not grant the requested change.

Effective-visible focus/grab/init-tree/layout work remains a separate explicit
unsupported service; the wrapper does not conceal it behind a successful flag
write. The genuine later InitRootWindow guard remains intact. All writes and
callbacks use the caller's existing transaction and shared execution budgets.

## Accepted targeted controls and observed HUD state

VM696622 checks/930 refusal controls pass. The visibility-input subset is130/
12 and tests declared/native-index entry, optional true, Nothing, NoWrite,
full four-byte null-context clearing, explicit false, ordinary snapshots and
partial-Boolean clearing refusals. Other fixture return values only observe
inputs; they are not claimed as a real Bool return from the void Show method.

The owned GC/Window suite passes155 top-level assertions/16 failed-root controls:
10763 writes/190634 work,60 Window cases and18 text cases. New coverage includes
four direct ingress guards, all local/requested combinations, repeated no-op,
actual modal hide-only reconfiguration, omitted/Nothing/NoWrite true defaults,
equal unparented calls and equality before reading an explicit cyclic parent
fixture (restored immediately). Direct guards are not independent failed VM
roots. Existing cold/warm, early/late write/work, save/reload and whole-root
save/object/GC/counter undo remain checked.

The actual dormant player's bObjectBeltVisible is false. Original Show(false)
therefore hides the belt and dispatches DeusExHUD's real parent callback. Its
size queries now measure the received-item label using original Font metrics,
leave its context correctly released and enter the mission-start size query.
Owned HUD passes10180 controls at the exact later boundary:
HUDMissionStartTextDisplay.ParentRequestedPreferredSize PC18/opcode100,
unsupported GC.SetFont1242,14459 writes and195 actual native objects.

The complete snapshot remains within4MiB. Assertions distinguish two completed
text queries/free GC nodes (TextLog and received-item label) from the active
mission-start query/stack node. They check ownership/links/counts, all27 saved
or copied context fields, actual quickbar setting/visibility and whole-root
rollback; the old193-object/1413 expectation is not retained as passing evidence.
This isolated player's original setting does not alter the accepted timed VR
quickbar presentation. Full desktop build, current Android-native library link
and strict ARM64 pass. CTest registers269:225 executed,44 explicit owned-data
skips,zero failures166.96s. All eight separate rebuilt owned first-Training
regressions pass:crate415/5,deferred429/21,animation588/24,FastTrace252/0,
Input classes10413/38,viewport10310/20,materials and1232 interaction/save-route
controls. All45 originals/23 frozen compiler inputs match length,UTC modification
time and SHA256 baselines; all compiler/test/probe jobs are terminal.

Actual original two-Fire cold/warm repeat2 reaches native1242 at255080
instructions/31256 writes. Both retain48 observations,two attack notifies,
four traces,two crate changes,nine births and full save/world/GC undo. Door
repeat2 retains the exact unprepared ReadInput refusal at239536/19432, both
56-unit moves/two endpoints and full undo. Only first Training is loaded;
neither diagnostic publishes a completed player or live physics.

## Next actually reached original services, not implemented here

The original mission-start size callback does GetGC1485, GC.SetFont1242,
GC.GetTextExtent1280 with Window.ConvertScriptString1469 applied to Message,
adds shadowDist to width/height and finally ReleaseGC1486. Existing exact text
measurement and script-string conversion helpers do not by themselves admit
these new native ingress/OUT-temporary protocols.

- GC.SetFont1242 has flags0x401 and one required NewFont ObjectProperty80/
  Engine.Font. Exec1002B6C0..1002B704 initializes None, copies its input, then
  calls10027640 with that same pointer twice. That method writes normalFont+7C
  then boldFont+80, unconditionally, with no layout or color/spacing effects.
- GC.GetTextExtent1280 has flags0x401:required Float destWidth80, Float OUT
  xExtent180, Float OUT yExtent180, required Str textStr400880. Its wrapper's
  exact initialized copies/OUT-address capture/publication order still needs
  the separate ingress audit before using the existing measurement provider.
- Window.ConvertScriptString1469 has flags0x401, oldStr Str400080 and result
  Str400580. Its wrapper/result-copy semantics must be audited; the underlying
  |n/bar/UTF16 algorithm is already implemented for SetText1550.

Only first Training is in scope. No account/character-creation UI, other-map
integration, commercial-code execution, startup bypass or live-world publication.
