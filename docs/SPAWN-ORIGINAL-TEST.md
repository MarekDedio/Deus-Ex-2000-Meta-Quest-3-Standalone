# Original-map world placement differential

`desktop/quest_spawn_original_test.cpp` compares the bounded placement helper
with the actual pinned `OverlapAABBModel.cpp`, compiled verbatim from
SurrealEngine revision `677ee14c5b83486e6634687953779aafb7973ad6`.

It uses real `BspNode`, `Array`, `vec3`/`dvec3`, `BBox` and `CollisionHitList`
types. Only the query class name and its Model data carrier are renamed. The
carrier contains the original node and LeafHulls arrays; no fake UObject,
vtable, GC root, ULevel, actor allocation or gameplay lifecycle is constructed.
GNU builds must use `-fno-strict-aliasing`: the verbatim upstream query reads
packed int32 bbox words through `vec3*`. Static assertions check the real
vector's size/alignment. The production helper still uses safe `memcpy`.

`UActor_Phys.cpp::CheckLocation` and `OverlapTester` are **not linked**. A small
audited harness uses their world-only zero-extents bridge and 27-candidate
z/y/x grid (0,+1,-1, float max(radius,height)). Independent six-box fixtures
constrain every first-free candidate rather than merely trusting two equal
loops. Exhaustion, asymmetric extents, each collision-flag combination,
zero-size bypass and inclusive contact are checked too.

Before invoking the unsafe recursive pin, the decoded Model is fully checked
with the bounded helper's layout validation. An additional preflight limits
root recursion depth to 1,024 and worst-case unpruned visits to 1,000,000,
including repeated shared descendants. Memoized worst-case work includes each
node and both scans of every addressed hull, repeated along shared DAG paths;
a full 27-query group is capped to 16,000,000 units before invoking the pin.
A valid shallow 12-node DAG that amplifies a 1,000-plane hull is rejected in
preflight without executing the unsafe oracle. Malformed input never reaches the pin;
the separate `quest_spawn_placement_test` owns malformed/refusal controls.

## Optional original-data mode

No arguments runs synthetic controls, suitable for ordinary CTest. Supplying
the original installation root additionally reads:

```
quest_spawn_original_test.exe "D:\Steam\steamapps\common\Deus Ex"
```

It selects each ULevel's actual serialized root Model, not the first brush or
mover Model, in Training, Liberty Island and Intro. Samples comprise the first
64 explicit authored actor Locations in exact Level actor order and 27 spatial
grid points. All three root primitive bboxes have `IsValid=false`, so the grid
bbox is derived from that same root Model's authored points. Those points
choose sample positions only; rendered geometry never classifies collision.

Prospective dimensions and collision flags come from complete serialized class
default ancestry, with child overrides. Radius/height must actually occur in
the chain; absent bool tags retain UE zero-initialized false. The observed
classes are:

| Prospective class | Radius | Height | World | When placing |
| --- | ---: | ---: | --- | --- |
| DeusEx.WeaponPistol | 7 | 1 | false | false |
| DeusEx.Ammo10mm | 8.5 | 3.77 | false | false |
| DeusEx.Doctor | 20 | 47.5 | true | false |
| Engine.PlayerStart | 18 | 40 | false | true |

For every class/sample pair, the public placement helper's found status,
attempted-probe count and exact float chosen location match the pin-backed
harness. Every one of the 27 candidate hull results is separately compared,
even after the first free location and even when class flags bypass placement.
Map size/write time are checked before/after; all original files are opened
read-only. There are no map, save or headset changes or commercial asset exports.

## Verified result and scope

Manual strict C++20/O2/Wall/Wextra/Werror compilation against the frozen host
archive succeeds (including the GNU aliasing option). Both executions return
exit 0:

- Synthetic: 9,025 checks, 246 placement cases, 6,615 candidate hull comparisons.
- With originals: 167,997 checks, 1,338 placement cases and 36,099 candidate hull
  comparisons in total. Original-only contribution: 1,092 cases / 29,484 hull
  comparisons; 866 found, 226 exhausted and 137 shifted outcomes.
- Actual roots: Training Model36 (9,524 nodes / 40,878 hull words), Liberty
  Model189 (13,212 / 51,104), Intro Model165 (7,810 / 31,991).

Ignored final evidence is `compile-final-status.log`, `synthetic-final.log` and `original-final.log`
under `artifacts/spawn-original-20261009/`; an earlier `compile.log` retains a
failed preliminary linker attempt and is not counted as successful evidence.
This is sampled
world-hull equivalence to the local engine pin, not exhaustive original-game
collision equivalence, compiled CheckLocation equivalence, complete Spawn,
actor/mover collision, campaign startup, live OpenXR, stereo or Quest
performance proof. Native 278 remains unsupported.
