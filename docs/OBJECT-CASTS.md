# Original object and class casts

This source update implements normalized UE1 MetaCast (`0x13`) and DynamicCast
(`0x2e`). It removes a real dependency of original inventory initialization;
it does not implement Spawn, inventory lifecycle, AI or full campaign startup.
The installed Quest user-test APK remains unchanged.

## Original semantics

The pinned SurrealEngine `VM/Bytecode.cpp` resolves the target as an actual
UClass before decoding the child. Execution parsing now does the same using
that function's source table, including during nested calls and in untaken
branches. Structural `AnalyzeProgram` remains read-only and only resolves
untyped identities; it does not perform a cast or claim execution support.

The child is evaluated once. Object and Nothing are the only accepted input
kinds; Nothing becomes a null Object. The result is always a detached Object,
either the original operand identity or null, never an assignable alias.
Child effects share the caller's transaction, budgets and complete rollback.

- MetaCast compares the exact canonical UClass identity along the class
  represented by the operand and its BaseStruct chain. An actor instance is
  not accepted merely because its instance class derives from the target.
- DynamicCast compares the target's literal export/UObject Name against the
  operand's actual Class ancestry, using NameString's ASCII-only case folding.
  Names are not FriendlyName or a path-derived leaf. Identically named classes
  in different packages can therefore match DynamicCast but not MetaCast.
  A class-object operand uses its Core.Class metaclass ancestry here, not the
  actor/asset ancestry represented by that class object.

These rules come from `VM/ExpressionEvaluator.cpp`, `VM/ExpressionValue.h`,
`Packages/Core/UObject.cpp` and `Package/NameString.h` in the local pinned source.

## Runtime identities and bounds

Target references, actual ObjClass and nonzero ObjBase references all use the
strict source-package resolver. Supported serialized UClasses have ObjClass=0;
their imports must declare ClassName=Class. The pin's non-Class import lookup
does not select such exports via metaclass ancestry. ClassPackage is not used
as a substitute qualifier. Ambiguous dotted/NUL import segments fail explicitly.
Literal dots and non-ASCII bytes in a declaration Name remain meaningful.

Serialized class declarations take precedence over a fixed, audited fallback
for the pin's native Core metadata and Engine asset UClasses missing from the
export table. These are class identities, not allocated actor placeholders.
Arbitrary names are never promoted to native classes. Native registry lookup
uses a non-interning comparison, so rejected operand strings do not grow the
global NameString table. Other absent native registrations and nonzero-ObjClass
class-like exports remain unsupported rather than inferred from a Class suffix.

A zero ObjBase defaults to Core.Object unless the literal class Name is Object,
matching `Package::LoadExportObject`. An unavailable nonzero base is rejected,
not silently replaced. Cycles, missing identities and a traversal beyond 128
classes fail; an already matching class does not traverse later cyclic bases.
Paths and names are capped at 8,192 bytes. Parsed target path capacities are
charged to the interpreter's shared retained-byte budget across nested calls.
Null cast targets are rejected fail-closed, including for null operands: the
pin has no safe nonnull DynamicCast contract for such malformed targets.

`CastPortableRuntimeObject` is a read-only inspection entry point. It requires
an indexed declaring object's original source table and shares the production
cast adapter. It neither executes bytecode nor enters an actor lifecycle.

## Verification

Final host build and ordinary CTest pass: 34 tests pass, with two optional
original-root tests explicitly skipped in that invocation. VM controls pass
641 checks/150 rejections, the shared hierarchy helper passes 111/54, and the
generated-package production adapter passes 381/59 across 164 exports.
The separate original-data runtime integration completed with exit 0: paired
save recovery, retained original assets, v4/v5 script-state composition and
guarded map travel pass. The original-data actor integration is still running;
its updated startup assertion is not yet counted as a pass.

The Android ARM64 build also completes successfully. APK Signature Scheme v2
verifies; both packaged port libraries match the current stripped outputs,
and no original commercial package files are present. This compile/package
check is not an installation or on-device cast/startup test. The frozen
installed Quest build is preserved.

Fresh Inventory and Health desktop previews pass their original-art/font and
composition checks and were visually inspected. No obvious missing tiles,
panel overlap or text clipping was seen. These remain CPU sample-state
previews, not evidence of live inventory, limb damage, controller input or
OpenXR rendering.

The ordinary test suite registers both the pure helper and production adapter.
The latter loads actual generated UE1 table files, checks source-import
provenance, declaration names, native/serialized precedence and bounded graphs,
and compares complete runtime checkpoints and source bytes for immutability.
It intentionally creates metadata-only fixtures without executable scripts;
their general runtime-startup success flag remains false.

The original-data actor test retains all previous source/save/state controls.
Its positive WeaponPistol fixture now requires the real Ammo class cast to
return null and the compiled InitializeInventory to reach required Spawn278 at
PC253, with full rollback. That updated assertion has not yet passed a current
original-data run. Returning None from Spawn would not satisfy the port goal.

```powershell
cmake -S desktop -B desktop/build
cmake --build desktop/build --parallel 4
ctest --test-dir desktop/build --output-on-failure
.\desktop\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex'
```

Generated package files, saves, logs and original commercial data are not
published in Git. Local batch evidence is under ignored
`artifacts/object-casts-20261009/`.
