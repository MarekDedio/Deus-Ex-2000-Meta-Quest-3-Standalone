# Original native Level tick baseline and newborn marker

First-Training prerequisite,2026-10-25. This corrects Spawn's native bTicked
initialization and establishes the freshly constructed native Level context.
It is not complete Actor/Level Tick, an active pending/newborn scheduler,
automatic crate/loot gravity, an account UI or a new APK.

## Exact allocation/default-copy authority

Static owned Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Core.dll:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
No commercial DLL/EXE is loaded or executed; objdump reads static bytes.

- LevelBase registration1039C9E0..1039CA45 has actual UClass10589058,
  native size0x8C (pushed1039CA1F), actual Core.Object superclass/Within via
  import105999F8, class flags1 and supplied object flags04084004.
- Level registration1039D9D0..1039DA36 has actual UClass10588B58, superclass
  LevelBase10589058 (1039DA07), native size0x1150 (1039DA10), class flags0,
  supplied object flags04084004 and actual InternalConstructor10303AE9.
- Both registrations supply the same UObject.StaticConstructor from import
  105999FC. Its actual Core thunk10101041 reaches1014F690, a complete RET.
  There is no Level-specific static-constructor frame-counter override.
- Independently read owned Engine.u v68/licensee0 tables contain3574 names,
  5622 exports and no serialized Class export named Level or LevelBase.
  Source Class defaults cannot override these native context fields.

The independently established Core default-preparation/allocation path is in
ORIGINAL-MODEL-ZONE-TIMESTAMP-INITIALIZATION.md. UClass.Register prepares its
actual class-size default block; InitClassDefaultObject10152300..36F passes
the actual superclass to InitProperties. The inherited payload is copied,
then10152185..1A7 zero-extends the larger destination. For Level, the tail
0x8C..0x114F includes native pending head0x104, in-tick integer0x108 and frame
counter0x10C. Fresh template0 CreateExport copies those prepared defaults
before the actual instance InternalConstructor, rather than guessing zeros
because package fields are missing.

Actual Level InternalConstructor10345D10..1D enters its default constructor
10345D30..10345E96. The complete body initializes arrays/URL/map members and
vtables but does not write0x104,0x108 or0x10C. The source Level serializer
1039D560..1039D7FF likewise does not archive them. Its sixteen-reference
array is0x9C..0xD8, not0x104: see the corrected address proof in
ORIGINAL-DEFERRED-ACTOR-OWNERSHIP.md. Actual FirstDeleted0x100 is distinct,
serialized and still decoded independently; this zero baseline does not
replace it or justify discarding an authored deletion queue.

This proves the initial zero context of a freshly constructed exact native
Level, not arbitrary subclasses, explicit templates or reused live state.
Do not re-zero the frame counter on each frame, native clock call or save.

Reproduction:

```powershell
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x1039C9E0 --stop-address=0x1039CA46 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x1039D9D0 --stop-address=0x1039DA37 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10345D10 --stop-address=0x10345E97 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x1039D560 --stop-address=0x1039D800 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103952FA --stop-address=0x1039531F 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
```

## Original newborn bTicked assignment

Engine103952FD loads the native Level frame counter at0x10C. After TEST and
SETE at10395309..0E, it masks the result to1, clears only source packed
bTicked bit0x200 and stores the new bit. Therefore the newborn marker is the
logical opposite of whether the **Level counter** is nonzero. It is not copied
from the spawning Actor, owner, template or class defaults. Other Actor flags
remain unchanged.

The portable Spawn path previously copied spawner.bTicked. It now reads the
actual loader-owned current-generation Level context and writes the real
Engine.Actor.bTicked scalar Bool declaration (flags0x2002) through the same
typed reference/Touch journal and existing saved-property channel. The raw
native assignment retains its existing interpreter write-count convention.

The Level context is owned only by a fresh exact serialized Level. Its retained
bytes are admitted by the root state budget. No scheduler mutation API is
exposed in this increment, so its current baseline is immutable across the
ordinary existing portable checkpoint operations. A future scheduler must
add shared-root native journaling, checkpoint continuation and pending-node
GC/lifetime ownership BEFORE exposing context mutations. Reached active-tick
Spawn currently explicitly refuses that missing integration rather than
silently omitting the native pending insertion.

## Verification and remaining work

Generated Spawn controls deliberately change the spawner's marker to false;
the newborn must still be true against the genuine Level0 baseline. Ordinary
true-spawner, saved/reloaded birth and legacy-reset/new-birth controls protect
the same ownership distinction. Generated source schemas now declare the real
0x2002 flags instead of accepting a mismatched fixture-only field.

Final verification,2026-10-25:

- Generated Spawn integration13614 checks/223 rejection controls passes,
  including the false-spawner/newborn distinction and birth save/reset tests.
- Full desktop build, strict ARM64 changed-test syntax checks and Android
  native link pass. An initial desktop link hit a Windows executable lock
  because tests had started too early; after those tests exited, the final
  build and a fresh full CTest run were completed in sequence.
- Final shared CTest:241 registered,202 executed,39 explicit owned-data skips,
  zero failures,60.80s. The final run uses the newly linked Spawn executable.
- Actual first-Training crate415/5, deferred ownership429/21 and animation
  MAIN/BLEND588/24 pass separately against the owned installation.
- Original startup/distance repeat2 retains220285 instructions/25676 writes,
  237 distance stores and full enclosing undo. Aimed original two-crowbar-Fire
  repeat2 retains228837/26047,48 observation rows,2 cycles/returned notifies,
  4 actual traces,2 crate changes and9 births; removal/surviving loot/debris
  remain a rollback-only diagnostic, not published headset gameplay.
- Both startup paths stop at the same explicit unsupported bound-player
  ConsoleCommand FExec. All45 checked original script/config/map files retain
  exact SHA256/length/modification time. All build/test processes are terminal.

These controls do not establish an active Level frame or a complete world scheduler.
The original owner-defer and newborn linked-list batches, current frame toggle,
renderer timing, cleanup and live player/input/publication still require
implementation. Only first Training gameplay integration remains prioritized;
other maps/account UI are not added to justify this prerequisite.

The next frame operation must join the internal stasis, owner/parity,
MAIN/BLEND and post-animation phases inside ONE shared transaction, with
Level-owned pending-node lifetime/GC and context rollback. Chaining separate
public Advance APIs would create independent roots and is not equivalent.
Native parity checkpoint continuation and actual CleanupDestroyed must be
implemented before a successful complete frame can be published; unsupported
Pawn/native overrides must never be silently skipped. No APK was packaged.
