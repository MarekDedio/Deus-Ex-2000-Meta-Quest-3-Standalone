# First Training: native3015 DumpLocation prerequisite

2026-10-22. Static audit only. The actual first-Training startup probe now
reaches `DeusEx.DeusExGameInfo.Login` PC133/native3015 after creating the real
JCDentonMale. This document does **not** implement that service, accept Login,
complete startup/crates/loot/gravity, or produce an APK. No other map was tested.

## Provenance and method

Owned files under `D:\Steam\steamapps\common\Deus Ex\System`:

- `DeusEx.dll`, 245760 bytes, SHA256
  `B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04`.
- `DeusEx.u`, 5386972 bytes, SHA256
  `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480`.

The PE was read as bytes with `pefile` and disassembled with MinGW
`objdump -d -M intel --start-address=... --stop-address=...`. Its preferred
image base is `10000000`; addresses below are preferred VAs, not executed
pointers. **No commercial executable/DLL was loaded or executed.** The local
`script_bytecode_inspect` reads package data only. The existing independent
listing is `artifacts/training-login-script-audit-20261022.log`.

## Original caller and selected branches

`DeusExGameInfo.Login` has551 logical bytecode bytes. Its nonnull Player and
absent `Loadgame` option gate leads to `ResetPlayerToDefaults` PC112, then:

- PC118 assigns local `dump`; PC133 invokes receiver Player's native3015 with
  **no arguments**, expecting an Object constrained to `DeusEx.DumpLocation`.
- PC136 tests `dump != None && dump.HasLocationBeenSaved()`; the latter is
  native3039 at PC160. None short-circuits that second operand, but returning
  fabricated None would bypass the actual successful-allocation behavior.
- The true branch calls native3038 `LoadLocation` PC173, pauses the player,
  applies Location/ViewRotation through original SetLocation/SetRotation and
  ClientSetRotation, and calls CriticalDelete751 on `dump` PC321.
- The false branch at PC332 obtains Team, selects FindPlayerStart, calls
  SetLocation267 PC442 and SetRotation299 PC468, writes Pawn.ViewRotation and
  calls ClientSetRotation. Login returns Player at PC543/544. **This false
  branch does not explicitly CriticalDelete the allocated dump object.**

Fresh session versus a previously saved dump location must be real state,
not a branch chosen merely to get past the refusal. Even the fresh false
branch still requires actual native3015 allocation/constructor behavior.

## Native3015: identity, Outer, allocation and constructor

The exported declaration `DeusExPlayer.CreateDumpLocationObject` has native
index3015, flags`0x401`, zero logical script bytes, no input parameters and
one Object ReturnValue constrained to DumpLocation. Independent DLL binding:
`1000E6E0..1000E6F8` registers index`0xBC7` using member-pointer cell
`100342D8`, whose bytes `30 4c 01 10` identify `10014C30` (export ordinal248).

At `10014C30..10014C7F` the exec thunk advances FFrame.Code once, constructs
FName enum0/None through import`10027138`, calls **receiver.GetOuter()** through
import`100271A8`, and supplies that Outer, NAME_None and instance flags0 to
`UDumpLocation::operator new` at `100134C0`, with native size`0xA8`. It is not
an Actor.Spawn and does not read a script-supplied Outer/name/flags argument.
Allocation returning null writes null to ReturnValue; successful allocation
calls the actual constructor`10014FB0` and writes its returned pointer.

`100134C0..100134E9` calls imported `UObject::StaticAllocateObject`
(`10027040`) with actual UDumpLocation class`1003B600`, the received Outer,
name/flags, no template, GError and null instance graph. StaticClass export
ordinal232 at`100134B0` returns that same class. Its native registration
constructor at`10014EC0..10014F15` supplies size`0xA8`, literals DumpLocation
(`10034BCE`), DeusEx (`10034308`), System (`100341CC`), UObject base/Within,
object flags`0x04084004`, InternalConstructor`100136E0` and UObject's static
constructor. Those registration inputs are not a recovered final serialized
ClassFlags/default block. Actual loaded package metadata must remain authority.

Constructor`10014FB0..1001511F`:

- Calls UObject constructor and initializes its owned compound fields.
- Initializes directory-array storage at`+7C..+84` to zero, indices`+88/+8C`
  to`-1`, user FString storage`+90..+98` to zero, and scalar/pointer fields
  `+9C/+A0/+A4` to zero. `+A4` is the Player pointer, independently confirmed
  by SetPlayer export ordinal219 at`100160B0..100160BA`.
- Reads GConfig's `User.ini`, section`DeusEx.DumpLocation`, key`CurrentUser`,
  into a64-WCHAR buffer and copies it to the user FString. No matching key
  was found in the currently owned System INI files; that observation does
  not establish missing-key behavior or license inventing a user value.
- Allocates a`0x1220` helper through GMalloc and stores it at`+28`, then calls
  PopulateDumpDirectory`10015200`.

PopulateDumpDirectory clears its old FString directory array, formats the
literal `*.%s` with `DMP` to `*.DMP`, invokes GFileManager virtual slot`+28`
with files=true/directories=false, and copies the returned names. No explicit
directory or current-user prefix is appended in this function. The current
owned System directory contains no matching files; Android search-root and
filesystem-service ownership remain to be established explicitly. Do not
silently replace this enumeration with an unconditional empty directory.

## Native3039 is DLL-session state, not an object/file-count default

HasLocationBeenSaved export ordinal169 at`10016150..10016157` zero-extends and
returns byte`1003BB05`. Native3039 registration references member cell
`10034B8C` -> exec`10016E40`, which advances Code, calls that getter and writes
the result. RVA`3BB05` lies beyond `.data` raw end RVA`38000`, within its virtual
end RVA`3E63C`: it is loader-zero-filled session storage, not a file byte.

SaveLocation3037 (`100160C0..100160FF`) sets the global byte to1 and copies
the object's two vectors at`+40` and`+4C` into globals`1003B5F0` and`1003B5D8`.
LoadLocation3038 (`10016100..10016148`) does nothing when the flag is zero;
otherwise clears it and restores those two vectors to the object. Object
construction does **not** reset this session-global saved-location flag.
This state therefore cannot be inferred from a per-object zero or DMP count.
Portable session initialization, mutation, rollback and persistence policies
still require implementation and tests before either branch is accepted.

## Vendored source: corroboration, not original-behavior replacement

At vendored SurrealEngine commit`677ee14c5b83486e6634687953779aafb7973ad6`,
`Native/NDeusExPlayer.cpp` registers3015 and returns
`UDeusExPlayer::CreateDumpLocationObject()`. However,
`Packages/DeusEx/UDeusExPlayer.cpp:21` allocates a named DumpLocation in the
transient package with Transient flags. **The owned original thunk instead
uses receiver.GetOuter(), NAME_None and flags0.** Follow original evidence.

`Native/NDumpLocation.cpp` only logs unimplemented calls and returns constants
for3039 and other methods. Those stubs do not prove original constructor,
filesystem, session-global or lifetime semantics. `PropertyOffsets.cpp:2975`
lists the script-visible DumpLocation fields but is not proof of native layout.

Next implementation must retain a genuine typed UObject, actual Outer/default
and native-owned state, bounded allocation and same-root rollback; not an
Actor substitute, unconditional None/false or a hidden startup branch bypass.
Constructor helper/file services, destruction (`10015120` closes/frees owned
resources then calls UObject.Destroy), cold/warm state and native3037/3038
mutation are not implemented or accepted by this static audit.
