# Original Input action commands (2026-11-01)

Read-only static source audit for the next native input service. No compiler
input is changed, no original commercial DLL/EXE is loaded or executed, and
no action-command/runtime/Quest/startup/APK acceptance is claimed here.

Implementation follow-up2026-11-02: the typed BUTTON/PULSE/TOGGLE coordinator
passes21663 checks/63 refusals; typed AXIS passes1173/233. Actual native
FindButtonName/FindAxisName and qualified property-journal binding now share
the real inherited metadata cache, including appended Core.Const authority.
The genuine Training Init-prefix executes19 commands/10 actual player stores,
then verifies full rollback at the unchanged missing Quest event boundary.
Public constructor-only authentication is unchanged; this is not completed
Input.Init, ReadInput or physical controller delivery. Full historical CRT
numeric domains remain incomplete. See ORIGINAL-INPUT-RUNTIME-ACTIONS.md,
ORIGINAL-INPUT-AXIS.md and STATUS.md for the exact final frozen gates.

Owned SHA256:

- Engine.dll: `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
- Core.dll: `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

The external MinGW objdump reads these PE files as data. Addresses below are
original preferred PE32 VAs and source offsets, not portable object addresses.

## Entry point and actual branch order

Engine UInput.Exec exported thunk1030178F jumps10393E10. Its receiver is the
FExec subobject at full Input+28: ESI+E78 is actual Input.Viewport+EA0,
ESI+E80 is Input.Action+EA8, and ESI+E84 is Input.Delta+EAC. Do not copy these
offsets into portable objects or confuse the subobject with the whole UObject.

The body selects, in this order:

1. BUTTON literal1047F48C, ParseCommand at10393E4C.
2. PULSE1047F47C, ParseCommand10393EE5.
3. TOGGLE1047F43C, ParseCommand10393F60.
4. AXIS1047F400, ParseCommand10393FDB.
5. KEYNAME1047F3B4.
6. KEYBINDING1047F398.
7. The previously audited alias/recursion-guard tail10394132..1039422A.

There is NO COUNT action branch in this owned GOTY UInput.Exec. Do not add one
from another Unreal release or an assumed command list. ParseCommand retains
its current cursor adjustments between the branch attempts; original leading
space/tab skip and ASCII letter/digit suffix rules still apply.

## Shared action selection and failure semantics

Each recognized action first reads actual Input.Viewport, then Viewport.Actor.
If Actor is None it takes that action's bad-command output path WITHOUT parsing
the property-name token. This is a genuine Actor=None branch, not permission
to treat an unavailable/revoked native Viewport as absent. The binary dereferences
Viewport; a portable missing authority must refuse before that unsafe access.

With a current Actor, it calls buffer ParseToken256 with UseEscape0 on the
remaining command. This is the existing audited CoreSetCommand.ParseToken256,
not ImportText.ReadToken. Failure takes the bad-command output path. Quoted
and unquoted capacity/cursor behavior must remain the original token behavior.

After ParseToken succeeds, it REREADS actual Input.Viewport.Actor and calls
the selected actual virtual FindButtonName(+74) or FindAxisName(+78) on the
whole Input UObject. Do not bind the property to the Actor captured before
token parsing. Actual property addresses remain selected after lookup; later
Action/Delta reads do not silently reselect the Actor or same-name property.

BUTTON/PULSE invalid Actor, token or property emits the exact CString
`Bad Button command` (1047F44C) using FOutputDevice.Log via Engine IAT10599DD4.
TOGGLE emits `Bad Toggle command` (1047F40C) through the same Log route.
AXIS emits `Bad Axis command` (1047F3C8) using FOutputDevice.Logf via10599A68,
with no format arguments. The output device is the ACTUAL supplied command
output, not an invented silent sink. Preserve the admitted output-device
profile; do not invent newline behavior or turn a warning into a failure.

Every recognized action reaches handled=true at103940D0, including a bad
command, a genuine property miss, or an Action that selects no store. It does
NOT fall through to an identically named alias, KEY query or later viewport
provider. Missing authority/provider is a refusal, not this genuine handled
bad-command result.

## Button operations

The selected address is a genuine INPUT-flagged ByteProperty, one byte at
index0. There is no Bool packing, integer coercion, saturation, counter or
array traversal in these branches.

- BUTTON10393EAF..10393ED7: read current Action after successful lookup.
  Press1 writes byte1 unconditionally. Release3 reads the selected current byte
  and writes0 only if it was nonzero. Every other Action performs no store.
  Thus Release3 on byte0 performs NO redundant VM write; Release3 also clears
  byte128/255, not only byte1.
- PULSE10393F38..10393F48: current Action1 writes byte1 unconditionally;
  all other actions perform no store. Do not add a release clear here.
- TOGGLE10393FB3..10393FC3: current Action1 XORs byte0x80; other actions
  perform no store. This is NOT logical inversion or XOR1. Lower seven bits
  survive. Byte1 becomes129; byte128 becomes0; byte255 becomes127.

## Exact property lookup

FindButtonName export103023EC jumps10393650..1039384F.
FindAxisName export10302DFB jumps10393920..10393B25.
Both first assert actual Input.Viewport nonnull, then their supplied Actor
nonnull. The name is constructed using actual FNAME_Find0, never interned.
FName None immediately returns nullptr without reading Actor.Class or cache.

For a non-None name, each captures that actual Actor.Class and uses the native
property cache key uint64(Class.ObjectIndex)*0x100+0x1F. Both services use the
SAME key/cache domain as original ReadInput. Cache contents are a count followed
by actual UProperty pointers, not copies of field names or values.

On cache miss they walk inherited actual UProperty declarations in their real
Children/Next/Super order, first counting flags bit4, then separately traversing
and storing those actual declarations. The iterator thunk10302798 jumps to
10392830, which tests actual Core.Property ancestry, skips non-properties and
advances through current Next then actual Super fields. Cache preparation is
not a Byte-only/Float-only prefilter; it includes ALL flags4 UProperties.

The subsequent cache scan compares actual FName first and actual property
Class ancestry second. FindButtonName selects ByteProperty imported at
1059A010; FindAxisName selects FloatProperty imported at1059A018. A same-name
entry of the wrong type is skipped, not treated as a terminating shadow. The
first matching name AND type wins. The cache lock is released before returning
Actor+selectedProperty.Offset. A miss returns nullptr. No ArrayDim loop exists.

Portable authority must retain exact actual Class/declaration/lifetime/name
and field ownership, including the newly registered Core.ByteProperty.
Unresolved property Classes cannot silently become ordinary nonmatches.
Do not select PropertyNamed's nearest-name shadow and then type-check it:
that changes the original skip-wrong-type search. A future owning cache must
retain real metadata roots/generation and byte accounting, not borrow a raw PE
key or let retired classes reuse cached declarations.

## AXIS arithmetic and SPEED parsing

After current Actor/token/FloatProperty selection, AXIS initializes Speed to
Float1 at10394045, then calls actual Core.Parse(Stream,"SPEED=",Float&) via
Engine IAT1059A01C. The Stream is the remaining command AFTER the property
token. The boolean return is ignored: absent SPEED preserves1; present parsed
value replaces it even when it converts to0. This parse runs BEFORE the
Action comparison, even when that Action later selects no axis write.

Current Action4 (Axis) uses, in original x87 sequence10394060..10394071:

`FLD FloatDelta; FMUL FloatSpeed; FMUL Double0.01; FADD FloatAxis; FSTP FloatAxis`.

Double0.01 at10429858 has bits`3F847AE147AE147B`. Current Action2 (Hold)
1039407A..10394085 omits the Double0.01 multiplication:

`FLD FloatDelta; FMUL FloatSpeed; FADD FloatAxis; FSTP FloatAxis`.

Other Action values select no store and still return handled=true. There is
no clamp, saturation, dead zone, normalization or finite-only input policy.
Do not round Delta*Speed to Float before the remaining operations or fuse
multiply/add. Use the established masked nearest-even PC64 arithmetic profile,
preserving load/NaN/signed-zero/overflow behavior, with one final Float store.

The selected Core.Parse overload is export101016CC ->10147670..101476F1.
It calls appStrfind(Stream,Match), and returnsfalse without a write on no match.
On match, advance by appStrlen(Match), call appAtof and store Float to the output;
returntrue. It is NOT a token-based option parser or strict number recognizer.
appStrfind export1010268F ->10145CA0..10145D7B uses ASCII-uppercase first-unit
matching, `_wcsnicmp` for the remaining units, and only a PRECEDING ASCII
letter/digit exclusion. Punctuation/underscore before SPEED= is allowed.
The existing CommandLineParamDetail::Find can supply this proven search when
called from index0 with a fresh previousAlnum=false and the same shared work.
Do not apply ParseParam's separate leading-dash/slash requirement.

appAtof export10103332 ->101235C0 calls appToAnsi then imported MSVCRT.atof.
appToAnsi export101024FA ->10149FF0..1014A07C copies at most1023 UTF16 units:
each unit<256 becomes its low byte; each unit>=256 becomes0x7F, then NUL.
This is neither UTF8 conversion nor arbitrary Unicode numeric parsing.
The CRT result is returned on x87 then the Parse overload stores Float. A
present malformed ordinary ASCII number follows atof's0-result behavior, not
std::stof's exception. A concrete adapter must establish its CRT ordinary
C-locale numeric/overflow/NaN/exponent profile or explicitly refuse unproven
domains; silently choosing a stricter parser or clamping is not source fidelity.

## Minimal faithful runtime integration

The current constructor-only public ActualNativeInputInstanceValid and query
snapshot must NOT be globally weakened or marked initialized to exercise these
primitives. A real action service needs the existing same-root native Input
COW payload, actual viewport/pawn reciprocal binding and an authenticated
native viewport back-pointer established by the real Init prefix. The existing
rollback-only first-Training reset-prefix diagnostic reaches that authority,
but does not complete Quest reconciliation or publish initialized=true.

An explicit action primitive diagnostic may extend that actual same-root prefix
under a private lexical lease, set stated Action/Delta test inputs in the COW
instance, select real inherited metadata as above and journal qualified Pawn
stores using execution.WriteReference. It must retain exact output, work,
simultaneous scratch and all beforeimages; check complete Input/Actor/state/
save/GC/lookup/revision rollback after every failure prefix. It must not fake
Init/Root dispatch/GIsRunning/Quest events or commit a partial startup.

Operational integration still requires typed action parsing/arithmetic,
ExecInputCommands live routing, Root.Process callbacks, Quest reset/release
reconciliation and ReadInput. Parsed helpers or private primitive diagnostics
cannot satisfy these missing runtime providers by supplying test-only callbacks.
