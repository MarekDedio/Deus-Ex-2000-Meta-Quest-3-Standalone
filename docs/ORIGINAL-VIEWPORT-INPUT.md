# Configured viewport Input construction and key queries (2026-10-27)

First Training remains the target. This phase supplies the genuine constructor
and key-query dependency for the original player/HUD path. It is not an account
login or character-creation interface, nor completed live VR weapon gameplay.

## Implemented runtime path

`PreparePortableRuntimeViewportInput` is an explicit standalone/non-editor
CPU-owner service. It requires a real live registered viewport, prepared player
wrappers, authenticated native Input schema, configured immutable User.ini and
actual Transient/lookup domains. It never constructs implicitly from a query.
An explicit editor load profile is refused rather than pretending to reproduce
the different editor constructor branch.

The real selected Input/InputExt UObject has its actual Class, Transient Outer,
concrete-class unique name and effective RF_Transient4000 flags. Configured
defaults are deep-copied before the original NoInit constructor steps. Actual
Actor.EInputKey is resolved through indexed Enum/Actor-Outer lookup, and FExec
is installed. InputExt reloads its CDO and then the instance with parent-first
config imports using the committed document and local invocation statistics.
Its52 Extension names are created/resolved in audited order and retained once.
Portable FNames have process lifetime; no unsupported original flag-storage
equivalence is claimed.

Viewport.Input is the owning GC edge. Constructor-only ownerViewport/client/
generation bookkeeping is separate from native Input.Viewport, which remains
null and initialized=false. No partial Init, ResetInput, platform update or
ReadInput success is inferred from constructor completion. Allocation, lookup,
counter, CDO reload, bootstrap and detached-result validation are staged before
nonthrowing publication. The weak inventory is not a GC root.

Read/query consumers validate current owner and lookup/client generation.
Revocation cannot become an empty valid result. Client cleanup retires Input
canonical/folded/weak lookup authority and breaks constructor ownership edges;
map unload clears Pawn pairing but retains viewport Input across reloads.
Transient Input/client/viewport identities are explicitly rejected from script
save references before generic non-map UObject acceptance. They are not Actor
births, inventory, map exports or serialized service handles.

Session objects, dynamic string/value/reference capacities, weak inventories
and old-plus-staged allocation/lookup peaks are metered. Session namespace
preparation transfers existing map nodes into preallocated replacement buckets
instead of cloning their strings. No retained-memory limit was raised.

`PlayerPawn.ConsoleCommand` preserves the original None-Player branch. A bound
call requires the actual Pawn/Viewport backedges and that viewport's own Input.
KEYNAME/KEYBINDING use the existing original parser/query protocol against live
Enum and binding storage. FNAME_Find does not intern unknown names. Enum spelling
reads authenticate the actual retained Name element, including original key239's
UK_ spelling. The explicit ordinary FString output-device profile appends CString
without a newline. Handled-empty remains distinct from an unknown command, which
refuses the still-unsupported viewport/Console fallback with its actual command.
The direct query diagnostic calls the same Input provider without inventing a
possessed Actor; unlike a complete UViewport.Exec it does not require one.

## Verification

Initial owned viewport integration passes10309 checks/20 refusals, including all
255 key-name and binding queries, exact empty/unhandled behavior, unknown-name
non-interning, cached config after source changes/deletion, two separate viewports,
constructor versus Init, detached snapshots, dormant first-Training save/map/GC
lifetime and revoke/clear/rebind retirement. Existing schema/config passes34355/
1357. The final compact-value viewport suite passes10310/20, VM suite696215/900,
and final desktop targets/Android-native/strict GNU+ARM64 checks pass.

The first actual two-Fire discovery retained original crate destruction and nine
births with whole-root undo, but exposed an overconservative fixed112KiB query
scratch reservation at147 HUD windows. The provider now admits a bound derived
from actual command/Enum/binding storage, including the supported GNU/libc++
capacity-growth and transient old/new-buffer profile, while preserving the
existing memory cap. This is an explicit bundled-library growth bound, not a
claim about every C++ standard-library implementation. No unsupported service
is bypassed. The earlier budget refusal is retained as diagnostic evidence,
not successful startup.

The growth-corrected run advances beyond the first query to238488 instructions/
27032 writes, but exposes real storage pressure at149 windows in DeusExHUD.InitWindow
PC220. It still retains original two attack notifications/four traces/two crate
changes/nine births and full undo. Lazy native window payload/before-image
storage advances this to151 windows, but the original HUD still requires more
memory. These refusals are NOT accepted final Training regressions or playable
startup.

The resource repair now also removes the eager native INT-array vector container
from scalar VM Values. Whole native arrays retain deep-copy semantics, moved
reserved capacity and ordered full-width values; actual allocated vector
containers are explicitly charged in VM, codec, runtime, capture and teardown
budgets. Native decal ownership transfers explicitly extract/move the vector.
No script indexing/comparison service or save wire format was added/changed.
Scalar Values physically need only an owning pointer for that absent payload,
instead of an inline vector. Exact-boundary tests include allocated container
size rather than making native arrays artificially cheaper. The final checks
pass with the memory limit unchanged. New array tests cover
no-container scalars, absent/zero-reserved arrays, deep copies/mutable independence,
reserved-empty move/clear, extraction/factory transfer and exact single/nested
byte-budget boundaries including allocated vector-container storage.

The compact-value actual Training two-Fire discovery advances to239566
instructions/27664 writes in DeusEx.HUDLogDisplay.CreateControls atPC110. It
now explicitly refuses the unimplemented original TextLogWindow subclass Init,
not FExec key queries or memory admission. It retains48 observations/two returned
attack notifications/four traces/two crate changes/nine births and whole-root
undo. This is a genuine newly reached native service, not completed HUD/startup.
Both final cold/warm repetitions reach that identical boundary with identical
instruction/write counts and whole-root rollback. Normal repeats retain239200/
19364 and the genuine ReadInput refusal. Door repeats retain239536/19432,
both56-unit door moves/two ends and the same ReadInput refusal.

Full final desktop build and Android-native link pass, as do strict GNU VM-test
and ARM64 runtime checks. Final CTest registers261:219 executed,42 explicit
owned-data skips,zero failures73.06s. One generated decal refusal test required
an ABI-independent limit: sizeof(Value)-1 replaces the old128-byte constant,
which the newly compact Value legitimately fit. The refusal and full rollback
assertions remain intact. Separate owned crate415/5,deferred429/21,
animation588/24,classes10413/38,FastTrace252/0,first-Training material checks and
1232 door/save route checks pass. The final audit confirms45 original files and
21 frozen compiler inputs unchanged by length/mtime/SHA256, with all compiler,
test and probe jobs terminal.

Ignored evidence is in artifacts/viewport-input-final-*-20261027.log,
viewport-input-compact-values-{owned,schema,vm-test,android-native,strict-*}
logs, and viewport-input-final-frozen-inputs-20261027.json. Earlier failed
budget-discovery/CTest logs remain separate from the passing final-fixed CTest
log. No unsupported boundary is counted as successful or playable startup.

Read-only original bytecode evidence also identifies the next likely FExec command:
DeusExRootWindow.InitWindow calls ConditionalBindMultiplayerKeys. Its original
255-key scan uses KEYNAME/KEYBINDING, then assigns absent multiplayer commands
with `SET InputExt T Talk` (and Y/B/K/Equals counterparts). Owned User.ini has
these keys empty and no existing matching commands. This path must be confirmed
after TextLogWindow and any earlier native dependencies and implemented with authentic journaled SET/config
effects, not a fabricated handled-empty response. IsKeyAssigned uses original
Console.EInputKey; the native Input provider correctly uses Actor.EInputKey.

## Remaining original services

The newly reached TextLogWindow.Init and immediate text/layout dependencies are
audited in ORIGINAL-TEXTLOG-STARTUP.md; the audit does not implement them.
Actual platform UpdateInput, InputExt.Process/ReadInput, Init/ResetInput,
ResetKeyboard/ResetConfig, post-Possess/travel callbacks and a committing live
VR pickup/equip/weapon/frame/render-publication bridge remain required. The
original Training diagnostic explicitly constructs Input before startup, but
does not publish a partially initialized player. No new APK is packaged yet.
