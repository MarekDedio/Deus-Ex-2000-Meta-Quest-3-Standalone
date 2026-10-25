# Original Input key queries and complete Enum decoding (2026-10-25)

This checkpoint implements the general key-query routines and their original
package-data prerequisite. It does **not** attach an Input instance to the live
viewport, complete player startup, or make crates/loot/gravity playable. The
first `00_Training.dx` remains the only integration target. No account login,
character-creation UI, other-map integration or new APK belongs to this work.

## Implemented scope

`native/surreal_portable_package_tables.{h,cpp}` now decode the complete actual
`Core.Enum` body: UObject tagged-property prefix, both UField links, compact
count and every compact package FName reference. Original identity, order,
duplicates and None entries survive. Invalid counts/references, truncated or
trailing bodies and byte-budget overruns refuse. Production
`LoadPortableFieldLinks` now uses this decoder for Enum exports, rather than
accepting a valid UField prefix while ignoring a malformed Enum tail.

`native/quest_input_key_queries.h` implements the audited general
ParseCommand/appAtoi/GetKeyName/FindKeyName/KEYNAME/KEYBINDING protocols described
in ORIGINAL-PLAYER-INPUT-CONSOLE.md. They preserve UTF16 CString boundaries,
ASCII command grammar, original FName identity, non-interning FNAME_Find,
live Enum traversal and the reread in GetKeyName. Binding output has no added
newline. Missing keys are handled with no output; unknown commands return
unhandled for an actual caller to continue or explicitly refuse. They are not
a catch-all FExec implementation. Unproved integer overflow and oversized
native format-buffer inputs explicitly refuse, rather than inventing indices.

The operations interface must be supplied with genuine Input, Enum, FName,
config, work-budget and ordinary-log output authority. The helper constructs no
UObject, default object, key table, persistent binding or FName. It is not yet
connected to PlayerPawn.ConsoleCommand; that production handler still honestly
refuses a bound player until native Input construction is available.

## Verification

- The commercial-free query test passes 20,430 checks / 123 refusals, including
  command cursor/suffix behavior, signed integers, UTF16/NUL boundaries,
  duplicate/missing/live-mutating enum names, output composition and work cuts.
- The generated complete-Enum fixture passes 48 checks / 27 malformed or
  budget refusals, including production field-link traversal.
- The owned read-only test passes 947 checks, 131 complete enums and 510 key
  queries in eight original packages. It distinguishes the actual
  `Engine.Actor.EInputKey` from `Engine.Console.EInputKey` and checks every
  key index against the actual names. Binding data in this test is an explicit
  supplied User.ini scalar inventory, **not** proof of native CDO/config
  initialization. All eight packages and User.ini retain size, mtime and hash.

Coherent desktop rebuild and shared CTest pass: 197 executed / 236 registered,
39 explicit owned-data skips, zero failures in53.36s. The first gate correctly
rejected an older descriptor fixture that relabeled a full UState payload as
an Enum; it now retains the UField prefix followed by a genuinely empty Enum
Names array. Strict ARM64 checks pass for the decoder and all four changed/new
test sources. The native Android libraries build and link successfully; no APK
was generated.

Original first-Training normal startup repeat2 still observes220285
instructions/25439 writes. Aimed two-Fire repeat2 still observes228837/26047,
48 rows/two completed cycles/two returned notifies/four real traces/two crate
health changes/nine births. Both repeats retain the actual lower crate's
10->4->-2/removal, six WoodFragments and surviving Lockpick, then reach the
same named bound ConsoleCommand/FExec refusal. Full save/state/revision/hash/
RNG/client/lookup/Actor/GC undo and all41 unchanged owned sources pass.

Those commands explicitly supply `--native-work 16777216`, as at the preceding
checkpoint, plus `--repeat 2` and the exact expected refusal. Initial runs that
omitted this diagnostic option correctly stopped at the default65536 shared
work limit in Mover.PostBeginPlay; no production limit was raised or failure
hidden. For Fire the explicit packets remain `--fire-training-weapon
00_Training.WeaponCrowbar1 --fire-location 1100 -275 -65 --fire-rotation -8192
32768 0 --fire-attacks 2`. Expected-boundary exit0 proves regression/undo only,
not successful startup. No other map was loaded.

Separate actual first-map controls pass: original player/HUD8301, isolated
native279 crate347/two genuine dormant-contents refusals, Actor.Trace277
43 production calls/37063 checks/23 refusals/nine shared-work cutoffs, and
original PlayerPawn.ConsoleCommand65/one refusal. The actual key-data947/131/
510 test also passes again after the final build. None of those separate
fixtures turns the combined startup diagnostic into live gameplay.

Local ignored evidence is in `artifacts/input-key-*-20261025.log`, including
desktop-final-build, android-build, ctest-final, startup-final-repeat2,
fire-final-repeat2 and owned-test logs. Every build/test handle is terminal
before this checkpoint is committed.

## Actual initialization dependency (static evidence)

Fresh read-only disassembly of the same owned Engine.dll/Extension.dll hashes
recorded in ORIGINAL-PLAYER-INPUT-CONSOLE.md establishes more than a back-pointer
assignment. No commercial code was loaded or executed.

`UInput::Init` calls `ResetInput` at Engine10394910..10394A7E. Reset requires a
real viewport, clears 255 native key-state bytes, dereferences the viewport's
actual Actor/Class and iterates input properties. The first iterator helper
10303030 ->10394EC0..10394F15 filters actual ByteProperty inheritance
(IAT1059A010 -> name record1059DE82); the second loop filters actual
FloatProperty (IAT1059A018 ->1059DEE4). Properties with flags bit4 have their
BYTE/DWORD input storage zeroed. Reset then clears the two native accumulators
and calls actual viewport virtual slot +7C with argument1. Base UViewport's
slot at10428FFC is104135CA -> IAT1059A348 (_purecall); the concrete platform
viewport must supply this service. Initializing against an Actor=None wrapper
or treating ResetInput as a no-op would be false authority.

Construction and reset are distinct phases. The already-audited original
LoadMap continuation calls Input.ResetInput **after** SpawnPlayActor/SetPlayer/
Possess (1038DFF3 versus helper1038BF10); key queries occur inside Possess.
The reached queries themselves only need constructor/config/Enum/binding
authority, not a newly invented requirement to reset before that callback.
Do not move the later reset ahead of the HUD merely because it is an eventual
startup dependency. See ORIGINAL-POST-PARTITION-CLIENT-SETUP.md. The embedding's
initial Init call order and concrete platform event/reset service remain to be
established separately.

The configured class is still `Extension.InputExt`, not bare Engine.Input.
Its constructor1002CFB0..1002D073 calls UInput's constructor, installs its own
vtables, performs original default/instance LoadConfig calls and calls the
Extension name bootstrap10022A80..10022ED3. That bootstrap has a guarded global
name-registration phase and then clears its guard; it is not an arbitrary
empty callback. The constructor's1002D004..1002D014 comparison is a **size**
check: Class100A2E00+4D8 is Defaults.Num, compared with virtual+64
GetPropertiesSize (Core101026E4). It is not a default-object identity check.
The default data pointer passed to LoadConfig is instead Class+4D4 at100A32D4.
Full configured native metadata/default/config/instance lifetime and the real
viewport association remain required before binding the queries to player
startup.

These are integration dependencies, not completion of live controller combat,
automatic world updates, crate settling, collectible loot or headset acceptance.
