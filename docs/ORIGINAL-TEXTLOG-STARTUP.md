# Original TextLogWindow startup contract (2026-10-27)

Historical entry-stage audit: the constructor/setter/scalar work now described
in ORIGINAL-TEXTLOG-IMPLEMENTATION.md passes this boundary and reaches the
distinct TextWindow preferred-size dependency. The original contract and
addresses below are preserved as source evidence, not current failure sites.

This is a read-only original-code audit and next implementation contract, not an
implemented service or a playable-APK claim. First `Maps/00_Training.dx` remains
the only map target. No account login or character-creation interface is involved.
No original commercial executable or DLL was executed or loaded as code.

## Current actual boundary

`artifacts/viewport-input-final-training-fire-20261027.log` records two cold/warm
original Training runs reaching exactly:

`NewChild native subclass Init is not implemented: extension.textlogwindow`

The caller is `DeusEx.HUDLogDisplay.CreateControls`, logical PC110, native1410,
after239566 instructions/27664 writes. Original two-swing crowbar diagnostics
retain48 observations, two returned attack notifications, four traces, two crate
changes and nine actor births. They undo the whole root and publish no gameplay.
This boundary is not successful player/HUD startup, live controller damage,
automatic gravity, collectible loot or Quest acceptance.

The relevant current host sites are `native/portable_unreal_runtime.cpp`:

- `NativeWindowAncestor` near7019 and `NewChildWindowNative` near7033: constructor
  admission and native suffix dispatch; TextLog is currently refused before birth.
- `InitializeChildNativeWindowStorage` near7368: existing owned native-array
  construction; `InitializeChildTextWindow` near7443: audited inherited Text.Init.
- `OptionalNativeWindowEvent` near6769 and base child notifications near7198:
  actual native-ancestor allowlists also need authenticated inherited dispatch.
- `SetHiddenChildVisibility` near7135: parent virtual selection is explicit.
- `NativeWindowConfigurationChanged` near6111: TextLog must not be admitted as
  the plain Window implementation because it has a genuine native override.
- `NativeWindowPreferredSizeCallback` near5901: TextLog inherits the TextWindow
  override, not Window's plain script forwarding or Tile's override.

Line numbers are navigational and can move. Adding TextLog to every allowlist
without these distinct native contracts would be fabricated support.

## Reflected declarations authenticated from Extension.u

All four TextLog fields have actual outer `Extension.TextLogWindow`, dimension1:

| Qualified field | Type / constraint | Property flags |
| --- | --- | --- |
| `Extension.TextLogWindow.Lines` | StructProperty, `Extension.ExtensionObject.DynamicArray` | `0x1000` |
| `Extension.TextLogWindow.bPaused` | BoolProperty | `0` |
| `Extension.TextLogWindow.bTooTall` | BoolProperty | `0` |
| `Extension.TextLogWindow.textTimeout` | FloatProperty | `0` |

Inherited `Extension.TextWindow.HAlign` and `VAlign` are ByteProperty flags2,
constrained to actual `Extension.ExtensionObject.EHAlign`/`EVAlign`; `hMargin` and
`vMargin` are FloatProperty flags2; `minLines` and `MaxLines` are IntProperty
flags2. These actual native declarations, owning class, dimensions and enum
identities must be validated rather than synthesizing same-named fields.

The original declarations of TextLog natives are AddLog1570 (String parameter
flags`0x400080`, Color struct parameter flags`0x80`), ClearLog1571 (no arguments),
SetTextTimeout1572 (Float flags`0x80`) and PauseLog1573 (Bool flags`0x80`). They
remain separate services; admitting construction does not implement them.

## Native constructor and Init order

Original `Extension.dll` image base is `0x10000000`.

1. `InternalConstructor10014830` invokes protected default constructor10014840.
   The constructor zeros the three native Lines array words at+178/+17C/+180,
   independently of copied CDO raw array bytes. The parent-taking constructor
   10047480 likewise calls Text's parent-taking constructor10045AD0, constructs
   the empty native array and installs the TextLog vtable1005B63C. It is not the
   virtual Init routine.
2. Virtual `TextLog.Init100474B0` calls `TextWindow.Init10045AF0` first. That runs
   genuine Window.Init/parent notifications, then the existing Text scalar/text/
   accelerator initialization. Native Lines must already be initialized and
   empty during those callbacks, not lazily fabricated after base Init.
3. TextLog's suffix writes HAlign0, then VAlign0, then textTimeout5.0f
   (`0x40A00000`). It does not leave the inherited Text alignments at1.
4. It destroys every current Lines entry's owned FString, sets native array
   count and capacity to0 and reallocates with element size20. This is a real
   clearing/freeing operation if a base callback populated the array. It is not
   merely a final empty-size assertion and does not call RegenerateText here.
5. It clears bit0 of the native flags word at+184 (`bTooTall`). It does not reset
   bit2 (`bPaused`), other flag bits or copied CDO paused state.
6. The ordinary existing NewChild lock/most-derived script InitWindow/optional
   show/reconfigure/unlock order follows. Exact TextLog uses inherited original
   `Extension.Window.InitWindow`, whose body is Return/Nothing. Script subclasses
   must still receive their actual most-derived callback.

`Lines` is owned native TArray storage, not an Actor list, raw pointer-shaped
script DynamicArray, serialized service handle or invented public IntArray. Each
retail element is20 bytes: a Float at+0, an owning FString at+4 (three Win32 words)
and full four-byte Color at+16. The constructor and destructor establish actual
ownership. TextLog.CleanUp100475B0 calls TextWindow.CleanUp10045C30; it does not
itself clear Lines. Destructor100145C0 destroys each entry's FString, removes
entries/frees native array storage, then continues inherited text/window cleanup.
Do not substitute an empty Cleanup handler for the full destruction lifecycle.

A portable representation must own strings independently, journal mutations and
before-images, charge actual vector/string capacities and staging peaks, and
destroy or swap owned storage without leaking on rejection/rollback/unload.
Lines contain no UObject pointers; do not add bogus GC roots for their contents.
Constructed-empty and uninitialized must be distinguishable. The existing
32MiB limit must not be raised to hide an eager per-object payload allocation.

## Inherited versus overridden virtual dispatch

Static vtable data at1005B63C (TextLog) and1005A458 (Text) confirms:

| Virtual slot | TextLog target | Consequence |
| --- | --- | --- |
| +54 | 100474B0 | real TextLog.Init |
| +58 | 100475B0 | real TextLog.CleanUp |
| +7C | 100465A0 in both tables | inherits TextWindow preferred sizing |
| +84 | 10003540 in both tables | inherits ordinary child-visibility callback |
| +90 | 10047910 versus Text10003600 | distinct TextLog.ConfigurationChanged |
| +D4 | 10047C30 versus Text10003BE0 | distinct TextLog.Tick |
| +D8 | 10047A10 versus Text10046930 | distinct TextLog.Draw |

Parent/child notification slots also match the inherited text/window table.
This permits specific authenticated inherited callback handling, not blanket
permission to use Window behavior for all TextLog virtuals. Bundled SurrealEngine
has its own TextLog preferred-size implementation; that is not the retail vtable
contract and must not replace the observed inherited TextWindow target.

## Immediate original HUD setters1556/1557/1558

After successful NewChild atPC110, authentic HUDLogDisplay.CreateControls calls:

1.1556 SetTextAlignments(0,0), atPC127;
2.1557 SetTextMargins(0.0,0.0), atPC143;
3.1477 SetFont(fontLog), atPC165;
4.1558 SetLines(MinLogLines,maxLogLines), atPC182.

These are actual subsequent dependencies, not proof of already implemented
portable setters. No1556/1557/1558 dispatch was found under `native` during this
audit. All six setter parameters are required, single-dimension flags`0x80`:
two Byte/enum, two Float, then two Int. Wrapper arguments are evaluated in source
order and trailing EndFunctionParms is consumed before the native method.

- **1556:** exec10047000 -> SetTextAlignments10046050. Two Byte low-byte result
  seeds are0. Method writes BOTH alignment bytes unconditionally, accepts the
  full byte range, and neither reconfigures nor invokes a script callback. Do
  not invent enum clamping, an equality shortcut or layout notifications.
- **1557:** exec100470A0 -> SetTextMargins10046070. Two full Float seeds are0.
  Compare current/new H first; only if equal compare V. If either differs, write
  BOTH raw new Float values, then invoke AskParentForReconfigure1004E060 once.
  If both compare equal, do nothing. The x87 test checks C3 alone: signed zeros
  compare equal and unordered NaN compares as equal for this branch. C++ `!=`
  would not reproduce the NaN case. There is no margin clamp/finite validation
  in the original method; do not silently substitute one.
- **1558:** exec10047130 -> SetLines10046120. Two full Int seeds are0. Clamp
  each signed argument to at least0, then replace minimum with
  `min(minimum,maximum)`. Compare these effective values to current fields. If
  either differs, write BOTH effective Ints, then AskParentForReconfigure once;
  otherwise no writes/callback. Constructor's -1 values are not preserved when
  the setter receives negative inputs. Bundled Surreal's unclamped SetLines is
  not the original behavior.

AskParentForReconfigure1004E060 sets Self's bNeedsReconfigure/bNeedsQuery first.
It walks current Self/parent local-visibility flags. Any hidden ancestor defers
layout without calling the all-visible parent negotiation/configuration path.
For an all-visible chain it negotiates the actual parent's virtual+88, may ask
the parent recursively, and/or reconfigures Self; a root can directly call its
virtual+90 ConfigurationChanged. Setter implementations must use the existing
audited hidden behavior and explicitly refuse unsupported visible work rather
than dropping these effects or always calling ConfigurationChanged immediately.

## TextLog.ConfigurationChanged and extent dependency

Original10047910 has a real native prefix and then an optional script callback:

1. GetGC1004DDC0 obtains the context at Window+134. If non-None, it calls
   GC.PushGC100278D0 and GC.SetGC10027760 with the context at Window+130. These
   are actual owned contexts, not a fabricated renderer Canvas.
2. If a GC was obtained, call GC.GetTextExtent10027BB0 with exact width
   `Window.Width - 2*hMargin`, two Float OUT temporaries and actual inherited
   Text (empty C string if FString has no characters). There is no max(0,width)
   clamp in this caller.
3. Compare `Window.Height - 2*vMargin` against returned text height using x87
   C0: less-than OR unordered sets bTooTall bit0; otherwise clears it. Preserve
   all other flag bits. No-GC skips this computation and preserves bTooTall.
4. ReleaseGC1004DE50 invokes GC.PopGC100279F0 when non-None.
5. Regardless of whether a GC existed, call ProcessScript1004DED0 for the actual
   ConfigurationChanged event, no parameters, checked=false. This does not call
   the inherited Text/Window native ConfigurationChanged first and must not
   duplicate the script callback.

PushGC can create/reuse a native GC stack node, copy context state and change
gcCount/stack/free/owner; SetGC copies actual context fields, including fonts,
styles, colors, clip, wrapping/metachar and spacing settings. Release must
restore the genuine stack protocol. These services are not currently supplied
by merely having constructor-created two GC objects.

GC.GetTextExtent10027BB0 is actual text measurement: normal/bold Font data,
line/word splitting10029D80, metachar decoding100294D0, glyph metrics10026880,
line heights and textVSpacing. It need not fabricate a Canvas, but must not
return a convenient zero extent or use a renderer-independent guessed width.
Full measurement and the GC stack protocol remain separate unimplemented work
unless a later implementation authenticates them explicitly.

TextLog inherits TextWindow.ParentRequestedPreferredSize100465A0. That path
also obtains/releases a GC, measures actual Text with wrapping, applies min/max
line height constraints and margins, and clamps only the final unspecified
width to MinWidth. Its empty-text/font-None/base-script branches are real
branches, not permission for a general empty-only preferred-size substitute.
The exact sizing body requires its own complete implementation/verification
when reached; this audit does not claim it implemented.

## Other native lifetime services remain distinct

ClearLog10047810 destroys all entries, empties native array storage, then calls
RegenerateText10047DC0. RegenerateText rebuilds inherited Text from native Lines,
inserting the original separator between entries, then asks for reconfiguration.
SetTextTimeout100478E0 is one raw Float store. PauseLog100478F0 assigns bit2 from
the native argument's low bit, preserving other flags.

AddLog100476A0 grows the array with original `count+32+floor(3*count/8)` capacity,
zeros the new20-byte entry, stores GetTickOffset1004FDA0 plus textTimeout, copies
the original FString and full Color, then RegenerateText. Tick10047C30 has a
distinct pause/expiry/delete/regeneration protocol. Draw10047A10 has a distinct
alignment/too-tall/color path. None is implemented by construction or covered by
an empty native Lines snapshot; implement/authenticate separately when required.

## Evidence and reproduction

Original files under `D:/Steam/steamapps/common/Deus Ex/System`, SHA256 verified
read-only on2026-10-27:

- Extension.dll: `93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C`
- Extension.u: `9984FAEDA55FB2100B1A15A37BFEB8CDB4878248B39AFF832002A799F1D787F7`
- DeusEx.u: `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480`

Existing durable evidence includes:

- `artifacts/world-native-startup-20261011/extension-exports-imports.log`: genuine
  decorated exports/RVAs for constructors, Init, destructor/CleanUp, native
  setters/wrappers, GetGC/ReleaseGC, GetTextExtent and later TextLog services.
- `artifacts/pawn-continuation-original-baseline-20261026.json`:45-file original
  data baseline used by the current verification phase.
- `artifacts/viewport-input-final-training-fire-20261027.log`: actual repeated
  reached boundary and rollback-only weapon evidence, not completed startup.
- `artifacts/hud-active-augs-textwindow-wrapper-audit-20261015.log`: earlier
  genuine inherited text-wrapper audit; do not substitute it for the specific
 1556-58/Init addresses audited here.

This audit used the repository's own data-only
`artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe` for
`DeusEx.HUDLogDisplay.CreateControls`, `.InitWindow`, `.StyleChanged`, the four
TextLog properties/natives, the six inherited fields, the1556-58 declarations
and `Extension.Window.InitWindow`. It used static GNU objdump only, e.g.:

```powershell
& 'C:/ProgramData/mingw64/mingw64/bin/objdump.exe' -d -M intel `
  --start-address=0x10047480 --stop-address=0x100475b0 `
  'D:/Steam/steamapps/common/Deus Ex/System/Extension.dll'
```

Other audited ranges:10014830..10014890 (default constructor),
100145C0..10014700 (owned Lines destruction),10046050..10046194 (setters),
10047000..1004719D (wrappers),10047910..100479D3 (ConfigurationChanged),
100465A0..10046843 (inherited preferred size),1004DDC0..1004DED0 (Get/ReleaseGC),
10027760..10027AC0 (copy/stack),10027BB0..10027D27 (extent),
1004E060..1004E223 (reconfigure), and static data at1005B63C/1005A458 (vtables).
No disassembled commercial code was called. No source/build inputs were edited
and no build, test, APK packaging or device installation was run for this audit.

## Next bounded implementation

After the current frozen Input batch is verified/committed: implement actual
TextLog constructor/owned Lines lifecycle and native Init suffix, plus the three
immediate authenticated Text setters and specific inherited callbacks. Verify
CDO-field preservation, native-array initialization-before-callbacks, populated
suffix clearing, exact setter equality/clamping/write/callback ordering, whole-
root rollback and memory admission. Keep unaudited ConfigurationChanged/extent,
Tick/Draw/log services as explicit boundaries until genuinely implemented.
Then repeat the actual first-Training startup/weapon diagnostic to establish the
next reached service. Do not assume the later SET/Input command is the next
boundary, or package a partially initialized player as a finished crate fix.
