# Next original startup dependency: Core.Object.GetEnum

Observed2026-10-27 after the original GC font/text packets and Window1460:
the dormant first-Training root reaches Core.Object.GetEnum PC0/opcode0,
`Unsupported runtime native 0 (Core.Object.GetEnum)`,15477 writes and198 native
objects. The actual caller is DeusExRootWindow.ConditionalBindMultiplayerKeys.
HUD UpdateSettings, scope construction/Lower and final root cursor selection
precede this call. It is an original key-binding dependency, not account or
character-creation UI. The root is still hidden and the diagnostic rolls back.

## Authored declaration and data-only retail evidence

Owned Core.u declares nativeIndex0,flags0x2401, with required E ObjectProperty80
constrained to Core.Object, required i IntProperty80 and ReturnValue
NameProperty580. The ten-byte native parameter body contains E then i. Native
zero is declaration-specific; do not route unrelated native-zero methods here.

The owned Core.dll export thunk10102194 jumps to1013E9A0. Static disassembly
is retained in ignored artifacts/core-get-enum-wrapper-static-20261027.txt.
No original DLL or executable was loaded or executed as code.

- E's local pointer starts None at1013E9C8 and is stepped with FFrame.Object.
- i's local signed INT starts0 at1013E9EB, after E's copied Step and before
  i's Step. The wrapper advances EndFunctionParms after both inputs.
- The result FName starts index0/None at1013EA12. A null E, a non-Enum object,
  a negative index or an index at/above the actual Enum name count returns
  None. ObjectProperty<Object> does not promise that E is an Enum.
- Valid bounds return the actual Enum name at the signed index from its own
  name array. There is no invented key-name list, byte wrapping or string
  conversion. The original class ancestry predicate identifies Core.Enum.

## Existing authority to reuse, not yet a native implementation

The runtime already owns bounded immutable PortableEnumDescriptor records,
populated from the real serialized exports by CacheRuntimeEnumDescriptor.
ReadPortableRuntimeEnum validates Core.Enum/native metadata, UStruct Outer,
current object indexes and actual source ownership. It is a public snapshot
reader, not permission to copy the entire name vector during a native call or
to construct metadata lazily. A caller-root native should borrow the same
authenticated descriptor and charge shared work/output storage.

Implement authenticated declaration-specific native-zero dispatch and argument
seeds, copying E and i independently before subsequent argument effects. Test
raw/declared/direct call boundaries, source-frame receiver semantics, signed
bounds, genuine non-Enum/None results, valid source names, NoWrite/zero-fill,
later mutation, budgets and enclosing rollback. Preserve other native-zero
dispatch and the explicit unsupported visible-root/input/FExec services.

Completion of this service alone is not successful original startup, live VR
weapon ownership, crate destruction, surviving loot or automatic gravity. The
remaining committing VR/frame/publication bridge is described in
TRAINING-FIRST-PRIORITY.md. First Training remains the immediate integration
scope; no other-map integration or replacement APK is claimed here.
