# Remaining configured Input and InputExt instance contract (2026-10-27)

Read-only owned evidence for the next input phase, not implemented constructor,
configuration, FExec or ReadInput acceptance. The preceding native schema/default
phase remains explicitly pre-config. First Training is the integration target;
no account or character-creation screen is needed.

Owned Core/Extension hashes are recorded in ORIGINAL-PLAYER-INPUT-CONSOLE.md.
Only static PE/package inspection was used; never load or execute commercial
DLLs/EXEs. Addresses are original preferred PE32 VAs, not host memory offsets.

## Propagate1 means parent-first writes to the same receiver

Core LoadConfig101023DD ->10150D60..10151384 omits parent recursion only when
Propagate is zero. At10150DAC..10150DC7, nonzero Propagate recursively processes
the actual supplied Class.Super before the current Class, retaining the SAME
destination receiver and supplied filename. The Config-class gate occurs first.

InputExt1E ->Input1E ->Subsystem112 therefore yields Subsystem's gated return,
then the Engine.Input section, then Extension.InputExt, on the same receiver.
Each class pass iterates inherited properties too. Ordinary Config4000 fields
use the current supplied Class's section; GlobalConfig40000 uses the property's
actual declaring Class10150E7E..10150E98. No descendant/object fanout or implicit
second default-object mutation was established inside LoadConfig itself.

FilenameNone selects receiver.Class.ConfigName (User here). This name does not
prove physical filename aliases, fallback/layering or packed-launcher GConfig
backend behavior. Use an explicit read-only User.ini binding in the embedding.
The owned User.ini contains Engine.Input, not Extension.InputExt, so treating
propagation as no recursion would discard the original bindings and aliases.

Fixed fields iterate actual ArrayDim. Dimensions greater than1 use %s[%i] keys,
so Aliases[0..39] import separately. Successful1024-unit GetString calls invoke
actual element ImportText with flags0. Missing values preserve storage; the
native ignores ImportText's return and can retain prefix writes on a malformed
struct. The typed import protocols already implement these cursor/write rules;
the new real native graph must supply traversal/Port/storage authority. Never
replace the40 fixed records with a dictionary or silently use an empty table.

StaticInitInput's final CDO LoadConfig(1,None,None) must be implemented explicitly.
Parent config must precede InputExt default copying. Preparing fields and zero
defaults does not itself apply configuration.

## Actual InputExt constructor order

Extension1002CFB0..1002D073 first calls UInput's constructor, then installs its
own primary/FExec virtual implementations. Outside editor mode it validates
actual CDO size and calls default LoadConfig before instance LoadConfig. In
editor mode it reloads only the instance. Both finish with the real Extension
name bootstrap. This is separate from inherited class static-callback equality.

UInput's actual ctor constructs its40 Alias records and255 bindings, installs
FExec and resolves actual Actor.EInputKey, not Console's same-named enum. Native
Viewport remains a separately owned Init edge. Actual instances require the
configured Class/default graph, Transient Outer, viewport registry/lifetime and
two-way GC edges, typed accounting/journaling and explicit revocation behavior.
Do not lazily manufacture them from a ConsoleCommand or a read query.

Allocation requests flags0, but actual Input class1E includes CLASS_Transient8.
Core10157E9E..10157EAD therefore ORs RF_Transient4000 into the resulting object
flags, as the existing native Window allocator already does. Validate actual
flags4000, not requested flags0; this still leaves RF_Transactional bit1 clear.
Keep transient Input/session identities out of serialized object references,
not merely out of map exports. The existing generic ScriptSaveSchema::ObjectClass
accepts indexed non-map UObjects too broadly for these new transient services;
add an explicit native-input/session exclusion, never recreate them from saves.

Preserve copied default values during construction: the Alias constructor
10303AFD ->10394F40 returns this without payload writes. The255 binding loop
10393493..103934B3 calls FStringNoInit through IAT10599B88, Core101031CA
->10109130, likewise without storage writes. Do not zero the copied aliases or
bindings again in the constructor. Enum assignment103934C5..103934E8 and FExec
installation103934B8..103934BE are real distinct constructor effects.

Viewport construction owns its Input pointer, but native Input.Viewport is a
separate Init effect: UInput.Init10393D30 writes that pointer then immediately
calls ResetInput. Constructor-only preparation can retain a bookkeeping
ownerViewport/generation identity, but must leave the native Viewport pointer
null and initialized=false until authentic Init completes. GC still roots Input
through the viewport's actual owning Input edge. Do not partially execute Init
by writing only its native back-pointer.

ResetInput is a later operation; do not move LoadMap's later reset ahead of
Possess just to satisfy key queries. Init/Reset/platform UpdateInput and actual
ReadInput have their own order and native service requirements. See
ORIGINAL-TRAINING-CONFIGURED-INPUT-GAP.md.

Also distinguish original native544 ResetKeyboard from UInput.ResetInput.
Engine103021B2 ->103B96D0..103B9756 checks actual Player is a Viewport with
nonnull Input+60, then passes that Input's actual Class+24 to UObject.ResetConfig
through IAT1059A098. DeusExPlayer.TravelPostAccept reaches this at logicalPC6.
It needs a genuine configured-class/default-binding/config reset service, not
the later key-state ResetInput or an empty callback. Config file/cache mutation
authority must target app-owned deployment state or isolated test copies,
never the original installation used as read-only source evidence.

Native Alias member Port must use real metadata: reject uint32(ElementSize*
ArrayDim)==0; reject NAME_None category when flags&3000; reject actual field
name Class; otherwise admit. Original intrinsic29B is Class, independently
verified at Core1014C8A6..1014C8BC/literal101AC4C4. Resolve its portable FName
identity rather than comparing a portable name index with literal667. Bind
typed Struct import to real Alias Children/Next/Super and actual member fields,
not a permissive Port=true. Raw binding config is not blanket Port-filtered.
Ordinary malformed-import return values preserve prefix writes and warnings;
only genuine provider/provenance/work/resource faults abort the atomic phase.

## ResetConfig native544 dependency

Core exportEAT1156/ordinal1157,10101A14 ->10151AC0..10151D85 selects Default.ini
for System ConfigName and DefUser.ini for User; other names return. It reads
ONLY the supplied Class section as packed key=value CStrings (cap32767), splits
each at its first equals and updates the mutable config cache. It does not
clear the destination section, invoke SaveConfig/Flush or call Windows profile
APIs. Then reload matching Class/descendant CDOs first with LoadConfig1, then
matching live instances with LoadConfig1 and PostEditChange. The native indexed
iterators use ascending actual identities and refresh object count each loop.

Original default blocks have Index-1 and are not ordinary instances. Portable
defaults have positive allocation leases, so explicitly exclude defaultOwner
objects from the instance pass to avoid a second CDO reload/PostEditChange.
Do not model reset as only a default copy or key-state clear.

The explicitly audited Editor backend GetSection10201852 ->1021B980 returns0
for an absent section and otherwise packs source-order duplicate pairs with
final NUL. SetString10201A3C ->1021BF90 changes cache/dirty state, not disk.
Only later Flush102026F8 ->1021A4B0 invokes FConfigFile.Write1021A770, gated by
Dirty and not NoSave, then appSaveStringToFile. This Editor profile does not
establish the packed launcher's concrete config factory; bind the portable
app-owned cache explicitly.

Owned DefUser.ini equals User.ini at SHA256
A276E5261238631A1DEF4AC769A35FF934F0A61877273F2D8AC331E0C478B531. It has
Engine.Input but NO Extension.InputExt section. Reset of configured InputExt
therefore makes zero SetString calls in this actual profile, while still
genuinely reloading its CDO and live instances. Its inherited PostEditChange
1014F9C0 calls Modify10150170; GUndo is called only for nonnull GUndo and
transactional ObjectFlags bit1. Actual Input instance flags4000 prove that gate
false; this is not permission to blanket-no-op the virtual service for other
classes or flags. Never alter original source config files for this work.

## Extension name bootstrap is52 real identities, not a no-op

10022A80..10022ED3 checks guard10097CB0 (initial1), creates/resolves the following
52 names in order and stores their actual identities in1009B320..1009B3F0.
Only after all calls does10022EB4 clear the guard. A zero guard skips the work.

```text
InitWindow, DestroyWindow, WindowReady, DrawWindow, PostDrawWindow,
ParentRequestedPreferredSize, ParentRequestedGranularity,
ChildRequestedVisibilityChange, ChildRequestedReconfiguration,
ChildRequestedShowArea, ConfigurationChanged, VisibilityChanged,
SensitivityChanged, ChildAdded, ChildRemoved, DescendantAdded,
DescendantRemoved, CursorRequested, MouseMoved, MouseEnteredWindow,
MouseLeftWindow, FocusEnteredWindow, FocusLeftWindow,
FocusEnteredDescendant, FocusLeftDescendant, Tick, ButtonActivated,
ButtonActivatedRight, ToggleChanged, BoxOptionSelected,
ScalePositionChanged, ScaleRangeChanged, ScaleAttributesChanged,
ClipAttributesChanged, ListRowActivated, ListSelectionChanged,
ClipPositionChanged, FilterChar, TextChanged, EditActivated, CalcView,
RawMouseButtonPressed, MouseButtonPressed, MouseButtonReleased,
RawKeyPressed, VirtualKeyPressed, KeyPressed, AcceleratorKeyPressed,
StyleChanged, ComputerStart, ComputerInputFinished,
ComputerFadeOutCompleted
```

Every call uses FName EFindName2 through Extension IAT100590DC. Core10102E96
->1014B950..1014BB81 preserves existing identity; newly added mode2 entries gain
04000000 at1014BB58..1014BB67. Existing entries do not retroactively gain that
flag. Portable NameString currently gives all names permanent process lifetime
but has no equivalent original flags API. State that embedding lifetime contract
explicitly; it is not evidence that native FName flag storage was reproduced.

This audit does not establish InputExt.Process, configured viewport FExec,
held-key execution, player input, completed startup or live crate gameplay.
