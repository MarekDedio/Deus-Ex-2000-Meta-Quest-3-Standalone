# First Training: configured viewport Input dependency

Read-only next-step evidence,2026-10-26. No Input/InputExt registration,
constructor, default/config or ReadInput service is implemented here. The
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
Engine.u/Extension.u. Add genuine native registration/default/field authority,
not fake classDescriptor records or Client/Viewport wrapper impersonation.
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
initialization. Effective inherited class/default-copy/config-section policy
and full name-bootstrap inventory still need completion before implementation.

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
