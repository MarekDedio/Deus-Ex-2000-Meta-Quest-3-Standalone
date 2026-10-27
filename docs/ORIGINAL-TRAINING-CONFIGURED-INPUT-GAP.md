# First Training: configured viewport Input dependency

Updated2026-10-27. Explicit native Input/InputExt classes/configured-class
selection and the complete native Alias/key field/link/pre-config-default
graph are implemented. The latter passes26262 owned checks/1342 refusals;
defaultsPrepared is true but configApplied remains false. Actual constructors,
User config and ReadInput services remain unimplemented. The
actual first-map discovery now reaches `Original viewport ReadInput configured
Input service is not prepared` after the local-player camera/fresh-Player and
actual viewport/session/backedge checks. The coordinator is documented in
ORIGINAL-TRAINING-LOCAL-PLAYER-TICK-GAP.md; it is not a completed input pipeline.

## Actual native ReadInput chain

- UViewport vtable10428F80 +54/cell10428FD4 ->10303733 ->10374990 requires
  Input at+60 first. Delta equal-1 OR unordered/NaN skips UpdateInput+7C(0),
  but it always calls actual Input+64(delta,*GLog).
- Base viewport+7C/cell10428FFC ->104135CA ->IAT1059A348 is _purecall, not
  a no-op. A concrete VR platform update/reset service is required.
- InputExt+64/cell1005C450 ->10054648 ->IAT100592DC imports original
  UInput.ReadInput103020EF ->10394580. Held keys use its distinct Process+60
  override1002D0B0, not a base-handler substitution.
- UInput.ReadInput10394580..10394847 first tests actual GIsRunning; false
  bypasses cache/keys/scaling, not UViewport's earlier Input requirement.
- Its cache collects ALL inherited actual UProperty fields with flags bit4.
  Held key bytes0..254 call actual Process(output,key,action2,rawdelta), except
  for delta-1/unordered. Scaling touches only genuine FloatProperty at each
  actual offset once; there is no ArrayDim or Byte/Bool/Int reset loop.
- The retained PC64 factor is Double20.0/Floatdelta (1042E4E0), or Double+0
  for-1/unordered (10429838). Multiply then store Float, never clamp/assign0.
  Infinity/NaN behavior is significant, including post-input multiplication.
- Native cache key is uint64(Class.ObjectIndex)*0x100+0x1F. Payload is count
  plus actual property pointers, protected by acquired/released cache lock.
  Portable reuse needs real class/generation authority, not basename caching.

Original Engine.dll and Extension.dll retain the hashes recorded in
ORIGINAL-PLAYER-INPUT-CONSOLE.md. All evidence is external PE/data inspection,
never execution or loading of commercial DLL/EXE code.

## Required owned input preparation

Neither native Input nor InputExt has a top-level serialized Class export in
Engine.u/Extension.u. PreparePortableRuntimeNativeInputClasses now registers
their real indexed native UClasses with actual Super/Within/metaclass/Package
edges, without fake classDescriptor records or Client/Viewport impersonation.
It selects only the actual bounded Input setting beside the loaded Core.u;
Read/repeated Prepare use that committed canonical selection, not fresh INI I/O.
The separate two-class domain adds no defaults, native fields or Input instance.
Current collision-domain default/instance validators are explicitly closed;
reusing their construction pattern does not authorize adding Input to them.

InputExt independent tuple: registrar1002CF10 (UClass ctor1002CF64), native
sizeFB0, supplied class flags0C, object flags04084004, Within Core.Object,
base Engine.Input, configUser. PrivateStaticClass100A2E00 isEAT1016/ordinal1017.
Instance callback1001C670; static callback imports UInput.StaticConstructor,
which is10393570 RET, NOT separate StaticInitInput103930C0.

InputExt ctor1002CFB0..1002D073 calls UInput ctor atCFD6, installs primary/
FExec vptr1005C3EC/1005C3E8. Noneditor checks Defaults.Num vs GetPropertiesSize,
then LoadConfig(1,None,None) on default data AND instance; editor only instance.
It calls actual Extension name bootstrap10022A80 (guard10097CB0), not empty
initialization. Effective class flags are1E: native ctor OR12, then inherited
Super flags masked4EC. Native parent-copy/default profiles are now established;
their actual graph/config/instance implementation and full name-bootstrap
inventory remain pending. See ORIGINAL-INPUT-NATIVE-FIELDS.md for the next
complete field/link/pre-config-default batch, not another class header.

Use a separate authenticated input domain and explicit configured InputExt
resolution. StaticInitInput must provide Alias struct/Aliases40 and native
binding properties from actual Engine.Actor.EInputKey (not Console's enum).
Construct Input with actual Transient Outer; bind genuine Enum/config/bindings,
viewport/Input backedges, GC roots, revocation and typed journal/work/state
accounting. Read queries must not lazily construct any of these objects.

KEYNAME/KEYBINDING's exact parser/query helper already exists. Binding it to
real input state can support the separate HUD FExec dependency, but is not
ReadInput success. Do NOT move LoadMap's later ResetInput ahead of Possess:
key queries run inside Possess and need constructor/config/Enum/bindings, not
a misplaced reset. Actual ResetInput separately needs viewport.Actor, Byte/
Float clearing, key/accumulator state and concrete viewport+7C(1) authority.

Only first Training is in scope. No fake absent Player, forced GIsRunning or
no-op platform/InputExt callback may replace these services. Live controller
weapons, loot, gravity, completed startup and a replacement APK remain unfinished.

## Class-phase verification (2026-10-27)

The owned registration integration passes10413 checks/38 refusals: exactly two
native Classes plus one GC domain, real indexed Super/Within/metaclass/Package
links, canonical/folded/direct-Outer lookup and casts, unchanged prerequisites,
missing-phase/config refusals, copied-Core adjacent-INI selection, committed
read/idempotence after that isolated INI changes/disappears, dormant first-map
checkpoint/warm restore/unload/reload/GC/shutdown. It never writes originals.
Refusal tests are preallocation failures; late allocation/budget fault injection
is not claimed. Actual lookup publication is staged before noexcept adoption.

An initial test incorrectly assumed ANY_PACKAGE lookup of absent Engine.Input
could not resolve an unrelated source object named Input. The corrected test
preserves/compares that original fallback result, checks exact Core.Class
and actual Package Outer without fallback, and passes. Production lookup
semantics were not changed to accommodate the test.

Independent config controls pass960 checks; literal native-header contracts,
strict GNU and ARM64 runtime checks, desktop build and Android-native link pass.
Full CTest registers259:219 executed,40 owned-data skips,zero failures105.87s.
The separate owned Input test above supplies the real data missing from its
default CTest skip. These counts are not a playable startup/headset claim.
Separate owned crate415/5,deferred429/21,animation588/24,FastTrace252/0 and
production Tick27971/584 before final schema control exit0. Final audit verifies
all45 originals and9 frozen inputs unchanged by length/mtime/SHA256, all jobs
terminal. These controls do not fill the remaining Input/VR implementation gap.

Actual first Training repeat2 still safely refuses missing configured Input
service at239200 instructions/19364 writes,144 slots/144 calls. The door variant
retains both56-unit moves/two ends at239536/19432. Two original crowbar attacks
retain228837/26047 and48 observations/two notifies/four traces/two crate changes/
nine births before the later bound ConsoleCommand/FExec refusal. Both diagnostic
families verify complete rollback, cold/warm immutable Model proof lifetime and
cold same-filename generation replacement. No new APK was packaged.

Ignored evidence: artifacts/input-class-final-*-20261027.log and the frozen
nine-input manifest. The earlier failed lookup fixture remains separately
logged; the final owned-registration-fixed log is the passing control.
