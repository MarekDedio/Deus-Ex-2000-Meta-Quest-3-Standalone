# Original held-item preparation (2026-10-15)

Status: CPU preparation and detached selection are tested; **not wired into the
Quest renderer yet**. The user reprioritized speech/quickbar comfort during this
work, so the current APK retains its existing generic hands.

`quest_held_item_assets.h` prepares the user's original right-hand-plus-item POV
assemblies off-thread. Keyring uses NanoKeyRingPOV, knife uses CombatKnife, and
crowbar uses Crowbar. All inherit actual PlayerViewScale1 from Engine.Inventory;
Skin/MultiSkins are actual None, with authored mesh texture fallback. Each asset
has its independently audited anatomical grip basis, original UVs/materials and
four explicitly derived rear-cuff closure faces. No commercial coordinates or
texture pixels are embedded in source or committed.

| Assembly | Original hand | Original item | Additional authored sleeve | Derived cuff |
| --- | ---: | ---: | ---: | ---: |
| Keyring | 152 | 60 | 0 | 4 |
| Knife | 152 | 114 | 0 | 4 |
| Crowbar | 152 | 80 | 16 | 4 |

The future renderer must replace the generic right hand with the complete
prepared assembly, not draw both. Cache original-size textures/GPU geometry
before gameplay; equipping must not read packages or allocate GPU resources.
Preserve the accepted OpenXR grip orientation without the legacy Glock rotation.

`quest_equipped_item_visual.h` selects only exact prepared class/mesh/scale and
effective material fingerprints against the current collected actor identity.
It distinguishes the native starter QuestVR.NanoKeyRing component from genuine
collected original UObjects. Missing/stale/ambiguous data returns no model.
Its optional melee profile preserves original typed damage/range units; it is
not yet used by the live trigger path, which still has older approximations.

Verification: strict builds and synthetic preparation32/8 rejection controls;
actual original-data preparation5935/26, rerun against the coherent host build;
detached equipment41520 controls. Nine original-data software views were
produced and inspected during preparation. These are not tracked Quest held-item
acceptance, attack-animation playback, physical swing detection or crate fixes.
