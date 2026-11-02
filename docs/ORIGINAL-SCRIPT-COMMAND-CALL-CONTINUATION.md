# Missing original script-command call service (static audit2026-11-02)

Latest implementation: see ORIGINAL-SCRIPT-COMMAND-CALL-IMPLEMENTATION.md. The
matched scalar/String command packet and actual ProcessEvent call now exist,
and actual Training ParseLeftClick|Fire/trigger/reset plus Console TimeDemo Bool
packet/copyback gates pass. The blanket missing-call refusal below is historical
audit context. Object/Class textual imports, aggregate/fixed-array layouts,
localized required-error output and some native overrides still explicitly
refuse. Complete menu interaction, initialized Input/ReadInput and playable
startup are not claimed. No new APK.

Read-only independent source/runtime audit during physical-input compiler
freeze. No source provider is implemented by this document. Original commercial
modules were inspected AS DATA ONLY, never loaded/executed.

## Real default-button blocker

portable_viewport_exec_host.inl ScriptConsoleOperations::Match currently throws
`Original ScriptConsoleExec command parameter/call service is not implemented`
when the prefix finds a genuine Exec function. Thus default LeftMouse
ParseLeftClick|Fire, RightMouse ParseRightClick and I ShowInventoryWindow cannot
be called through actual bindings yet. Do not replace bindings, swallow the
refusal or claim trigger/A/Menu gameplay from the generated mapper tests.

Owned signatures: DeusExPlayer.ParseLeftClick Exec0x2c2/zero input parameters;
ParseRightClick Exec0x2c2/zero inputs (its children are locals);
ShowInventoryWindow Exec0x202/zero inputs; Engine.PlayerPawn.Fire Exec0x202 with
one optional Float F flags0x90. The runtime-selected override stays authoritative.

## Source tail, not guessed command splitting

Core ScriptConsoleExec10150860..10150B02 requires this selected tail:

1. Retain the already-selected real UFunction and zero its actual parameter
   storage. Zero storage is not CDO/class defaults or VM omission markers.
2. Walk actual property Children/Next order WHILE(flags&0x480)==0x80. The FIRST
   non-input/Return property ends parsing; filtering all flags80 fields is wrong.
3. Only the first parameter may receive compatible non-null Executor injection,
   if it is actually ObjectProperty and the Executor satisfies its referenced
   Class. That branch consumes no text.
4. Otherwise call original ParseNext, then the SELECTED property ImportText with
   flags1 and the actual remaining UTF16 CString pointer.
5. Required import failure logs localized BadProperty, does NOT call ProcessEvent,
   but still returns handled=true after selected parameter cleanup. Optional
   failure stops further parsing, then calls with the zero-filled remainder.
6. Invoke the receiver's actual ProcessEvent using that RETAINED function and
   null result, then selected DestroyValue for every input parameter, including
   failed/unparsed input storage. Exceptions also require genuine cleanup.

ParseNext10147B50..10147B9E skips spaces, tabs, CR/LF and semicolon-to-EOL
comments. Core SET's whitespace-only helper is not sufficient. Required failure
uses LocalizeError("BadProperty","Core",null)101023B5 and ordinary Logf
1010280B, not an explicit ExecWarning-name overload. Owned Core.int:62 says
`'%s': Bad or missing property '%s'` (function name, then property name).

Float ImportText export906 ->10166940..101669B6 always writes appAtof's Float
and returns a cursor, including EMPTY text ->Float0. It scans until NUL/comma/
')'/CR/LF, NOT whitespace. Float appAtof101235C0 uses appToAnsi10149FF0's1023-unit
profile and CRT atof; InputAxisCommandDetail::Atof is the existing exact ordinary
decimal basis, retaining its explicitly unsupported special/hex CRT domains.
String flags1 copies the whole remaining CString and returns the ORIGINAL
cursor. Generic token splitting or optional-parameter Nothing would change this.

## Runtime integration/ownership requirements

Thread remaining UTF16 command, caller Execution and actual output collector
through ScriptConsoleOperations/ScriptConsoleCommandPrefix and Console/Player/
Game routes; never recursively enter the public Execute API/new root.
FunctionIdentity/FunctionFor retain selection and actual ordered fields;
Zero/Describe supply typed zero, not class-default values. NativeConfigImport
contains actual Int/Bool/Name/String/Class import providers, but its Class-specific
constraints cannot silently be applied to an ordinary ObjectProperty.

OriginalNativeActorCallback in portable_actor_native_event.inl supplies relevant
ProcessEvent gates but cannot be called unchanged: it reselects by name and
requires parameter flags80, rejecting Fire's genuine optional90. Extract a
retained-function/packet-capable adapter. Authenticate actual Actor +10/+2C
through OriginalAnimationActorVirtuals/OriginalAnimationPendingKill. Preserve
Actor begun-play, scriptability/probe, pending-kill, native-index and RPC gates.
Console needs its separate Core ProcessEvent profile. Bare VM CallFunction is
not equivalent: original ProcessEvent suppresses nonzero nativeIndex, among
other conditions. Do not fake these by a direct ungated call.

Core UProperty.DestroyValue10165590 is a genuine scalar no-op. Dynamic String/
Struct/Array teardown is NOT. DestroyPropertyValue in portable_object_properties
contains selected dynamic providers, but its current actor-field/constructor-
link address model cannot be reused with a fabricated actor. Refactor selected
provider logic for an owned parameter packet. Any OUT reference backing must
survive enclosing rollback; never retain a stack-local cell in undo closures.
All packet storage, parser/import work and teardown share the actual root's
budgets/lifetimes. No fake provider, budget raise or no-op command call.

## Honest additional digital diagnostic options

A negative actual trigger sample can select original ParseLeftClick and prove
the exact missing-call error plus complete native/source/script rollback.
A positive ROOT-CONSUMPTION-ONLY variant can explicitly set diagnostic actual
Root.handleKeyboardRef=1 and actual keyDownMap[Ctrl17]=1, then supply tracked
left Menu. Owned DeusExRootWindow.VirtualKeyPressed offsets89..118 genuinely
returns false under Ctrl/Alt/Shift, while Root.Process consumes due to the real
grab. This can check physical Menu held/Input.Keys[I]=0/Root key I=1, reset
release/repress and tracking-loss release without replacing any binding/event.
Such explicit diagnostic Root state does NOT prove an inventory UI really
grabbed focus or that original menus/button commands work. These variants are
not yet implemented or passing owned gates.
