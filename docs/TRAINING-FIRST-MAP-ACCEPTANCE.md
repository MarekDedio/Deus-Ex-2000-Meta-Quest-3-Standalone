# First Training: whole-map acceptance

Immediate content target: ONLY `Maps/00_Training.dx`. The opening corridor,
desk/key and recording route are a subset, not completion of this milestone.
Other-map integration/testing remains deferred. Shared regressions that protect
this map, saves or accepted VR features remain useful.

## Required exercises, not current completion claims

This minimum checklist joins actual first-map DataLinkTrigger tags to their
original Mission00 instructions, plus original actor gates and the explicitly
first-map branch of Mission00.Timer. It is not inferred from every conversation
in the shared Mission00 list, which also contains later-map content.

| Exercise | Original first-map authority | Required playable result |
| --- | --- | --- |
| Opening, desk and key | NanoKey0; Train-key movers47/48; opening route test | Original spawn, speech, smooth doors, keyring and locked/unlocked state |
| Breakable crates and lockpicks | DataLinkTrigger31/25; Containers tags Cratepick1/2 in Mission00.Timer | Held-weapon damage, distinguish invincible crates, real destruction/collectible contents, support/gravity, original DL_Lockpick/MS_CratesBroken progression |
| Code keypad | DataLinkTrigger7; Keypad0 validCode0012/Event paddoor; movers26/18 | Read the authored clue through the actual datacube/text link, enter code, original door event; reject wrong codes |
| Electronic bypass | DataLinkTrigger18; Keypad2/Event hackdoor; movers38/37 | Collect/use multitools, original hack progress/consumption and door event |
| Unconscious cadet | DataLinkTrigger6/32; Trigger0 Carcass proximity/Event bodydoorsopen; movers25/3 | Search for the real key, carry/drop the real body onto the medical table; original one-shot body gate, not a player-only trigger substitute |
| Darkness | DataLinkTrigger8/DL_dark | Usable light augmentation, energy/lighting feedback and navigable original room |
| Build steps | DataLinkTrigger27/DL_cratestack | Carry/drop permitted metal crates, stable collision/gravity, climb the resulting stack; do not let heavy wooden crates bypass authored restrictions |
| Platforms and pipes | DataLinkTrigger9/10; DL_part2/DL_jump | Jump, crouch, land and recover using the stairs without teleport/debug shortcuts |
| Ladder | DataLinkTrigger11/DL_ladder | Reach and climb the actual ladder with VR controls |
| Contaminated pool | DataLinkTrigger12/DL_swim | Swimming, disposable timed HazMat use, damage/air feedback and ramp exit |
| Medical and repair services | DataLinkTrigger20/19; DL_medbot/DL_repairbot | Real medical service and bioelectric recharge with working feedback/UI |
| Player information | DataLinkTrigger1/DL_F1F2 | Usable inventory/toolbelt, goals and notes while preserving comfortable accepted HUD |
| Scripted progression and endpoint | Mission00.Timer first-map branch; FlagTrigger0/1; Teleporter0 | Correct flags/timers, hologram departure and munitions hand-in; reach first-map exit readiness without opening every door or skipping events |

Numbers such as movers47/48 mean full identities
`00_Training.DeusExMover47`/`00_Training.DeusExMover48`.
The authored Teleporter0 destination names the next map, but loading/testing
that map is not needed for this first-map acceptance milestone.

The medical instruction advertises augmentation installation as a general bot
capability. It does not establish a mandatory canister-install exercise in this
map. Similarly, goals/notes are taught functionality, not all route-blocking
events. Exact cadet/key identity, datacube text association and obstacle surface
census still need complete static first-map enumeration before implementing
those individual gates; do not guess from names or old renderer-only lists.

## Verification needed for each implemented exercise

- Verify original scripts/actor state with the real first map; do not force
  begun-play, invent loot or use cosmetic falling as physics acceptance.
- Repeat from a fresh start without debug teleporting. Preserve original
  prerequisite/locked/failure paths and one-shot instruction/event ordering.
- Check save/reload and repeat interactions without duplicated loot, stale
  bases, detached visuals or resurrected one-shot triggers.
- Verify exact replacement APK on Quest: visible and usable held items, body/
  hands, NPCs, comfortable HUD/speech, lighting, collision and frame timing.

Existing `desktop/tutorial_route_test.cpp` covers the opening subset; it must
not be reported as the whole-map executable acceptance test.

## Static evidence provenance

The historical read-only package audit generator is
`artifacts/tutorial-route-20261012/route_audit.cpp`; its ignored evidence outputs
are `training-actors.log`, `training-conversations.log` and
`training-scripts.log` in that directory. The map output includes a filtered
actor selection, NOT a complete map census. Conversations are joined only by
actual first-map tags above. Mission00.Timer's later-map branches are excluded.
These source observations are not fresh gameplay completion evidence and must
be refreshed against current owned source before each exercise's acceptance.
No original commercial executable/DLL or other map needs to run for this audit.
