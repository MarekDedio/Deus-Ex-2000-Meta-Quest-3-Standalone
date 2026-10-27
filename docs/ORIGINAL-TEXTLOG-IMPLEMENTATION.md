# Original TextLog construction and text setters (2026-10-27)

Verified implementation checkpoint. The genuine source contract and
original hashes/addresses are in ORIGINAL-TEXTLOG-STARTUP.md. First Training is
the only integration map. This is not completed player/HUD startup, live crate
gameplay or an APK acceptance claim.

## Implemented ownership and initialization

Native Lines live in the existing lazy native Window payload, never in copied
Win32 pointer-shaped DynamicArray bytes. Each record owns its Float expiry,
FString bytes and all four Color bytes. Initialized-empty is distinct from
unconstructed. The array is constructed before base Window.Init and parent
notifications. The inherited Text initialization then precedes the original
TextLog suffix: alignments0/0, timeout5, real destruction/freeing of current
Lines and capacity, and bTooTall=false. bPaused is not overwritten.

NativeTextLogStorage deep-copies actual vector and string capacities as well as
data; this matters for admission and exact rollback. Its copy refuses a bundled
library that cannot reproduce those capacities. Raw Float words, NUL/UTF8 text,
Color bytes, independent ownership, move/swap and freeing are separately tested.
Live state and before-images charge the owned capacities; rollback swaps owning
Window payloads without allocation. No retained-memory cap or save format changes.
Lines have no UObject references and add no GC roots or Actor births.

The actual private Lines declaration and scalar declarations are authenticated.
Generic script-value reads and host stores refuse Lines instead of exposing an
invented public script array. Detached diagnostics expose copied records and
initialization/capacity, not pointers or future query authority.

Specific genuine inherited optional events, parent/child notifications and
ordinary child-visibility dispatch admit TextLog. Its distinct ConfigurationChanged
and inherited TextWindow preferred-size path remain explicit unsupported services;
they are not aliased to plain Window behavior. ToggleChanged propagation through
a TextLog parent also remains explicitly unsupported; the Training log has no
toggle children. Tick/Draw/AddLog/ClearLog and expiry/regeneration behavior are
not supplied by admitting construction. The distinct timeout/pause scalar
methods implemented later in this batch are described below.

## Original text setters1556/1557/1558

The runtime authenticates actual authored declarations, required parameter types,
flags/dimensions and alignment Enum identities, then uses the original caller's
VM/root transaction. All argument seeds are their original scalar zeros.

- 1556 writes both raw Byte alignments unconditionally, without enum clamping,
  equality shortcut or layout callback.
- 1557 compares H first and V only when H compares equal. The x87 C3 decision
  treats equality and unordered alike, including signed zeros and NaNs. A change
  stores both raw Float values, then makes one genuine reconfiguration request.
- 1558 uses signed nonnegative clamps, then min<=max, compares effective values
  in original order, stores both on change and requests layout once.

The adapter propagates the existing real hidden-parent layout behavior or a
visible-path unsupported exception. It never swallows layout failure. Work/write
and rollback budgets remain shared with the caller. Host FPU exception flags are
not modeled by the C3 branch helper; its claim is the audited branch/store result.

## Bounded isolated diagnostic

ProbePortableNativeTextLogWindow constructs an explicit genuine BASE Root and
calls the original NewChild declaration for TextLog through the same VM native
ingress as the game. It is not a substitute configured DeusExRootWindow. An
observation-only scope records Lines immediately after native construction and
before base callbacks. The entire diagnostic always rolls back.

The explicit fixture populates native Lines and sets paused/too-tall/timeout,
then exercises the actual inherited Text-initialization/TextLog suffix. It does
not run a second base Window.Init or claim callback-populated gameplay AddLog.
Layout flags are explicitly reset as fixture state before setter observations,
not attributed to TextLog.Init. The twelve detached stages inspect original
setter ingress, data, freeing, equality, clamping, no-result seeds and writes.
PrivateWrite intentionally calls the real host reference store guard directly:
VM WriteReference would first encounter the separate private read denial.

Wrong receiver/type/arity, early/late write/native-work/retained-byte limits and
an explicit final failure must restore save bytes, source inputs, GC/lookup,
root ownership and name counters. No calls are made after a poisoned HUD failure.

## Verification

Standalone native storage:580 checks/1 refusal pass.
Standalone original text setters:467200 checks/19 refusal controls pass.
These are owning-storage and algorithm tests, not successful original startup.

Initial target desktop/strict ARM64/Android-native builds pass. Actual discovery
advanced first to SetTextColor1476 and then to SetTextTimeout1572, preserving
the original two-crowbar sequence and whole-root undo at240102 instructions/
27890 writes. These were unsupported-boundary discoveries, not completed startup.
An extra dynamic-actor/explicit-fire-pose command combination was invalid for its
distance fixture; that separate failed invocation is not accepted evidence.

The colour diagnostic initially tried to copy those added RGBA/Plane fields for
every Window/GC and hit its independent4MiB snapshot bound. It now copies only
actual TextLog and its own contexts, retaining the bound rather than raising it.
Timeout/pause scalar implementation and narrower diagnostics pass their target
desktop/Android-native builds. Exact HUD boundary assertions were updated only
after the final replacement boundary and pending graph were observed.

Review found missing ordinary-input snapshot registration for the new native
indices. The VM now retains argument1's Step value before argument2 can mutate
its source. Mixed direct/resolved bytecode alias tests pass174 checks/12 refusal
controls, including source-frame/native-receiver separation, real OUT mutation,
host mutation of single-input sources and late whole-root rollback. The full VM
suite passes696389 checks/912 refusals. Earlier detached-value probes alone did
not prove this.

The final target desktop build, Android-native link and strict ARM64 check pass.
Rebuilt owned HUD discovery now reaches HUDLogDisplay.ParentRequestedPreferredSize
PC129/opcode101,165 objects/11767 writes, with a complete detached snapshot and
the explicit unsupported TextWindow preferred-size virtual. The final original
two-Fire diagnostic reaches that same boundary at240624 instructions/28196 writes,
retaining48 observations,two notifies,four traces,two crate changes,nine births
and complete undo. Its exact-boundary control exits0; this is not successful
startup. All45 original data files still match length/mtime/SHA256 baselines.
The rebuilt owned HUD suite passes9128 controls against that exact165-object/
11767-write prefix, including msgLog/winLog/next-child edges, initialization
locks, actual player timeout and paused CDO state, plus its existing lifecycle,
rollback, GC and replacement controls.

The final resumed full desktop build exits0. Its registered CTest run exits0:
263 registered,221 executed/passed,42 explicit owned-data skips,zero failures,
266.93 seconds. Separate rebuilt owned-data runs all exit0: crate415/5,
deferred429/21,animation588/24,FastTrace252/0,native Input classes10413/38,
viewport Input10310/20,Training route1232 checks and first-Training materials
(1308 actors,227 selected materials,zero null materials,74 decoded textures,
one procedural/unsupported texture). These diagnostics do not establish live
VR melee, surviving collectible loot or gravity acceptance.

Final rebuilt original two-Fire repeat2 exits0 at240624 instructions/28196
writes on both attempts, retaining the exact preferred-size refusal and all
weapon observations above. Original door repeat2 also exits0 at239536/19432,
retaining the exact missing-ReadInput refusal and both original56-unit panel
moves/two endpoints. Both verify whole-root undo,41 source fingerprints and
immutable Model cold/warm/replacement lifetime. Expected refusal is not
successful startup. No other integration map was loaded.

Final audit confirms all45 original files and12 frozen compiler inputs match
their length/mtime/SHA256 baselines, with all compiler/test/probe jobs terminal.
Authoritative final logs are artifacts/textlog-resumed-desktop-all-20261027.log,
textlog-resumed-ctest-20261027.log, textlog-resumed-training-fire-20261027.log,
textlog-resumed-training-doors-20261027.log and the eight
textlog-resumed-owned-<executable>-20261027.log files. Targeted HUD/VM/storage/
setter/Android-native/strict ARM64 logs retain the prefixes named above.
No new APK has been packaged or installed.

## Original SetTextColor1476 source evidence and implementation

After TextLog construction and1556-58, the actual owned HUD reached
`DeusEx.HUDLogDisplay.StyleChanged` atPC26/native1476. Original bytecode calls
the inherited `Extension.Window.SetTextColor` with the actual HUD colText.
The runtime now routes1475/1476 through the shared `WindowColorNative` adapter;
this is original native field/GC behavior, not a presentation-only label color.

`Extension.Window.SetTextColor` has native1476, flags`0x401`, one required
`NewColor` StructProperty, dimension1, flags`0x80`, actual `Core.Object.Color`.
Window.TextColor and Window.textPlane are dimension1 StructProperties flags2,
respectively constrained to actual Core.Object.Color and Core.Object.Plane.
The corresponding GC.TextColor and GC.textPlane declarations have flags0.
Window.wGc is dimension1 ObjectProperty flags0 referencing actual Extension.GC.

Static original Extension.dll evidence, using the SHA256 already recorded in
ORIGINAL-TEXTLOG-STARTUP.md:

- `execSetTextColor10053B30..10053B6C` evaluates one argument into an
  **uninitialized** canonical four-byte Color temporary, consumes the original
  EndFunctionParms, then calls Window.SetTextColor1004D9E0. It does not initialize
  black/white/zero or retain a caller's stale Color on an unwritten result.
- `Window.SetTextColor1004D9E0..1004DA99` unconditionally copies all four RGBA
  bytes to Window.TextColor at+3D. It then writes Window.textPlane at+44: each
  RGB byte is multiplied by the original binary64 reciprocal at1005E3C0
  (`0x3F70101010101010`) before a binary32 store; W is positive zero.
- Only after the Window stores, if the actual wGc at+130 is non-None, it calls
  `GC.SetTextColor10027580..10027604`. That copies allRGBA to GC.TextColor at+68
  and performs the same RGB/W stores to GC.textPlane at+6C.
- There is no equality shortcut, gGc update, callback, layout request, font,
  background style, translucency flag or GC stack operation in this service.
  A None drawing GC is a valid branch, not permission to fabricate one.

The adapter authenticates these actual fields and uses the caller Execution's
native writes. Its canonical argument-layout hook extends the existing1475
uninitialized CoreColor4 protocol to1476; it deliberately does not register an
initialized argument seed. Full canonical Color writes and complete four-byte
Context clearing can materialize a valid value; malformed channels, missing
or unwritten/partial native destination bytes must not turn into guessed color.
Raw RGBA alpha is preserved even though Plane normalization uses RGB only.

The isolated TextLog fixture calls the authentic1476 declaration through
`execution.CallFunction`, not a direct helper-only substitute. Its source checks
four original Window/wGc Color+Plane writes even for equal values, two Window
writes when wGc is explicitly None, and unchanged independent gGc. A256-case
channel sweep checks exact Float bits, allRGBA channels and positive-zero W;
layout flags, alignment/margins/lines, fonts, background style and text
translucency remain unchanged. Negative canonical/no-result and outer-root
budget/rollback cases are retained. These are verification controls, not a claim
of successful original HUD/Training startup or headset acceptance; the actual
runs stop at the distinct preferred-size dependency described above.

Durable export/RVA evidence is
`artifacts/world-native-startup-20261011/extension-exports-imports.log`, including
Window/GC SetTextColor and the Window exec wrapper. The repository's own
data-only script_bytecode_inspect reads the genuine .u declarations and
HUDLogDisplay.StyleChanged. Static GNU objdump reproduces the three ranges above;
no commercial DLL was executed or loaded as code. Native1572 timeout/log startup
and later dependencies remain independent from this color service.

## Original timeout1572 and paused-state1573 scalar services

After1476, actual owned HUD discovery next reached
`DeusEx.HUDLogDisplay.SetLogTimeout` atPC20/native1572. Original bytecode first
assigns its real HUDLogDisplay.displayTime, then calls the actual winLog's
SetTextTimeout. A later refusal must undo both operations in the outer root;
passing only the timeout method is not completed startup.

The implementation supplies the two authentic bounded scalar services, not the
remaining log regeneration, expiry Tick, Draw or text-extent services:

- `Extension.TextLogWindow.SetTextTimeout`, native1572, function flags`0x401`,
  one required newTimeout FloatProperty, dimension1, flags`0x80`, no return.
  Target textTimeout is an actual FloatProperty dimension1 flags0.
- `Extension.TextLogWindow.PauseLog`, native1573, function flags`0x401`, one
  required bNewPauseState BoolProperty, dimension1, flags`0x80`, no return.
  Target bPaused is an actual BoolProperty dimension1 flags0.

Genuine original static evidence:

- `execSetTextTimeout100481A0..100481E6` initializes its full Float temporary to
  positive zero, evaluates one argument, consumes EndFunctionParms, then calls
  `SetTextTimeout100478E0..100478ED`. The method unconditionally copies the raw
  DWORD to+174. No comparison, finite check, negative clamp, callback or update
  of any already-existing Lines expiry appears in the method.
- `execPauseLog10048210..10048265` initializes its full native INT temporary to0,
  evaluates one argument, consumes EndFunctionParms and directly assigns bit2
  of+184 from argument low bit while preserving the remaining bits. The
  separately exported `PauseLog100478F0..1004790E` performs the same bit update.
  The wrapper does not need to call that export to have the same behavior.
- Both always store, even for equal values. Neither touches bTooTall, timeout
  versus pause's other field, Lines storage/content/capacity, inherited Text,
  Window/GC color, layout, fonts, style, GC stack, Tick or log regeneration.

`TextLogScalarDeclaration` authenticates actual source/function/input schema.
`TextLogScalarNative` requires a genuine constructed TextLog with initialized
native Lines and exact argument Kind/count, then makes the one journaled field
write in the caller Execution. The separate argument-seed branch supplies the
original positive-zero Float/false Bool; these are initialized temporaries, not
the uninitialized Color protocol used by1476. A missing required argument still
refuses at the VM schema boundary. The Bool native low-bit observation does not
authorize accepting arbitrary wrong-typed VM Integers as a Bool argument.

The fixture contains raw Float-word controls for positive/negative zero,
negative values, both infinities, quiet/signaling NaNs and duplicate stores;
it uses bit comparisons instead of NaN-sensitive ordinary equality. Pause
controls include duplicate false/true values, preserved too-tall state and
no-result false seeding. The owned fixture and exact-boundary Training control
pass as described above; neither establishes completed startup or live gameplay.
Scalar VM Float transport/copy is distinct from finite geometry; save codec14's
existing ScalarF32 bit transport is the relevant serialization path, not F32's
finite-only geometry path. Native Window/service state is not newly made into
an Actor save manifest by adding these transient field methods.

Export/RVA provenance remains
`artifacts/world-native-startup-20261011/extension-exports-imports.log`; original
Extension.dll/.u hashes are in ORIGINAL-TEXTLOG-STARTUP.md. The repository-owned
data-only inspector authenticates both declarations/properties and actual
HUDLogDisplay.SetLogTimeout. Static objdump ranges above do not execute/load
commercial code. Native source was frozen while this documentation section was
added; no build/test was run by the author of this evidence section.
