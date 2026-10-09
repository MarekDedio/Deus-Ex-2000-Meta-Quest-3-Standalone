# Scoped script-state saves

Runtime checkpoint version 4 adds persistence for properties committed by the
bounded actor interpreter and its native animation clocks. Version 5 additionally
stores explicit portable state frames, typed locals and state-keyed disabled sets.
Version 6 additionally retains mutations of concrete, loaded Actor class-default
blocks, separately from actor instances. Version 7 retains native ordered
child/base lists, four touch-event flags and removal from the world registry.
Version 8 additionally preserves runtime-born actors, their actual loaded
classes, appended Level slots and frozen typed birth defaults.
None of these versions adds
automatic NPC startup, AI, live animation, or a general UnrealScript savegame.
Only the current map's supported authored and runtime-born actors are restored.

This document updates the earlier memory-only/save-refusal description in
[PORTABLE-SCRIPT-EXECUTION.md](PORTABLE-SCRIPT-EXECUTION.md) and
[ANIMATION-POSES.md](ANIMATION-POSES.md) for current-map checkpoint storage.
Map replacement and unload still refuse committed scoped script state because
there is no per-map archive yet. Saving it does not remove that travel guard.
Ordinary nested structs now use their original declarations for shape and
object/class constraints, including InventoryItem; see
[AUTHORED-STRUCTS.md](AUTHORED-STRUCTS.md). Version 8 can cold-restore a supported
born weapon/ammo graph; it does not establish a complete authored player inventory.

## Runtime envelope

The runtime checkpoint still starts with magic `0x53515844`. A checkpoint
without committed script objects or class defaults is written as version 3, with no new trailer.
A checkpoint containing only properties/clocks uses version 4; one with any
portable state object uses version 5. A checkpoint with class-default mutations
uses version 6, including when it has no actor records. Any native lifecycle
record selects version 7; a nonempty birth manifest selects version 8. The version word changes, the existing
version-3 fields retain their order and widths, and one trailer is appended.

```text
u32 magic, u32 version=4, 5, 6, 7 or 8
version-3 fields:
  inventory[], inactive actors[], activated actors[]
  f32 player health, damaged actors[(path,f32 health)]
  u32 credits, u32 skill points
  conversation flags[], goals[], notes[], applied-effect identities[]
u32 script blob length
script blob[exactly that many bytes]
```

All integers and floating-point bit patterns are explicitly little-endian.
Strings are `u32 byte length` followed by bytes, without a terminator; lists
begin with a `u32` count. The trailer must be nonempty and cover the exact
remaining runtime payload. Truncation, oversized lengths and trailing bytes
are errors. Versions 1, 2 and 3 remain readable through their existing field
sets; they cannot contain a script trailer. A v4 envelope must contain codec v1
without state records; a v5 envelope must contain codec v2 with a state record.
A v6 envelope must contain codec v3 and a nonempty class-default section.
A v7 envelope must contain codec v4 and at least one native lifecycle record.
A v8 envelope must contain codec v5 and a nonempty birth manifest.

This version number is distinct from the Persona/UI metadata version and from
the paired `.qsv` bundle format. The alternating-slot, checksum and durable
publication workflow described in [SAVE-RECOVERY.md](SAVE-RECOVERY.md) remains
the outer recovery mechanism. A runtime trailer is not authentication, and
the codec does not implement file I/O.

## Script blob schema

`native/quest_script_state.h` exposes `EncodeScriptSavedState` and
`DecodeScriptSavedState`. Its internal format starts with eight magic/version
bytes: `44 58 51 56 4d 53 01 00` (`DXQVMS`, version 1, reserved zero).
If any object has portable state, byte 6 is 2 instead of 1. Without such state,
the exact legacy v1 representation is retained. Class-default mutations select
byte 6 = 3; without them the v1/v2 representation remains unchanged. Native
lifecycle records select byte 6 = 4; otherwise v1-v3 bytes remain unchanged.
Births select byte 6 = 5; without births the v1-v4 representation is unchanged.

```text
mapName
u32 object count
objects[]:
  actor object path, actual class path
  u32 property count
  properties[]:
    fully-qualified property key, property name, u32 fixed-array index
    u8 value tag, typed value payload
  u8 hasClock (0 or 1)
  optional complete animation clock
  codec v2-v5: u8 hasState, optional portable state object
  codec v4/v5: u8 hasLifecycle, optional native lifecycle:
    u8 worldRemoved (0 or 1)
    u8 touchEventSent[4] (each 0 or 1)
    u32 child count, child actor paths[] in native order
    u32 based count, based actor paths[] in native order
codec v3-v5:
  u32 concrete class-default count (nonzero in v3; may be zero in v4/v5)
  class defaults[]:
    loaded script Actor class path
    u32 property count (nonzero)
    properties[]: same property key/name/index/value layout as above
codec v5:
  u32 birth count (nonzero)
  births[]:
    map-qualified object path, loaded concrete Actor class path
    u32 appended Level actor slot
    u32 frozen-default property count (may be zero)
    frozen defaults[]: same typed property layout as above
```

Born actors have no fabricated map export. Restore first registers the whole
symbolic birth graph, then validates actual class ancestry, reflected fields,
typed references and native links, including references between cold births.
Manifest slots must form the exact contiguous tail after the authored Level
slots, retaining original null holes. Abstract/nonactor classes, authored name
collisions, duplicate slots and missing initialized instance records fail.

Frozen defaults copy the concrete class's mutable patch at birth and share
immutable authored defaults. They are independent of later CDO mutations.
Instance overlays remain separate. Prepared actors are temporarily GC-rooted;
replacement registries and geometry are prepared before nonallocating
publication. Loads replay no startup/lifecycle callbacks. Versions 1-7 clear
the abandoned birth tail instead of merging it with the old timeline.
See [actor births](ACTOR-SPAWN.md) for allocation, rollback and GC controls.

Class-default records contain no animation clock or state frame. They are
canonical by case-insensitive class path and share the actor section's object,
property, value-node, depth and retained-byte budgets. Duplicate class paths or
property name/index aliases are rejected before materialization. Actual runtime
validation additionally requires a loaded script Actor UClass and its inherited
property declaration; an actor instance, asset or unrelated property is not a
valid default target. See [class-default semantics](CLASS-DEFAULTS.md).

Native link lists retain their order and duplicates, including relationships
created by reentrant callbacks. They are not regenerated from arbitrary
script-written Owner/Base fields. Runtime schema validation requires each
link to be an actual Actor in the saved map and the original Actor.Touching
four-slot ObjectProperty schema. World removal is separate from bDeleteMe:
scripts can clear that flag after Destroy without recreating the native slot.
Deleted UObject identities remain callable/readable until map teardown.
See [actor lifecycle](ACTOR-LIFECYCLE.md).

The v2 state object contains three booleans (HasStack, portable-frame override,
hasFrame), optional qualified running/local-owner paths, u32 next-statement ordinal,
u8 latent ordinal, counted typed fixed-array locals, and counted state-keyed
disabled-name sets. It stores no native pointers or package-local property indices.
The running code may differ from the same-named local declaration owner after an
inherited label jump. A false override preserves immutable raw dormant context
without fabricating native class locals; it requires no portable frame.

Structural codec validation represents all twelve pinned latent ordinals, retains
null/stopped stale PC, checks canonical case-insensitive ordering and duplicate
identity rejection, and bounds every count/value/tree before materialization.
Runtime preparation adds actual authored schema/ancestry/HasStack/typed-local and
PC checks; only Continue/Stop have supported restore behavior today. Continue
allows the exact end ordinal produced by a committed Return; next resume rejects
the absent statement. Disabled identities are folded on preparation so subsequent
Enable mutates the restored set. See [state execution](STATE-EXECUTION.md).

Value tags are stable wire identifiers, independent of `Vm::Kind` enum
ordinals and C++ structure layout:

| Tag | Value payload |
| --- | --- |
| 0 | Nothing; no payload |
| 1 | Byte; one byte |
| 2 | Int; signed 32-bit bit pattern |
| 3 | Bool; one byte, exactly 0 or 1 |
| 4 | Float; IEEE binary32 |
| 5 | Name; bounded byte string |
| 6 | Object; stable path string, not a pointer/package-local index |
| 7 | String; bounded byte string |
| 8 | Vector; three binary32 components |
| 9 | Rotator; three signed 32-bit components |
| 10 | Struct; counted `(field name, typed value)` members |

The codec preserves signed zero, concrete value kinds, string bytes and
original spelling. Empty/`None` names and empty object references are retained
as supplied; it does not guess missing references. Value strings preserve
original UE1 code-page bytes as well as UTF-8 bytes, rather than imposing a
new text conversion. Embedded NULs and non-finite numeric values are rejected.
Identity paths, keys, names, sequence names and struct fields are bounded
printable ASCII.

Objects sort by case-insensitive path. Properties sort by case-insensitive
fully-qualified key and array index; struct fields sort by case-insensitive
name. Duplicate identities and case collisions are rejected, not overwritten.
Separate fixed-array slots may share the same exact property key/name.
Decode also requires canonical order. Equivalent object/property iteration
orders therefore produce identical blob bytes; this is not a claim that the
legacy gameplay prefix's unordered collections are canonical.

## Budgets and allocation boundary

Generic codec defaults are:

| Limit | Default |
| --- | --- |
| Encoded bytes and estimated aggregate retained state | 32 MiB each |
| Actor identities and class-default records | 4,096 combined; birth plus overlay counts once |
| Properties | 65,536 total, including overlays, class and frozen birth defaults |
| Typed value nodes | 262,144 total |
| String bytes | 8,192 per string |
| Map name | 128 bytes, also within the string limit |
| Value depth | 32 levels, root at depth zero |
| State local declarations | 65,536 across objects |
| State local array elements | 262,144 across objects |
| Disabled-state sets | 65,536 across objects |
| Disabled-event names | 262,144 across objects |
| Native actor links | 65,536 across child and based lists |
| Level actor slot index | Below 1,000,000 |

Retained estimates include object/property arrays, nested map nodes, strings
and bounded sorting pointers. Encoding validates/counts the complete tree
before reserving the output. Decoding first uses a non-materializing pass with
string views; restored object/property/field containers are allocated only
after the complete payload passes limits and canonical uniqueness checks.

Runtime saves further limit the entire runtime payload to 16 MiB. The codec's
byte allowance is reduced to `16 MiB - legacy prefix bytes - 4`, so the blob
cannot evade the whole-file cap. Snapshot capture also checks live nested
values before copying them. Runtime schema inspection has its own metadata
budget of 64 MiB for cached table metadata and expanded paths, separate from
the serialized-value quota. Unsupported map-local classes in a different,
unloaded custom map fail closed; imported original campaign classes use the
initialized script reflection graph.

Persistent property/state/class-default/birth/native-link storage is measured cumulatively without copying before a
transaction and after allocating state mutations/at commit. This uses conservative
codec accounting with the 16 MiB runtime cap and aggregate state counts, so
independent calls cannot grow unbounded sets/locals/default values. Combined
retained bytes and typed value nodes are checked, not just encoded byte length.
The combined save capture
still accounts for gameplay prefix, object identities, properties and clocks;
its quota can reject a capture even within this state-only bound.

## Complete animation clock

Each optional clock retains all five channels: main plus four Deus Ex blends.
Each channel stores its sequence, normalized frame and previous native tween
history: vertex offsets 0/1 and interpolation fraction. This is necessary;
`AnimSequence` and `AnimFrame` alone cannot reconstruct an interrupted tween.

Main state stores rate, last frame, minimum rate, tween rate and old rate;
loop, notify and finished flags; and the FinishAnim waiting flag. Each blend
stores those five rates and all four `SimBlendAnim` components. The clock also
stores binary64 simulation time, byte RemoteRole and byte Fatness.

Clock numbers must be finite. Simulation time is nonnegative, last frames are
in `[0,1)`, tween rates are nonnegative, and channel frames cannot exceed one.
Negative tween frames and the transient native notify boundary at frame one
are preserved. Previous history fractions and offsets are not clamped or
validated against invented mesh data: the pinned native can capture wrapped
offsets with `fraction=NumFrames` at frame one, and original assets can have
dangling sequence declarations. Such finite metadata survives a save exactly;
the real mesh sampler still diagnoses invalid history or missing frame accesses
when drawing it. Persistence does not make an unusable pose drawable.

## Validation and application

Structural decoding is only the first gate. The runtime checks the blob's map
against supplied quicksave metadata, where available, then validates saved
identities against the user's read-only authored map/package tables. Checks
include actual actor class, fully-qualified property identity/name, fixed-array
index, exact concrete value kind, supported struct shape and object/class
reference constraints. Reflected animation properties must agree with their
saved clock. A structurally valid blob is not permission to write an arbitrary
actor, property or object reference.

Native asset UClasses such as Mesh and Texture can be synthesized by the pinned
engine and absent from disk script exports. Validation compares the authored
qualified class identity, follows available authored `ObjBase` links, and uses
only the exact pinned native Mesh/Primitive, Texture/Bitmap and Core class
hierarchies for these absent wrappers. It neither treats every missing class as
`Core.Class` nor accepts an unrelated actor in a Mesh property. Unknown native
hierarchies fail closed.

`ValidatePortableRuntimeState` is a read-only preflight for live gameplay,
property overlays, clocks and portable state. It can inspect the saved map's authored schema
while a different map is loaded; it does not replace the map or apply that
timeline. Schema caches may be populated during inspection. Born identities are
validated symbolically without allocating UObjects. Actual version-4 through version-8
application requires the matching authored map to be loaded and resolves all
targets and prepares all allocating containers before clearing live state.

A rejected/truncated/mismatched checkpoint leaves the current gameplay state,
property overlays, clocks and portable state intact. Capture collects and schema-validates its
state before writing, so validation/budget failure must not truncate an existing
checkpoint. After successful application, saved script state replaces the
current scoped overlays/clocks/state/class defaults/native lifecycle rather than merging abandoned timelines.
Version 8 also replaces births. Native collision membership is rebuilt from the
restored fields and Level order, as in the pinned saved-game LinkActorsToLevel;
cached/stale hash buckets are not serialized. This is not a physics save.
Read-only validation does not currently preflight collision resource work:
extreme typed geometry can pass schema validation but fail actual load
atomically, leaving the live timeline intact.
Every validated version-1 through version-5 load clears all mutable class
defaults, including omitted classes. Read-only validation never clears them.
Every successful version-1 through version-6 load clears the abandoned native
lifecycle timeline: child lists and touch flags reset, removed actors return,
and original Base membership is restored without callbacks. Version 7 replaces
native lists explicitly; omitted records likewise reset to the original map
baseline. All allocating preparation completes before publication. Only a
successful mutation/load advances the runtime world revision.

A fully validated legacy version-1/2/3 load has no script blob and intentionally
clears scoped overlays, clocks, portable state objects and committed flags, returning those fields to
authored inheritance. Read-only legacy validation does not clear them.
Cross-map script references and arbitrary dynamic objects are not synthesized.

## Evidence and commands

The actor-birth update passes 4,480 structural codec controls, including 4,133
rejections; legacy codec1-4 byte fixtures remain unchanged. Generated actual-VM
Spawn tests pass 1,416 checks / 37 refusals, including cold symbolic graphs,
frozen defaults, legacy birth removal and GC at rollback/replacement boundaries.
The 50-entry host suite passes 47 ordinary tests; three original-data entries
skip by default and must be run separately with the user's installation.
Original runtime save/load and 996 original class-default/lifecycle checks
pass with this source. These are host results, not Quest GPU or campaign proof.

The earlier state-frame codec adds 1,251 checks, including 621 rejection
controls, for all twelve structural latent ordinals, code/local-owner identities,
fixed-array locals, disabled sets, malformed input and cumulative retained/count
budgets. The final-source host suite passes 30 ordinary tests with two separate
original-data tests skipped by default (32 entries total).

The separate original-runtime test with the explicit game root also passes the
new state-only v5 composition and paired v3/v5 save generations. It independently
walks the gameplay prefix and verifies exact canonical script-blob bytes, restored
gameplay semantics, read-only wrong-map rejection, legacy reset and guarded travel.
The unordered legacy gameplay prefix is not claimed to be byte-canonical. These
results are in ignored `artifacts/state-execution-20261008/ctest.log`,
`state-codec-test.log` and `original-runtime.log` (terminal exit code 0).

The final-source original actor integration also completed exit 0 in
`original-actors.log`: state-only/selected-frame/None/Stop/disabled-set roundtrips,
twelve new schema rejections, end-ordinal and mixed-case controls, and complete
payload rollback at unsupported original InventoryItem/AI behavior. Physics-only
v4 restoration and legacy clearing also pass. These fixtures do not run a full
NPC initialization or campaign.

The earlier property/clock-only evidence below remains a regression baseline.

The standalone structural codec was compiled with g++17, optimization and
`-Wall -Wextra -Wpedantic`, without compiler diagnostics. It passed 1,415 checks,
including 1,355 rejection controls: all value kinds, nested structs, every
clock field, bit-preserving round trips, deterministic order, every truncated
prefix, malformed tags/bools/counts, non-finite values, case collisions and
aggregate/depth/retained-byte budgets. Logs are in
`artifacts/headless-20261007/actor-visuals/script-state-codec-build.log` and
`script-state-codec-test.log`.

The separate original-data integration runs passed full roundtrips, read-only
different-map validation, wrong-map application refusal, legacy reset, four
positive authored reference controls (Mesh/Owner/PointRegion/Texture), and 15
malformed schema/trailer controls with byte-identical preserved live state.
The gameplay/progress regression also passed composed paired v3/v4 saves.
Logs are `script-save-reference-original-actors.log` and
`script-save-reference-original-runtime.log` in the same artifact directory.
To repeat after rebuilding the host targets, run:

```powershell
.\desktop\build\quest_script_state_test.exe
.\desktop\build\quest_state_frame_codec_test.exe
.\desktop\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex'
.\desktop\build\portable_runtime_state_test.exe --game-root 'D:\Steam\steamapps\common\Deus Ex'
```

The original-data tests use a user-owned installation read-only and temporary
checkpoints outside it. Without the explicit root, their original-data coverage
skips; a synthetic test pass is not an original-map restore pass. These
integration checks include unchanged version-3 saves, version-4 round trips,
full clock continuation comparisons, read-only validation, wrong-current-map
application refusal, malformed schema/trailer preservation and legacy reset.
Never fault-inject corruption into the user's only save.

## Remaining scope

This is scoped current-map persistence, not a per-map campaign archive. Travel
and unload still refuse committed scoped state; shutdown is not a substitute
for saving. Version 5 stores supported persistent state locals/code/PC/disabled
sets, but not VM call stacks, latent call continuations, timers, RNG or
event/animation residual elapsed debt. Version 8 adds the supported born-actor
graph; it does not serialize every possible native subsystem. The stored FinishAnim flag does not
implement a latent VM resumption engine.

Automatic authored startup, complete AI/physics/latent behavior and live
notify/AnimEnd scheduling remain unfinished. Explicit state control and event
eligibility are supported within bounded transactional APIs. The pure clock is not
hooked to live per-frame mesh uploads; original asset decoding and saved clock
fields do not prove NPC movement or animation. Quest stereo rendering,
controller/XR behavior, save/restore frame time and full campaign progression
still require implementation and physical verification. No headset access or
device test is part of the standalone codec evidence above.
