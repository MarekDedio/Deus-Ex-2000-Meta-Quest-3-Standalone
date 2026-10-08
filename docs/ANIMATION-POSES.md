# Original mesh animation poses

The shared CPU sampler retains original Deus Ex v68/lic0 vertex animations
instead of permanently drawing absolute frame zero. Quest uses it when building
initial actor geometry; desktop actor captures use the same code. This is pose
rendering, not a live animation engine or campaign completion.

## Data and sampling contract

- All signed-16-bit XYZ frame positions stay in immutable, shared per-asset
  storage. Mesh copies share these arrays rather than duplicating a character's
  complete animation set. Normals are derived only for frames needed by a pose.
- Sequence names, groups, start/count/rate, stable-sorted notifies, mapped
  render indices, direct normal topology and special attachment corners are
  retained. Existing static first-frame triangles remain unchanged for callers
  which explicitly use that cache.
- Original assets can have dangling authored sequence spans: Sword3rd declares
  three frames but stores one. The decoder preserves and labels this metadata
  instead of rejecting an otherwise usable static/material asset or inventing
  missing frames. Sampling checks every accessed frame, as the pinned renderer
  does; a missing required frame produces a diagnostic omission. An early pose
  with stored frames can still be sampled from a partly dangling span.
- Main sequence lookup is case-insensitive and falls back to the first authored
  sequence when absent/unknown. No sequence table means no mesh, as in the
  pinned renderer. Blend lookup never falls back to a different sequence.
- Normalized nonnegative frames interpolate consecutive sequence frames and
  wrap the second frame. Frames at/above one are rejected: the pinned renderer
  can extrapolate there, but its valid native clock keeps frames below one.
- Negative frames use native tween history (vertex-array offsets and fraction).
  The pinned initial zero-offset history is supported. A real transition's
  history cannot be reconstructed from saved `AnimFrame` alone.
- Fatness is `byte / 16 - 8`, neutral 128, and expands raw positions along their
  raw smoothed normals **before** interpolation, mesh Scale/Origin/RotOrigin,
  and actor placement. Four Deus Ex blend channels add deltas from absolute
  raw frame zero, not weighted cross-fades. Lod meshes also blend normal deltas;
  ordinary MeshDX does not. Attachments receive only the main channel.
- Lod normal construction uses direct wedge indices plus SpecialVerts, then
  drawing remaps both positions and normals. Normals use the pinned unit-face
  smoothing/finite-zero normalization and inverse-transpose transforms.

Per-asset retained bytes, per-pose temporary bytes, source vertices, animation
frames, sequences/notifies, distinct sampled frames and normal triangle work
have explicit bounds. Errors produce diagnostic omissions, not geometry from
partially decoded frames. These limits do not prove a 3 ms Quest frame budget;
pose preparation and memory must still be measured on the device.

`PortableActorSnapshot` reads inherited animation fields in the winning
property's source package, including explicit `None` and indexed blend values.
`bAnimByOwner` chooses the immediate live actor owner, while Fatness remains
the rendered actor's property. No guessed idle names are inserted.

## Offline checks

```powershell
.\desktop\build\quest_mesh_animation_test.exe
.\desktop\build\quest_mesh_animation_test.exe --game-root 'D:\Steam\steamapps\common\Deus Ex'
.\desktop\build\quest_actor_animation_snapshot_test.exe
.\desktop\build\quest_actor_animation_snapshot_test.exe 'D:\Steam\steamapps\common\Deus Ex'
```

The original-asset sampler test independently re-reads serialized frame,
sequence, notify, topology and remap data, then compares pose samples against
the pinned DX formulas/matrices. It is not a comparison to rendered frames
from the original Deus Ex executable. See [ACTOR-VISUAL-TESTING.md](ACTOR-VISUAL-TESTING.md)
for explicit isolated sequence/frame captures and their fixture labels.

Implementation references are the pinned SurrealEngine commit
`677ee14c5b83486e6634687953779aafb7973ad6`: `UMesh.cpp`, `ULodMesh.cpp`,
`UMesh.h::GetSequence`, `VisibleMesh.cpp`'s DX paths, `Math/vec.h`,
and `UActor_Animation.cpp`. This is a port contract against that implementation,
not proof of bit-exact behavior of the original closed-source Deus Ex DLL.

## Still required

The bounded portable interpreter can execute isolated original PlayWaiting and
Play/Loop/TweenAnimPivot helpers, including their actual native commands and
captured tween history. This is not automatically started NPC behavior.
Original ScriptedPawn initialization, states, ticking, event eligibility,
notify/AnimEnd dispatch and transitions still need integration before NPCs
animate and behave normally. A pure native main/blend clock has offline tests,
but live renderer updates, attachment rendering and general dynamic campaign
saves remain unfinished. Version-4 saves now preserve supported actor overlays
and complete clocks/tween histories; travel/unload still require a per-map
archive before they can discard scoped state. See [script-state saves](SCRIPT_STATE_SAVE.md).
See [PORTABLE-SCRIPT-EXECUTION.md](PORTABLE-SCRIPT-EXECUTION.md).
Quest currently samples the initial authored pose but does not animate it over
time. Desktop explicit pose overrides are fixtures, not simulated startup.
