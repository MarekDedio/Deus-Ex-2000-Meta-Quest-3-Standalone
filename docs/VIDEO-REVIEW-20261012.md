# Training headset video review (2026-10-12)

The user supplied `dev.deusex.questvr.smoketest-20261012-121538-0.mp4`,
a 53.33-second Quest recording. It is local test evidence, not a redistributable
game asset. Frames/contact sheets remain in ignored artifacts, not Git.

The user explicitly explains that this session starts from an existing save.
Its first position is **not evidence of an incorrect fresh spawn**.

## Observations and source correlation

- Jaime's portrait, original panel artwork and progressive text are visible
  during the opening and key instructions. This proves the remote panel is
  drawn on this intermediate build, not complete face-to-face conversation UI.
- Door leaves remain visible while opening. The same-session log reports the
  use of `DeusExMover17` at 12:16:03.744, then 141 motion-publication frames with
  two leaves (9.391ms mean, 10.468ms worst). This is not final performance
  acceptance, especially while screenshot readback or save/load occurs.
- Around 30 seconds into the recording, the actor bank is refreshed. The
  same-session log queues 35 targets at 12:16:08.214. The old implementation
  deletes all actor renderers/colliders before incrementally replacing them;
  the prior 29-actor refresh takes 153 frames/66 GPU chunks. Walking more than
  10m from a streaming center triggers this path. This explains visible object
  pop-out/reappearance; it is not proof of a corrupt map or mesh file.
- The object brackets are absent or almost invisible. Source review finds that
  the pinned SDK uploads only one vec4 despite the renderer's count of 16.
  The correction packs all strips into its supported four-matrix upload path.
  An additional selection fix accepts a controller ray starting inside an item.
- The window above the desk appears dark/opaque. Static BSP's baked renderer
  ignores surface blending flags even though the actor renderer already handles
  translucent/modulated materials. Correct original surface flags and draw
  order must be verified before claiming see-through glass is fixed.

## Required follow-up

The replacement implementation retains the old actor render/collision bank
until the new bank is complete; staging is never submitted. Each bank retains
its atlas generation, and scene/revision/atlas tokens gate the atomic swap.
Cancel/failure drops staging; map/session teardown explicitly retires both.
Gameplay remains gated during refresh so hidden/collected items cannot be used
through stale targets. A world-registry mover refresh keeps opened/partially
opened doors authoritative when streaming rebuilds their geometry.

Static BSP now recovers actual original UModel surface flags through the DXQS
sidecar. Transparent faces are split by surface, use original screen/modulated
blend rules, and sort with blended actor geometry after all opaque world/actor/
player geometry. This is not the original full BSP polygon visibility sorter;
intersecting transparent surfaces still need broader validation. The existing
25m actor residency radius and 512-actor cap are unchanged, not a full-campaign
visibility/streaming solution.

On the exact replacement APK, repeat the corridor-to-desk walk without loading
or changing maps. Check visible objects throughout the refresh, all four white
brackets while aiming, and the room behind the original glass. Then check both
directions on the stairs, key pickup/keyed doors, saves and inventory pause.
Host tests and this older recording do not accept the replacement APK.

## Replacement-build evidence

The signed replacement APK's exact hash/installation identity is recorded in
[QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md). Host CTest passes 64 ordinary tests
(70 registered, six opt-in skips), with separate original route1,184/stair26/
speech-art13 checks and the actual window1942 audit. A same-camera software
comparison shows the observation room/NPCs through the formerly black pane;
it does not validate Quest blending.

Process25187 reports valid tracking, steady72fps idle samples, two complete
retained-bank swaps, and a paired-door update cost of8.369ms mean/8.946ms worst
over130 frames. Larger loading/refresh/screenshot stalls still occur. The live
eye capture shows textured hands and TrainingCombat after a map change, not
the Training desk/window or an aimed target. The user's new walkthrough remains
the physical visual acceptance step; no replacement-build acceptance is inferred
from bank logs alone.
