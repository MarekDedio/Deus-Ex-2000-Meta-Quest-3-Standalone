# Original ReadInput continuation: static audit (2026-11-02)

Read-only PE-as-data inspection during the physical-reconciliation source
freeze. No original DLL/EXE was loaded or executed. This is not an implemented
ReadInput provider, initialized-state publication or gameplay acceptance.

Owned Engine.dll SHA256 is
0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4.
Export index1449 RVA20EF jumps103020EF ->10394580 (UInput.ReadInput).
Export index1451 RVA3733 jumps10303733 ->10374990 (UViewport.ReadInput).
Artifacts quest-readinput-owned-input-20261102.log and
quest-readinput-owned-viewport-20261102.log retain disassembly read as data.

## UViewport.ReadInput10374990..10374A16

Requires its actual native configured Input at+60. Compare Float elapsed with
Double-1 at10429C58. Ordered and unequal calls actual virtual UpdateInput(0)
at+7C; equal OR unordered skips polling. Then always call configured actual
Input.ReadInput(elapsed, actual output-device global) at virtual+64. This is
not permission to repoll from the later post-input(-1) call.
Reload viewport.Input AFTER UpdateInput returns (103749F7); do not retain its
pre-poll pointer as the ReadInput receiver.

## UInput.ReadInput10394580..10394847

1. The imported process-running state is observed first; false returns without
   building its cache or processing/scaling inputs. Use the existing explicit
   original app-loop authority, not a hardcoded running bit.
2. Obtain the actual current Viewport.Actor Class. Original cache key/profile
   constructs the all-INPUT-property inherited field list, count then fill,
   flags4, before testing elapsed. Reuse the actual shared property cache and
   ordinary lifetime/authentication, not a list of guessed axis names.
3. Ordered elapsed !=-1 walks keys0..254 ascending, REREADING live Input.Keys
   for each slot; nonzero calls configured Process(key,Hold2,elapsed). A Root
   callback can clear later keys, so a detached held-key snapshot is wrong.
   Each Hold Process reloads the actual GLOBAL output device10599BBC, not the
   supplied ReadInput output argument (which is unused by this body).
4. For ordered elapsed !=-1 keep factor=Double20 / Float-elapsed in the source
   x87 extended profile. Double20 is1042E4E0 (bits4034000000000000). Equal or
   unordered keeps Double+0 from10429838. This is NOT factor20*elapsed.
5. Traverse cached fields in original order. Only actual FloatProperty IsA
   declarations are multiplied at their real Actor cell; reread current actual
   Viewport.Actor for the store as the body does. Use source numeric arithmetic
   and qualified VM stores/undo, not direct actor mirrors.
6. Release cache-use authority after the scaling pass. Whole-root failure must
   restore native input, source baseline/cursor and all script stores together.

Post-input elapsed=-1 multiplies by0, not unconditional assignment0. Preserve
IEEE behavior for NaN/Infinity/signed zero; do not replace it with neutral state.
The scaling factor stays in extended precision across the field loop, with
each current Float loaded and each result stored to Float independently.

## Consequence for the in-progress port mapping

The first candidate mapped a full left-stick excursion to150 Action4 units
independent of elapsed. Generated protocol tests pass, and Android compiled,
but this static finding shows that candidate is NOT movement-calibrated.
Before final runtime gates, weight the port axis contribution by the ACTUAL
poll elapsed. A rate of15000*elapsed, owned Speed2 and Action4*.01 produces
300*elapsed; original20/elapsed then matches digital full-forward magnitude6000
before later PlayerInput processing. Do not replay queued axes per render row.

Regular polling must receive the real CPU input-frame elapsed, not assume72Hz
or use a stale render-row period when several rows queued. Initial reset has
no ReadInput elapsed argument; explicitly use its newest observation period
for that reset-only adaptation. Pose/world aim conversion and handover from
legacy locomotion/fire remain independent required work.

Update: all first-candidate builds terminated successfully before source edits.
The elapsed correction, baseline/cursor consistency and empty-poll final lease
checks are now implemented and pass73840 generated checks/28 refusals. Regular
polling requires explicit elapsed; initial reset alone may select its newest
physical period. Corrected native builds and actual Training validation remain
pending. Initialized-state authentication and ReadInput itself are still absent.

Independent read-only review confirms these source details. It also confirms
the current physical helper must retain normalized analog LEVELS for empty
source polls before regular ReadInput acceptance: no new XR row is not tracking
loss. Re-emit the last admitted gated level at the new actual CPU elapsed, with
rows0/cursor unchanged; a valid loss row replaces levels with0 and source
revocation refuses. Both frozen2 builds terminated successfully before this
fix. Retained normalized levels and an actual-owner empty-poll diagnostic are
now implemented; generated tests pass73849/30. Final native/strict ARM64/owned
Training source/empty-poll gates pass; see QUEST-PHYSICAL-INPUT-RECONCILIATION.md.
Initialized-state publication and ReadInput itself remain missing.
