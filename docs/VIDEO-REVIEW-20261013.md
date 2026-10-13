# Training video review — 2026-10-13

Reviewed the user's dev.deusex.questvr.smoketest-20261013-072848-0.mp4
(96.307417s,1920x1080). Ignored extracted frames and a contact sheet are under
artifacts/video-review-20261013. The recording predates the changes described
below; it does not verify their on-device behavior.

## Observed and traced

- The observation-room glass reveals NPCs; the NPCs are visibly in reference
  poses. All18 authored Training ScriptedPawn records inherit AnimSequence
  Fighter, which none of their eight meshes contains. Sampling falls back to
  All/frame0. Quest has not run their original startup/PlayWaiting lifecycle
  or elapsed animation clock. Hardcoding a replacement sequence would not
  establish authored NPC control, patrols or conversations.
- The inventory shows three icons while details select hidden NanoKey0.
  Raw-list navigation included non-displayable/unplaced items whereas drawing
  correctly excluded them. Independent X/Y input also allowed diagonal page
  changes and lacked held repeat/tracking rearm. There was no icon ray selection.
- Entering another area queues a full nearby actor-bank rebuild. The old
  Update gate paused gameplay for its entire multi-frame duration even though
  a complete bank remained visible. Pickups also requested a complete rebuild.
  Coarse preparation, driver uploads and bank retirement can still cause
  individual long frames and require headset timing evidence.

## Changes under verification

Navigation now uses original-layout visible targets and stable actor identity,
with explicit empty/error/overflow states, dominant-axis repeat input and
right-trigger/controller-ray selection against the actual rendered panel.
Original reflected ItemName/Description strings replace technical names.
The unsupported equip branch no longer falsely announces EQUIPPED.

Spatial rebuilds retain active gameplay/collision within completed coverage.
Actor-owned visibility slots mask ordinary picked-up/dead actors in both active
and staged banks without geometry upload, while targets/audio are invalidated.
Emitting actors still require authoritative lighting refresh. Moving brushes
are rebased to current live poses before atomic publication. Authoritative
restoration/material replacement remains blocking, and save-load clears the
removal ledger only after successful state restoration.

The shader mask and uninterrupted streaming require acceptance on the exact
replacement APK. Pure ownership/navigation tests do not prove GPU behavior,
nor do average FPS samples prove the absence of stalls. NPC T-poses, actual
equipment activation and the full original HUD are not fixed by this batch.
See ORIGINAL-HUD-CONTRACT.md for the user's additional PC-reference requirements.

## Replacement-build observations

The user tested the first streaming/inventory APK (SHA256ACC35F28...) and reports
freezes basically fixed and inventory browsing possible. The exact installed
base.apk hash was verified. A captured Training corridor confirms a rendered
scene; this does not isolate GPU mask correctness. Live profiling still found
109.273ms all-mover rebase and116.01ms spatial selection. The final replacement
removes both repeated reflected snapshot reads by retaining authoritative/live
mover samples, with final-idle/save-restore cache controls. It awaits install
and headset timing because USB disconnected during the test.

The same user test identifies invisible selected weapons, refused damage on
breakable crates and floating stacked crates. Source confirms the attack
adapter only accepts Engine.Pawn. The stack cause must be checked against
authored mesh transforms, collision and physics before assuming gravity alone.
