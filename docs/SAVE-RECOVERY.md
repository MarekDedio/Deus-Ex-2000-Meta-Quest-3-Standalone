# Quicksave recovery and restoration

The Quest quicksave prefix is in the app's private `files/DeusEx` directory.
New saves publish one paired UI/runtime bundle into alternating
`quest-save-0.slot0.qsv` and `quest-save-0.slot1.qsv` files. The bundle carries a
generation number, bounded payload lengths, and checksums for its header and
both payloads. These checks detect corruption; they are not authentication.

Capturing or publishing a save never truncates the latest complete slot. The
inactive slot is replaced only after the staging file is durably written,
decoded, and compared with the intended bytes. A load considers complete slots
from newest to oldest. UI metadata and runtime bytes must come from the same
candidate. Runtime parsing is checked before a cross-map worker is queued, so
a malformed newer runtime can fall back without first replacing the map.

The old `quest-save-0.meta` and `.runtime` files are read-only fallback inputs;
new saves do not rewrite or delete them. The old two-file format has no shared
generation identity, so this migration cannot prove that an already-existing
legacy pair was originally saved together.

## Pose and parser limits

New metadata is version 5: its pose describes the map-local floor anchor and
head heading, rather than the world translation tied to a previous OpenXR
tracking origin. Restoration reconstructs the world pose from the current
tracked head position and heading. Versions 1 through 4 retain their legacy raw
tracking-space translation interpretation and its recentering limitation.

Metadata is limited to 64 KiB, map names to 255 bytes, dialogue keys to 1,024
bytes and 4,096 entries, and Persona history to twelve entries of at most 256
bytes each. Invalid lengths, embedded NUL text, duplicate dialogue keys,
nonfinite pose values, and trailing bytes reject the candidate before changing
UI state. Bundled runtime data is limited to 16 MiB and uses the shared runtime
parser for validation and application.

## Actors and conversations

Cross-map restoration first loads the authored map, decodes its actor meshes,
and prepares its complete actor texture array. Only then does it apply saved
inactive actors and capture visible actor snapshots. This keeps mesh/material
resources available if an older same-map save later reactivates a unique item.
The rollback path likewise decodes meshes before applying the private checkpoint.

A successful same-map load clears response choices from the abandoned timeline.
A map load also clears pending choices. Failed same-map runtime parsing leaves
the existing choices, UI history, and actor preparation untouched. The controller
quicksave action refuses to save while a response choice is pending; choice
queues themselves are not part of the snapshot.

Speech playback is cleared under the audio mutex when a restore succeeds or a
map load starts. An already-running MP3 decoder is invalidated using a
main-thread epoch, not destroyed or waited on during that operation. Its result
is consumed when ready and discarded if it belongs to an abandoned state. The
worker owns compressed bytes and does not consult mutable runtime objects.
Session teardown still joins outstanding workers before releasing session data.

The runtime snapshot currently models inventory, inactive/activated actor state,
actor/player health, credits, skill points, goals, notes, conversation flags and
applied-effect deduplication. It is not a serialization of arbitrary UnrealScript
locals, stacks, timers, or every campaign-system property.

## Remaining on-device checks

Host tests can check the codecs, durable journal operations, runtime restoration
primitives, map-pose transform math, and asynchronous result token invalidation.
The original-data asset-retention test exercised Training's unique HazMatSuit2
mesh (471 vertices) through hidden saved-map loading and older same-map
reactivation. Its retained material array has 43 decoded layers and one explicit
procedural FireTexture fallback. This is not proof of all actor visual fidelity.
They do not establish OpenXR/GPU/audio behavior. When the headset is available:

1. Save, move physically, turn, recenter, and load. Check restored map position
   and heading, including a new XR session and a cross-map restore.
2. Save before a conversation choice, open choices, then load the same map.
   Press A and verify that no abandoned response is confirmed or replayed.
3. Restore while a long spoken line is playing or decoding. Check that old speech
   stops and does not appear after the restored state becomes playable.
4. Save before a unique pickup, save again after collecting it, restore that
   map from elsewhere, then recover the older generation. Check that the item
   returns with its original mesh and materials, not a placeholder cube.
5. Confirm save/load status messages and measure worst-frame time during save,
   load, map preparation, and staged actor uploads.

Do not intentionally corrupt a user's only save for these checks. Recovery
fault-injection belongs in isolated test fixtures or disposable app save copies.
