# Original bound player input console dependency (2026-10-25)

Follow-up2026-10-27: authentic configured Input construction and bound viewport
KEYNAME/KEYBINDING are now implemented. See ORIGINAL-VIEWPORT-INPUT.md. The
historical discovery boundary below is superseded; the rest of FExec fallback
and actual ReadInput remain unimplemented. No absent-Player shortcut was used.

This is a static dependency audit, not an implementation or gameplay acceptance
claim. Actual bound Training startup now reaches
`DeusEx.HUDMultiSkills.RefreshKey` and refuses with
`Bound PlayerPawn ConsoleCommand native FExec is not implemented` at
`Engine.PlayerPawn.ConsoleCommand`, native index0/op0. The authentic unbound
`Player=None` branch already returns an empty FString. It must not be used as a
substitute for the genuine bound player/viewport/input service.

Only read-only package inspection and PE disassembly were used. No commercial
DLL or executable was loaded or executed, no runtime/compiler input was edited,
and no build was run for this audit. All addresses below are original preferred
32-bit image VAs, not portable ABI offsets.

## Owned inputs

The inspected installation is `D:/Steam/steamapps/common/Deus Ex/System`.

| File | SHA256 |
| --- | --- |
| Engine.dll | 0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4 |
| Core.dll | 8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4 |
| Extension.dll | 93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C |
| Engine.u | 1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA |
| Extension.u | 9984FAEDA55FB2100B1A15A37BFEB8CDB4878248B39AFF832002A799F1D787F7 |
| DeusEx.u | 257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480 |
| DeusEx.ini | B6538EF8ED23E518FCFD88F0108C6B59ADF59FDC7AC3831463AD2A7E94F2AECC |
| User.ini | A276E5261238631A1DEF4AC769A35FF934F0A61877273F2D8AC331E0C478B531 |
| DefUser.ini | A276E5261238631A1DEF4AC769A35FF934F0A61877273F2D8AC331E0C478B531 |

Engine.dll image base is10300000; Core.dll image base is10100000. The current
User.ini and DefUser.ini are identical snapshots, not evidence of an automatic
fallback/layering policy. Extension.dll image base is10000000.

## Entire reached script caller

The owned `DeusEx.HUDMultiSkills.RefreshKey` body is189 logical bytes, flags2.
It has FString locals `KeyName` and `Alias` (flags400000) and Int `i`:

1. Clear actual `curKeyName`, set i=0.
2. While i<255, call actual Player.ConsoleCommand with `KEYNAME ` concatenated
   with String(i). This is a VirtualFunction call, not Actor.ConsoleCommand.
3. For a nonempty result, call `KEYBINDING ` concatenated with that exact name.
4. If the entire returned binding is case-insensitively equal to `BuySkills`,
   store this KeyName and break. It is not an alias-substring search and does
   not resolve an alias command graph for this comparison.
5. Otherwise increment i and repeat. If no match was found, use the actual
   localized `KeyNotBoundString` property.

The loop/query operations are at logical offsets15/42/93/122/140/154/164/176.
Thus a correct unbound key is a legitimate empty binding; a missing native Input
service is not permission to claim that all keys are unbound. There is no
`BuySkills` binding in the inspected User.ini/DefUser.ini, but that does not
eliminate the authentic255-entry query loop or justify a hardcoded final label.

## Full PlayerPawn wrapper and output sink

Owned `Engine.PlayerPawn.ConsoleCommand` has flags400, nativeIndex0 and precisely
FString Command flags400080 plus FString ReturnValue flags400580. Its base-field
link names Actor.ConsoleCommand, but its native body is a different wrapper.

`Engine.dll103BA200..103BA3F3`:

- Steps the Command operand once using the current frame Object, then advances
  the EndParms byte at103BA256..25A.
- Assigns ReturnValue the empty FString before checking Player. The Player
  pointer is pawn+5D4, tested at103BA2C7. None jumps directly to command-local
  cleanup103BA3AF; no fallback FExec or Actor.ConsoleCommand call occurs.
- Bound Player constructs a stack FString output device at ebp-3C: vptr1042B934,
  contained FString at+4. Empty command storage passes the original empty wide
  string10485C3C, not a null pointer.
- At103BA2FB..30C calls slot0 of the actual Player's FExec subobject:
  ECX=Player+2C, with UTF16 CString command and output-device reference.
- Ignores the FExec handled Boolean. Copies the output device's FString into
  ReturnValue at103BA30E..375. Empty output stays empty whether the provider
  handled the command or not. This is not authority to omit provider traversal.
- Destroys the output FString at103BA375..3A9 and the Command FString at
  103BA3AF..3DB, then returns8 at103BA3F1. No output-device pointer escapes.

The sink vptr1042B934 contains thunk1030179E ->10374500..103745AE. Its Serialize
appends the supplied UTF16 CString to its FString (or assigns it when empty),
ignoring the event argument and **adding no newline/separator**.

KEYNAME/KEYBINDING use imported `FOutputDevice::Log` at IAT10599DD4. This resolves
to Core thunk10102E00 ->101452B0..101452DC: when its global suppression gate does
not bypass logging, it calls Serialize slot0 with event2F8. It does not itself
append a newline. The gate reads Core1029A1FC and flags at an object reached via
102961F0; its broader error/suppression ownership was not established here.
An implementation must explicitly bind the ordinary logging profile or audit
those globals rather than claiming all process-global states are covered.

## Actual FExec route

`UViewport::Exec` export thunk1030204A ->10373A60. ECX is the FExec subobject
at viewport+2C. Its leading dispatch is:

- Assert the actual Player.Actor pointer (viewport+30) is nonnull.
- If actual Input (viewport+60) is nonnull, call Input+28's FExec slot0 at
  10373AAF..3AC9. A true result returns immediately through10373B0C.
- Only if unhandled, try actual RenDev (viewport+64), FExec+28, at
  10373ACE..3AE5. Then proceed to viewport-specific commands beginning GETPING.

Both reached key commands return true from UInput even when their result is
empty. Thus they stop before RenDev/viewport/UPlayer fallback. The rest of the
viewport command body was not exhaustively certified by this audit.

2026-10-27 correction:the old Console label at+64 was wrong. The literal
1047BFE0="RenDev" is used for that actual slot at10373775/10373973. Canvas is
at+5C; actual Console is+34,constructed from UConsole105823E0 at103896D5..10389702.
Later viewport fallback10374267 offers Console.ProcessConsoleExec+48,then
UPlayer. The complete reached SET route/persistence contract is recorded in
ORIGINAL-INPUT-FEXEC-SET-GAP.md; it is not an implemented fallback chain.

`UPlayer::Exec` thunk10302CB6 ->103D7ED0..103D8098 is not a replacement for the
viewport's Input-first override. Its observed fallback provider order is actual
Actor.Level+AC virtual74; an object reached through Actor+A8/+544 virtual48;
Actor+628 virtual48; Actor's own virtual48; Actor+C0 linked objects virtual48;
then Actor.Level+40's FExec+28. All calls preserve the actual command/output
device, and false is returned if none handles it. This audit does not assign
unproven field names to the A8/628/C0 providers or implement their services.

2026-10-28 follow-up: actual mappings are now independently verified and the
unknown-command route implemented: A8=Actor.Level(LevelInfo),544=LevelInfo.Game,
628=PlayerPawn.myHUD(NOT Pawn.Weapon3D0),C0=Actor.Inventory. Actual native Level,
Game,HUD,Self,live Inventory and GameEngine offers retain their original order,
Actor rereads and next-after-callback semantics. The integrated SET diagnostic
now reaches missing UEngine.GSys,not a fabricated Core SET shortcut. See
ORIGINAL-PLAYER-FEXEC-ROUTE.md for precise authority,scope and verification.

## UInput KEYNAME and KEYBINDING

UInput FExec export thunk1030178F ->10393E10. Its ECX/ESI is the secondary FExec
subobject at Input+28; the primary UObject begins at ESI-28. This distinction
matters for every field address below.

At1039409D..40E5, `KEYNAME`:

- Uses Core.ParseCommand on the advancing UTF16 command pointer.
- Calls imported appAtoi on the remainder, then actual primary-vtable+6C
  GetKeyName, logs the returned CString and returns true.
- `GetKeyName` thunk10302AFE ->10394B20..10394BFA accepts only0<=key<255.
  It reads actual Input.Enum at+EA4, indexes that UEnum's Names at+34, resolves
  the actual FName spelling, and returns the string after its first three
  UTF16 units if total length>3. Otherwise it returns the empty CString.
  It does not invent a platform/USB key table or independently verify `IK_`.

At103940E8..4130, `KEYBINDING`:

- Uses the same ParseCommand, then primary-vtable+70 FindKeyName with the entire
  remaining CString plus a local OUT key index.
- `FindKeyName` thunk1030341D ->10394C40..10394CF0 formats `IK_%s` (literal
  1047F4B8) into a256-wide-unit stack buffer and constructs FName with
  FNAME_Find0. It does **not add/intern** an unknown name. It searches the actual
  Input.Enum Names array in ascending index order and returns the first match.
- A failed name lookup emits nothing but still returns true. On success, reads
  the actual Binding FString at Input+2AC+12*key and logs its CString (or empty
  CString for an empty array). It does not mutate that binding or execute it.
- The full remainder is significant: ParseCommand removes its immediately
  following spaces/tabs, but FindKeyName does not tokenize away a trailing
  argument/whitespace suffix. Oversized formatting, FName capacity/locale edge
  cases and CRT integer overflow require explicit portable refusal/audit,
  not silent truncation or an invented successful answer.

Core.ParseCommand thunk10102BE9 ->10147A20..10147B42 skips leading ASCII spaces
and tabs, compares the command prefix case-insensitively, and rejects a match
when the next unit is an ASCII letter/digit. An underscore is not one of those
rejected suffix units. On success it skips following spaces/tabs. A suffix
rejection rewinds only the matched command, not previously skipped leading
whitespace. appAtoi thunk1010111D ->101235B0 calls CRT `_wtoi` through102AB514;
the existing config-import audit already refuses unestablished signed overflow.
The reached script sends short decimal0..254 values and actual short key names.

## Native Input registration, metadata and initialization

Native class registration at10392FE0..10393047 supplies sizeFB0,
registrationClassFlags0C, registrationObjectFlags04084004, base Core.Subsystem,
within Core.Object, name Input, package Engine, and config name **User**.
PrivateStaticClass is10588550 (export name-table index1415/RVA288550).
These are registration arguments, not an independently certified effective
inherited flag word or a portable memory layout.

The native ctor10393430..103934FD constructs:

| Original native field | Layout/evidence |
| --- | --- |
| FExec | Secondary vptr at+28, value10428C14 |
| Aliases |40 entries of16 bytes at+2C; FName Alias then FString Command |
| Bindings |255 FStrings of12 bytes at+2AC |
| Viewport | Native pointer at+EA0, assigned by Init |
| Enum | UEnum pointer at+EA4, resolved in the ctor |

StaticInitInput thunk10303A08 ->103930C0..10393344, distinct from the empty
StaticConstructor10393570, creates a UStruct Alias of size10hex, config
NameProperty Alias at0 and config StrProperty Command at4, then config
StructProperty Aliases at2C with arrayDimension40. It next creates one native
config StrProperty per non-None enum entry: name is the enum's actual spelling
minus its first3 units, offset2AC+12*i, category RawKeys, flags4000.

The lookup in both the ctor103934C5..4E8 and StaticInitInput1039324C..268 is
`StaticFindObjectChecked(UEnum, AActor::PrivateStaticClass, "EInputKey",0)`.
The outer is the actual exported AActor class1057CDF0 (export index1351,
RVA27CDF0), **not ANY_PACKAGE** and not a fictional Engine.Input.EInputKey.

Owned Engine.u contains two EInputKey enums; they are not interchangeable:

- Engine.Actor.EInputKey is export2290, outer export2, payload offset143677,
  size516. Tagged-property terminator0, base0, next4584, count255, followed by255
  compact package-name references, with no trailing bytes. This is the one used
  by native UInput. Examples:0 IK_None,1 IK_LeftMouse,2 IK_RightMouse,
  65 IK_A,112 IK_F1,254 IK_OEMClear.
- Engine.Console.EInputKey is export261, outer309, offset92931, size516,
  next554/count255. Its full name sequence differs from Actor's. A basename
  enum search can silently select wrong metadata.

StaticInitInput links the native class and verifies linked size against its
native size/default buffer accounting at103932DF..3331F. It then calls actual
default-object LoadConfig(Propagate1, ClassNone, FilenameNone) at10393322..334.
The name `User` and supplied-file binding do not prove the packed launcher's
GConfig factory, physical User.ini alias, absent-file fallback, merging or
writeback. See ORIGINAL-NATIVE-DEFAULT-CONFIG.md and
ORIGINAL-NATIVE-CONFIG-IMPORT.md for the already explicit profile boundary.

The viewport ctor at103731E4..10373223 resolves the configured native input
class using StaticLoadClass and `ini:Engine.Engine.Input` (literal1047BF68),
then StaticConstructObject and stores the result in actual viewport+60. Thus
even choosing native Engine.Input is configured class authority, not permission
to attach an arbitrary map Actor or a process-wide singleton table. The
construction passes actual GetTransientPackage as Outer, NameNone, flags0 and
TemplateNone; the viewport association is the owned native pointer plus Init's
back-pointer, not an invented viewport UObject Outer.

The owned DeusEx.ini `[Engine.Engine]` actually selects
`Input=Extension.InputExt` at line32, not bare Engine.Input. For this configured
class the reached query route remains the audited UInput implementation:
Extension's XInputExt ctor1002CFB0 calls the imported UInput ctor, then installs
primary vptr1005C3EC and FExec vptr1005C3E8 at1002CFDC..CFE8. FExec slot0 points
to thunk1005461E -> IAT100592C0, imported Engine.UInput.Exec; primary slots6C/70
point to1005463C/10054636 -> IAT100592D4/100592D0, imported GetKeyName/FindKeyName.
InputExt overrides PreProcess/Process for other input work, not these three
reached methods. Its additional construction/default/config/lifetime still
requires real preparation; directly instantiating the base is not equivalent
configured-class authority.

`UInput::Init` thunk10301460 ->10393D30..10393DAC stores the passed actual
viewport at+EA0, calls primary-vtable+68, then emits an initialization log using
the actual viewport name. The +68 slot is ResetInput (also inherited by the
configured InputExt via thunk10054642 -> IAT100592D8). ResetInput and the remaining
fields throughFB0 belong to broader reset/event/axis services; their complete
initialization/event behavior was not certified here. Serialize10393590..103935E2 calls
the base serializer and serializes the actual+EA4 enum reference.

## Reusable portable services and remaining authority

Current `PlayerPawnConsoleCommand` in portable_unreal_runtime.cpp authenticates
the owned native declaration and original const Player ObjectProperty, requires
an actual map PlayerPawn and one FString, rejects revoked/stale native Player
leases before the None read, returns empty only for actual None, and otherwise
explicitly refuses. This is correct diagnostic separation, not a working
KEYNAME/KEYBINDING implementation.

`quest_native_client.h` supplies a revocable client lifetime/identity and bounded
viewport descriptors/registration. The runtime creates real indexed native
Client/Viewport wrappers with client/viewport identity, actual Player.Actor
binding, Outer/Class/Name and GC roots. There is currently no native Input
UClass/default object, actual viewport-owned Input instance, Input.Enum binding,
config StrProperty inventory or Binding storage attached to those wrappers.

Available foundations include actual owned package/export/property parsing,
Outer-aware lookup/FName identity, metadata UClasses (including StrProperty and
StructProperty), native class/default preparation, transaction budgets/journals
and retained native session roots. `quest_native_config.h` provides immutable
ordered supplied-file records, newest scalar lookup and1023-UTF16-unit bounded
PropertyScalar under the audited Editor config profile. Its explicit supplied
file semantics do not authorize stealing DumpLocation's engine-owned file
binding or silently treating another service's configuration as Input's.
`quest_unreal_string.h` provides UTF16-unit CString/WTF8 transport semantics.

The next faithful increment needs an explicit configured Input-class resolution
and native metadata/default/config preparation boundary; an owned live Input
instance linked to its actual viewport and enum; correct binding import;
authenticated Player->Viewport->Input FExec dispatch; and bounded string/log
results within the existing Execution/journal/budget/lifetime model. Ordinary
read queries must not fabricate persistent state, native work or successful
unknown commands. Revocation, replacement and rollback must preserve authority.

BUTTON/PULSE/TOGGLE/AXIS, alias execution (later in UInput.Exec), input event and
reset behavior, Console, remaining viewport commands, and UPlayer/Actor/engine
providers remain separate unsupported services until implemented/audited. The
presence of the reached query evidence is not a catch-all ConsoleCommand
completion, full input pipeline, playable startup, or crate damage/loot/gravity
acceptance claim. No APK/headset test was produced by this audit.
