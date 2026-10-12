# Native decal ownership and checkpoint integration

Added 2026-10-12. This implements runtime ownership, explicit port persistence
and original native Attach/Detach. Positive Attach now follows the split geometry
and reentrant primary mover callback path; generated integration passes 8,268
checks/143 refusal controls. Rendered native shadows and accepted campaign
startup remain unfinished. Original geometry authority remains
[DECAL-NATIVE-CONTRACT.md](DECAL-NATIVE-CONTRACT.md).

## Runtime contract

One map-owned GC object retains the actual native Level, root Model and surface
record owners. Owner strings alone are not GC edges. Mirror-only owners do not
add native geometry roots. Ordered records, whole relative quads, node-list
duplicates/empty unrestricted lists, mover rotations and owner SurfList mirrors
remain exact. A journal copies native state/roots before mutation; rollback
restores them before provisional actor identities disappear. Native-only changes
participate in the world revision and unsaved-map/travel guard.

Attach publishes a provisional owner-bearing primary record and the current
reflected SurfList before `SetBase(self.Owner,true)`. Completion preserves its
insertion position and captured mover rotation, while Texture USize, DrawScale,
raw MultiDecalLevel and later SurfList reads remain live after callbacks. Entry
clamping is not repeated. Pretrace RNG is visible to callbacks and their further
draws are not overwritten by a stale candidate seed. See
[DECAL-GEOMETRY-PLAN.md](DECAL-GEOMETRY-PLAN.md) for the phase API contract.

Primary completion tokens use a transient state domain and per-surface structural
serials. Same-surface structural mutation during callbacks invalidates the token
and explicitly refuses the transaction rather than using the original native
record pointer after invalidation. Copy/swap journaling retains these sidecars;
checkpoint import creates a new domain. Sidecars are not actor GC roots or wire
records; actual native Level/Model/record-owner references remain the roots.

The VM has a bounded, appended IntArray value kind. Runtime ArrayProperty support
requires actual Core.ArrayProperty/Core.IntProperty identities, an owned inner
and fixed ArrayDim1; ArrayDim is not a dynamic length. Authored tag9 payloads use
the pinned loader's CompactIndex count followed by signed little-endian INTs.
Native writes can update the original Const SurfList; script-facing Const-array
aliases are read-only. Generic dynamic-array indexing is deliberately unsupported:
the original Core.dll token0x10 body unconditionally asserts before its unusable
placeholder access. No modern clamp/grow semantics were invented.

Native-zero Engine.Decal.DetachDecal validates its actual declaration and original
SurfList schema. It processes the current reflected list in reverse, removes the
first matching owner record on each surface and pops even when no record exists.
Foreign records remain. An empty list does not create native state. The original
Decal.Destroyed script reaches this native and its superclass callback normally.

## Deliberate port checkpoint format

Codec10 / runtime envelope13 adds stable value tag11 for IntArray and an optional
native graph after the existing optional RNG section. Both array-only and graph
saves select this version; removing both retains earlier feature-selected bytes.
Older versions reset native geometry rather than merge it or replay Attach/RNG.
This is not original GOTY disk serialization: original native FDecal geometry is
not archived by the original model serializer.

Positive Attach does not change this native wire contract: graph codec 10 and
decal-only runtime envelope 13 remain unchanged. The later tutorial-mover
checkpoint wrapper can select envelope 14 separately; it does not serialize
primary completion tokens or change the native graph section.

Preflight proves the exact saved map/native Level/root Model and authored counts,
canonical Decal owner identities (including cold births), native Level bindings,
membership-node surface identities and reflected/mirrored SurfList agreement.
Legitimate missing/foreign native records are not repaired or normalized.
Staged GC objects and roots publish only in the existing nonthrowing restore
block. Loading does not project corners, regroup by Texture or consume RNG.

Budgets cover array element/capacity payloads, each graph aggregate, capture,
journal, mutation candidates, root buffers and a conservative128MiB combined
restore-staging bound. Schema metadata has its own64MiB cap. Property/Level
temporary decoding has a separate64MiB capacity-aware bound, including simultaneous
old/new record buffers during growth. Native Model scratch is conservatively
precharged at16x export bytes plus fixed handles before decoding. These are explicit
structural/payload/capacity bounds, not measurements of allocator overhead or RSS.
Transient state-domain and actual surface-serial capacity are included in retained,
journal and restore-staging accounting despite not being serialized.

## Verification

The historical ownership/Detach checkpoint baseline passed 57 ordinary CTest tests
(61 registered, four owned-data skips).
All four optional owned-data suites passed separately: runtime save/restore,
original defaults1002, PawnList67 and full actor controls across Training,
UNATCOIsland and Intro. Standalone strict VM677905/338 expected rejections,
codec7355/6482, authored-array110/79 and model/property450 refusals passed.
The generated inventory migration still rejects65,536 valid properties plus
four derived Planes and accepts65,532 under the unchanged16MiB cap; only its
fixture padding name was shortened to accommodate the appended VM vector.

The focused owned-data checkpoint/world-startup run completed successfully on
2026-10-12 (`artifacts/decal-runtime-20261012/original-decal-world.log`). Its actual
world startup refusal at that stage was PreBeginPlay slot756/JaimeReyes0 at
unsupported Engine.Decal.AttachDecal, with exact complete rollback. This was
separate from successful checkpoint injection/Detach controls, not accepted startup.
The integrated positive Attach now advances beyond that old gap; investigation of
the next probe limit is ongoing and does not establish accepted world startup.

Generated VM/codec/state/array tests cover independent literal wire fixtures,
ordered signed values, malformed counts, corruptions, exact budgets and atomic
rollback. The owned Training actor test creates real dormant Shadow births,
stages the original floor geometry in a checkpoint, cold-restores it, collects GC,
replaces it repeatedly, rejects forged symbolic graphs, and executes original
Detach/Destroyed with a forced post-native instruction failure. It checks exact
checkpoint bytes, foreign-record preservation, RNG, world revisions and complete
legacy reset. Checkpoint injection is test setup, not evidence that Attach works.

Current generated native Attach verification separately passes 8,268 checks/143
refusal controls in `artifacts/tutorial-mover-20261012/spawn-positive.log`. It
executes a finite floor and mover callbacks, checks exact callback-visible state,
post-callback radius/Multi/RNG and repeated record order, and verifies warm/cold
restore without replaying Attach, SetBase or RNG. Failure controls cover revoked
client lifetime, callback primary invalidation and failures after native geometry,
with exact checkpoint/revision/GC/external OUT rollback. These are host checks,
not rendered decal or complete playable-world acceptance.

```powershell
& 'D:\Android\Sdk\cmake\3.22.1\bin\cmake.exe' --build artifacts/prerequisites-20261009/build -j4
& 'D:\Android\Sdk\cmake\3.22.1\bin\ctest.exe' --test-dir artifacts/prerequisites-20261009/build --output-on-failure
& artifacts/prerequisites-20261009/build/portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex' --world-startup-only
```

Remaining: renderer publication,
UVs/modulation/visible clipping/mover deltas/LastRenderedTime; Quest visual and
performance acceptance. Original world/player startup and campaign Tick are still
unfinished. Every actual new APK is automatically installed data-preservingly
when the configured Quest3 is connected.

### Native client binding now implemented

The subsequent [native client integration](NATIVE-CLIENT-BINDING.md) implements
the owned registry, Quest target capture, pre-teardown revocation and entry
guards described below. Positive CPU Attach is now integrated on top of that
binding; player possession and rendered decal acceptance are separate.

The original entry guard checks native client presence and viewport count,
not possession, tracking or focus. The bridge must be engine-session-owned,
nonserialized and independent of map/decal lifetime. Quest has one logical stereo
viewport: register a value/token only after SessionInit has actual swapchains,
then bind on the worker after InitializePortableRuntime (which resets runtime
globals). Never dereference GL/OpenXR handles from that CPU worker. Preserve
registration through ordinary map/checkpoint changes, and do not restore it from
a save or infer it from a camera/player actor.

The SDK destroys framebuffers before the derived SessionEnd callback. Teardown
therefore needs dispatch revocation/worker join at the real ownership boundary,
not a late raw-framebuffer pointer cleanup. Tracking loss/focus changes and
xrEndSession STOPPING alone do not mean those owned render targets were destroyed.
Generated controls must cover absent/empty/registered clients and the exact
client-before-Texture-before-Multi clamp guard order, with no unintended RNG,
SurfList, graph or GC mutation. A registered viewport with no PlayerPawn is valid;
GetPlayerPawn/possession is a separate native binding. Unsupported downstream
Attach paths must remain explicit failures until genuinely implemented.
