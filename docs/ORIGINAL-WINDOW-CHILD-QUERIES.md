# Original Window child queries and client-object link (2026-10-29)

Static source contract for the actual next Training startup dependency. This
document does not claim implementation or completed startup. Commercial DLLs
and EXEs are never loaded or executed; the port's package inspector and static
PE disassembler supplied the evidence.

## Original declarations and wrappers

Extension.Window GetTopChild1455, GetBottomChild1456, GetHigherSibling1457 and
GetLowerSibling1458 each have flags401/logical5: optional Bool bVisibleOnly90
and Object ReturnValue580 constrained to Extension.Window, dimension1. The
NativeParm program consumes the actual authored input declaration.

Wrappers10052D40/10052DB0/10052E20/10052E90 initialize the Bool temporary to
TRUE1, Step/copy the optional expression, consume EndFunctionParms and return
the actual Window pointer. Omitted and NoWrite input must preserve TRUE, not
the Bool declaration's false zero. Inputs are caller-frame copies, not aliases
that can be reread after later evaluation.

GetTopChild1004C6B0 starts at lastChild+140; GetBottomChild1004C740 starts at
firstChild+13C. GetLowerSibling1004C650 follows prevSibling+144; higher1004C680
follows nextSibling+148. With visibleOnly false, return that exact first link.
Otherwise skip candidates whose LOCAL bIsVisible bit0 at+30 is false. Do not
test effective ancestry, initialization, sensitivity, selectability or layout.
An empty/exhausted real chain returns None; an unavailable provider is not an
empty chain. Resource/cycle controls must fail explicitly rather than fabricate
an exhausted chain.

SetClientObject1487 is flags401/logical5, one Object newClientObject80 constrained
to Core.Object and no result. Wrapper100525E0 seeds ObjectNone0, Steps/copies,
then writes actual clientObject+118 at10052618. Native SetClientObject10003390
is also a direct pointer store. No ownership transfer, tree reparenting,
destruction, reconfiguration or callback is part of this method.
The actual clientObject field is ObjectProperty, dimension1, flags2, Core.Object
reference, base0, outer Window. NativeParm refs are313/316/319/322 for the four
queries and402 for SetClientObject; sourceText and function header base are empty.

## Actual script order

DeusEx.HUDActiveAugsBorder.ClearAugmentationDisplay84 bytes executes:

1. PC15 GetTopChild() on winIcons, with default visibleOnly TRUE.
2. While currentWindow is nonnull, PC38 Hide1414, PC50 SetClientObject(None).
3. PC69 reads currentWindow.GetLowerSibling() AFTER those operations/callbacks.
4. PC75 stores iconCount0 after the chain is exhausted.

This is a live walk, not a captured vector. Hide does not destroy, unlink or
reorder the icon. Existing LinkNewWindowChild appends at lastChild/top and
Lower1412 prepends at firstChild/bottom; actual traversal links differ from
immutable UObject Outer and parentOwner. Preserve all of these identities.

## Integration and verification requirements

Authenticate each original function/field schema and source. Queries must read
actual constructed native Window links, using the existing shared work budget.
Route SetClientObject through NativeWindowWrite/Execution.WriteReference so
Touch beforeimages and nativeWindowPropertyRoots retain/undo its exact GC edge.
Do not append a permanent reference or write an arbitrary script overlay.

VM input-copy/seeding coverage must include optional/default/NoWrite BoolTRUE
and initialized ObjectNone. Owned tests should cover top/bottom/higher/lower,
mixed local visibility beneath a hidden parent, explicit false, live reads
after Hide and None store, nonempty and empty chains, exact Object identity,
invalid receiver/declaration/input and bounded refusal/full rollback. Passing
the real empty initial HUD path alone is not proof of nonempty traversal or
completed original Training startup.
