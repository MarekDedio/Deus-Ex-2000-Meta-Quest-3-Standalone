# Original tracked held items (2026-10-15)

Status: original keyring/knife/crowbar assemblies are wired into the Quest
renderer, built and replace-installed with saves preserved. Exact-build Quest
captures show the keyring and crowbar in the tracked right hand; live selection
logs also resolve the knife. Physical alignment/comfort and the
human quiet-trigger check were subsequently accepted by the user ("everything
works"). Knife device capture remains absent. The user accepted
the HUD and resumed this work; trigger use no longer reveals or holds the bar.

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

The renderer replaces the generic right hand with the complete prepared
assembly, never both. Original-size textures/GPU geometry are cached during
initial preparation, not on equip. The accepted OpenXR right grip is applied
once, without the legacy Glock rotation, extra scale or PC camera offsets.
Stable GPU deques retain uniform pointers; partial preparation/upload failure
clears held resources only and keeps generic hands/body. Unsupported items
explicitly fall back to a generic hand rather than retaining the last weapon.

`quest_equipped_item_visual.h` selects only exact prepared class/mesh/scale and
effective material fingerprints against the current collected actor identity.
It distinguishes the native starter QuestVR.NanoKeyRing component from genuine
collected original UObjects. Missing/stale/ambiguous data returns no model.
The live melee trigger path now uses the typed base HitDamage and MaxRange,
converting range to meters once. It remains controller-ray trigger use, not
the original Fire/skills/state-machine pipeline or physical swing detection.
Firearms/ammo retain their earlier approximations and are not new held variants.

HeldVisualCache limits same-item reconciliation/metadata reads to10Hz, with
immediate invalidation for equip/cycle/keyring/pickup/consumption/load. External
script material/item changes are revisited at roughly10Hz plus frame scheduling.
Render reads only
detached selection/alive state and tracked poses, never the portable runtime.
Session teardown frees held GPU resources; ordinary map replacement retains
the prepared catalog. Tracking loss, loading, death and Inventory hide hands.

Verification: strict builds and synthetic preparation32/8 rejection controls;
actual original-data preparation5935/26, rerun against the coherent host build;
detached equipment41520 controls. Nine original-data software views were
produced and inspected during preparation. The expanded actual collected/equipped
Crowbar+CombatKnife contract passes2016 checks/16 refusals, including exact
prepared fingerprints, native ring identity, stale/consumed refusal and unchanged
source/checkpoint/GC state. Cache scheduling passes1007448 checks/42 refusals.
These are not attack-animation playback, physical swing detection or crate fixes.

The installed signed APK is recorded in QUEST-TEST-BUILD.md. Actual1680x1760
Quest frames inspected from process8708:
`artifacts/held-items-20261015/quest-held-live-20261015.bmp` shows the keyring;
`artifacts/held-items-20261015/quest-held-followup-20261015.bmp` shows the crowbar.
Both are original full hand-and-item assemblies, with the generic left hand
retained and no second generic right hand. HELDSTATE reports cacheReady1,
drawReady1/rightTracked1 for each. These views show rendering, not a complete
rotation/ergonomic acceptance pass. At idle HUDSTATE reports belt0/alpha0,
grip0/trigger0 with health1/compass1. Steady windows reach72fps/13.89ms;
diagnostic screenshot readback causes slow frames and is not performance proof.
