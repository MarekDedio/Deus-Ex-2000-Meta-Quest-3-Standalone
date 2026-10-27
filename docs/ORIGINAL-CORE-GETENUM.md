# Original UObject.GetEnum native-zero implementation

2026-10-27. This is an original key-binding/startup dependency,not an account or
character-creation screen. It does not complete original input,root activation,
live VR crates/loot/gravity or a replacement APK. No original DLL/EXE code was
loaded or executed; the original wrapper was inspected only as static data.

## Exact native declaration and packet ownership

portable_core_get_enum.inl authenticates Core.Object.GetEnum,its owned source,
native0/flags2401,Core.Object Outer and actual Core.Function class. Its ten-byte
ordered NativeParm body contains E then i; E is required ObjectProperty80/
Core.Object,i required IntProperty80,and ReturnValue NameProperty580. Materialized
declarations must match the full schema/body; minimal immutable resolution
identities must match the same canonical function/source/flags/native tuple.
Raw numeric zero or foreign similarly-named functions gain no GetEnum authority.

The VM copies E and i once in caller-frame order,independently of the native
receiver,before later argument effects can change prior sources. Original None/
INT0 seeds come from authenticated host authority,not global native-zero defaults.
The adapter validates copied input kinds/dispositions and returns an owned Name
value,not a String or writable alias. Static UObject dispatch is admitted before
unrelated Actor/class-default execution guards; it needs no started world.

Core.dll export10102194 jumps to1013E9A0. Original Steps initialize E=None,then
i=0; result starts Name None. A real None/non-Enum or negative index returns None;
an index at/above actual Enum count returns None; a valid index returns its real
source name. The retained static evidence is
artifacts/core-get-enum-wrapper-static-20261027.txt (ignored).

## Actual current Class and immutable Enum authority

The type test uses the current UObject.Class chain when bound,matching retail
IsA. A not-yet-bound source identity uses the existing cast resolver. This is
important for later linker/map tables: a cached valid generated Enum may have a
newer export index than an older on-disk header. Reopening that header instead
of using the actual current Class was exposed by the existing late-map fixture
and corrected; the source cache itself was not weakened or fabricated.

BorrowNativeEnum centralizes the existing public reader's native Core.Enum,
Core.Struct Within/Outer,current object/index and immutable descriptor checks.
The public reader still returns a detached snapshot; the native borrows only
the authenticated name array and copies the one selected Name. No lazy metadata
registration,whole-array copy,basename key list or wrapped/clamped index is used.
Selected Name storage is admitted to shared scratch and its bytes are work-
charged. An unprepared real Enum service refuses a valid query without publishing
metadata; None/non-Enum/negative early-outs retain their original ordering.

## Targeted verification

The VM suite passes701113 checks/1024 refusals,including1101/26 new GetEnum
controls:direct/final/virtual/global calls,source-frame one-read input copies,
later mutation,NoWrite/zero-fill bounds,foreign/malformed native-zero exclusions,
seed failure before effects and late whole-root write/effect/RNG undo.

Owned metadata integration passes13896 checks/125 refusals,including1128 native
queries across159 actual Enum descriptors. It checks first/middle/last names,
negative/count/INT_MIN/INT_MAX bounds,distinct Actor/Console255-key enums,real
None/non-Enum objects,initialized inputs,wrong kinds/arity,budgets and generated
first-map Enum names/None/retirement/reload. The backing copies are isolated;
only first Training is loaded. Existing complete checkpoint,model,GC,revision,
default/state,source and native-class lifetime assertions remain in place.

The unbound original root now completes its script InitWindow and reaches the
remaining native root-activation guard at InitWindow PC196/opcode11,15491 writes,
198 objects. The current native frame is InitRootWindow,not an unfinished script
callback. Bound-player startup must still be measured independently. Next
activation/input contract and a corrected PreRender WindowReady sequence are in
ORIGINAL-ROOT-ACTIVATION-GAP.md; the required live bridge remains in
TRAINING-FIRST-PRIORITY.md. Accepted VR HUD layout and memory/save limits remain
unchanged.

## Final checkpoint verification

Expanded owned HUD10175,VM701113/1024,metadata13896/125 and owned GC155/16 pass.
Full desktop build,current Android-native library link and strict ARM64 pass.
CTest registers269:225 executed,44 explicit owned-data skips,zero failures,
145.49s. All eight rebuilt owned first-Training regressions pass separately;
actual GC/font/Window controls also pass after the final host rebuild.

The completed script callback is no longer on the failure stack; only native
Extension.PlayerPawnExt.InitRootWindow is active at the genuine activation guard.
The bound two-Fire repeat2 independently reaches FExec SET at271677/32553,retaining
the attack/trace/crate/birth observations and full undo. Door repeat2 retains both
56-unit moves/two endpoints and the exact ReadInput refusal at239536/19432.
See ORIGINAL-INPUT-FEXEC-SET-GAP.md for the newly reached actual persistence route.

Final audit verifies all45 originals/26 frozen compiler inputs unchanged by
length,UTC mtime and SHA256,with all jobs terminal. Passing evidence uses ignored
artifacts/core-getenum-*20261027 logs; corrected metadata and HUD-prefix2 are the
passing controls,not the intentionally retained initial warning/late-linker/
old-stack discovery failures. No budget,save format or accepted VR-layout change.
