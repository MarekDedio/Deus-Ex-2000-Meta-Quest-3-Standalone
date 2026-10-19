# Training recording milestone

Current priority,2026-10-19: complete the entire first `00_Training` map before
other campaign-map integration/testing; see TRAINING-FIRST-PRIORITY.md.
The short recording route below is the earlier introductory checkpoint, not
the full current acceptance target. Neither the full tutorial nor original
campaign startup is claimed complete.

## First recording route

Start from a fresh Training session, not an older quick-save. The coordinates
below are the original map's Unreal units, not metres or room-tracking positions.

| Step | Original actor / location | Expected result |
| --- | --- | --- |
| Start | `PlayerStart1` at `(-1149.24, 825.844, -65.1035)` | Stable viewpoint, hands/body, textured corridor. Move forward into the corridor. |
| Instructor | `DataLinkTrigger23` at `(-811.617, 832.431, -75.0674)` | `DL_Start`: Jaime's Academy introduction, original speech79 and audio79. |
| Door instruction | `DataLinkTrigger24` at `(-309.724, 840.977, -78.4475)` | `DL_door`: original speech41/audio41. |
| First door | `DeusExMover16/17` at `(-220, 800, -64)` / `(-219.998, 863.999, -64)` | Point and press **A**. Both visible panels slide apart over two seconds, by Y `-56/+56`, rather than disappearing. |
| Key instruction | `DataLinkTrigger4` at `(385.213, 849.001, -148.447)` | `DL_key`: original speech52/audio52 explains the key on the desk. |
| Pickup | `NanoKey0` at `(415.188, 1072.24, -144.79)`, near `JaimeReyes0` | Point at the key and press **A**. The collected key has authored ID `Train`; inspect it in inventory. Jaime is at `(361.279, 1206.01, -53.1002)`. |
| Keyed door | `DeusExMover47/48` at `(352, 576, -64)` / `(416, 576, -64)` | The pair stays locked without the `Train` key. With it collected, **A** opens both panels by X `-56/+56` over two seconds. |

The instructor recordings remain unmodified, including references to the mouse
and right-clicking. In VR, use **A** for the corresponding pointed interaction.
Remote instructor speech uses the original InfoLink panel, speaker photograph
and bitmap fonts. It is independent of the blue diagnostic panel, so hiding
that panel must not hide the instructor's line. Face-to-face cinematic dialogue
UI remains a separate unfinished path; see [DIALOGUE-UI.md](DIALOGUE-UI.md).
The introduction starts ahead of spawn because its original trigger is in the
corridor, not at the player start.
Contacts crossed during another transmission enter a bounded 16-line queue.
The queue plays in order after the current voice/subtitle finishes. It resets
on map/session replacement and quick-load; pending speech itself is not yet
part of save persistence. For a clean recording, allow each instruction to
finish before saving or changing maps.
The recording route relies on the named contact datalinks, not on repeatedly
using the NPC. This map explicitly gives `JaimeReyes0` an empty `BindName`;
direct NPC-use conversations are not yet verified as the original tutorial's
correct conversation flow.

## Controls for the recording

- Left stick: move; right stick: snap turn.
- **A**: use/pick up the pointed actor.
- Left grip: hide/show the blue debug panel. Hide it before recording.
- Left **Menu**: open/close the paused original-art inventory screen.
- In inventory: right-stick up/down selects items; left/right changes pages;
  **A** equips/uses the selected item; **B** or **Menu** returns to play.
- Right grip: cycle the selected inventory item while playing.
- **Y**: quick-save; **X**: quick-load.
- Avoid **B** while playing: it is still the diagnostic next-map shortcut.

## What this implementation does

The bounded VR interaction layer reads original mover key positions, rotations,
timings, glide mode, explicit paired tags and matching NanoKey IDs. Door brushes
remain visible and collidable. Closing doors can reverse on a player block.
Supported movement progress is saved and restored, including a partially open
door. Timed movers pause when the inventory is open.

Named instructor lookup accepts only an exact authored single-speech-to-End
datalink. It does not substitute a random Jaime line. The three introductory
lines above fit this subset. More complex conversation graphs are refused:
for example, `DL_CombatEntry` also triggers the combat entrance, so dropping
that event and playing only its speech would break progression.

These are explicitly scoped VR interactions, not execution of the complete
original Mover state machine, player possession or world startup. Matching-key
door use is a VR convenience; the original game separately uses the key ring
to toggle the lock. Automatic NPC AI and the full tutorial are not finished.

## Acceptance checklist

Host evidence (2026-10-12): 1,184 original-data route checks pass, alongside the
bounded mover primitive and save-regression controls. A desktop closed-door
view shows original textured panels and static lighting, with zero missing
actor materials in that preview. These results do not check live Quest motion.
The first checkpoint's user test confirmed the introduction and door opening,
but exposed a visible opening-animation FPS drop and broken stairs. Follow-up
changes add original speech-panel portraits/artwork, interaction brackets,
footprint-based stair sweeps and changed-mover-only publication. The shared
stair tests traverse six real Training/UNATCOHQ routes in both directions; they
are not headset acceptance. See [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md) for
the exact installed checkpoint and remaining physical checks. The route is not
yet accepted as recording-ready.

Complete this from a fresh start on the exact APK intended for recording.
Desktop data/logic checks alone do not establish headset visual acceptance.

- [ ] Walk from the original start without debug teleporting or map shortcuts.
- [ ] Hear each introductory line once, with the correct readable subtitle.
- [ ] Hide the debug panel; verify subtitles remain visible.
- [ ] See Jaime's original portrait, panel artwork and readable progressive text.
- [ ] Aim at a door/item: white focus brackets follow the same target used by A.
- [ ] Walk from the corridor to the desk without nearby objects vanishing during refresh.
- [ ] See the room behind the desk window through the original glass material.
- [ ] Open and pass the first door; both panels move smoothly and remain visible.
- [ ] Walk up and down the lobby stairs without stopping at risers or clipping.
- [ ] Verify the keyed door is locked before collecting the desk key.
- [ ] Pick up the key, inspect the original-style inventory, then open the keyed pair.
- [ ] Save halfway through a door's movement; load and verify its pose and continuation.
- [ ] Open inventory during movement; the world pauses and resumes without a jump.
- [ ] Check hands/body, nearby NPC/objects, lighting and doorway collision; no cube substitutes or disappearing brushes.
- [ ] Record the route on Quest and inspect the actual video for missing world/UI/audio and comfort issues.

## Next tutorial gates

After this short route is reliable, expand coverage to keypad/datacube access,
multitools and lockpicks, movable/destructible crates, crouching/jumping, ladders,
swimming, medical/repair equipment and other item-specific interactions.
`Mission00` also has real flags and scripted progression; they must not be
replaced by simply opening every door.

The authored exits lead from Training to `00_TrainingCombat#combat`, then to
`00_TrainingFinal#stealth`. Combat still needs its conversations and event gates,
weapons/targets, NPC AI, demolition tasks and inventory transfers; Final needs
stealth/detection and its own scripted progression. A successful introductory
recording is therefore a useful milestone, not completion of all three maps.
Those later-map integrations/tests are deferred until the first Training map
fully works; the exit names here are source context, not the current work order.
