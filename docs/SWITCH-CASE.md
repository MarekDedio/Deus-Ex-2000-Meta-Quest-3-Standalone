# Original Switch/Case execution (2026-10-10)

The portable VM now executes original Switch `0x05` / Case `0x0a` control
flow in synchronous functions and explicit persistent state slices. This is
campaign interpreter work, not automatic level startup, a campaign playthrough
or newly accepted Quest gameplay. The installed seated-player APK is unchanged.

## Primary contract

The local SurrealEngine pin is
`677ee14c5b83486e6634687953779aafb7973ad6`. `VM/Bytecode.cpp` decodes the
Switch size byte and selector, and Case's absolute uint16 next offset; `0xffff`
means default with no expression. `VM/Frame.cpp::ProcessSwitch`,
`VM/ExpressionEvaluator.cpp` and `VM/ExpressionValue.h::IsEqual` provide the
control, alias and comparison contract. This is not original-DLL execution or
a closed-source renderer equivalence claim.

The selector expression is evaluated once, retaining its variable alias when
present. Only visited Case expressions execute. Comparison reloads the selector
after each Case expression, so that expression's writes can change the match.
The first match/default resumes after that label. Subsequent labels are no-ops,
not another search: authored fallthrough, explicit break Jumps, nested switches
and direct jumps to Case labels retain their normal statement behavior.

Comparison is selector-directed, not generic exact-kind equality or assignment
coercion. Byte/Int use numeric ToInt without uint8 wrapping; Float uses ToFloat.
Bool, Name, Object, Vector and Rotator require their corresponding typed value
or Nothing conversion. Name/object identities are case-insensitive, strings
remain exact and can compare with a Name's text or Nothing's empty string.
A Nothing selector matches only Nothing or a null Object, not every typed zero.
Generic Struct comparison remains explicitly unsupported because portable values
do not retain the original UStruct layout identity; arbitrary struct memory
cannot be inferred from a field-name map. No invented layout or success stub is
used. Unrepresentable struct-as-Vector/Rotator memory punning is also refused.

Top-level Switch requires an immediately following Case. Every non-default
Case link must resolve to a top-level Case boundary, never an operand or code
end. Cyclic links are charged to the shared instruction limit, not followed
indefinitely. Selector storage, case expressions, nested calls, native OUT
references and selected bodies share the existing aggregate budgets and root
transaction. A failure restores the enclosing effects, aliases and actor state.
Nested-expression Switch/Case is not an expression-language substitute.

## Persistent state behavior

Case search reads the current code and statement position, including changes
made while evaluating the selector. Each visited label advances the persistent
position before evaluating its expression. A match keeps a callback-selected
new position; a mismatch resolves the old label's next byte offset in the new
current program, rather than applying an old ordinal to different code.

Code and local declaration ownership stay distinct. Same-code relabeling does
not recreate locals. A-to-B-to-A recreation invalidates old state-local aliases;
using one fails with rollback rather than rebinding it to guessed new storage.
The existing live-iterator replacement rule still applies. A cleared selector
frame ends the slice; code cleared during Case evaluation is explicitly invalid.
Stop/unsupported latent state is not silently converted into Continue.

## Original head-turn fixture

Read-only original package inspection shows
`DeusEx.ScriptedPawn.PlayTurnHead`: Switch at logical PC52 with Byte selector;
Cases at59/86/113/140/167 for1/2/3/4/0, default at194, break target197.
The original property writes set `Engine.Pawn.AIAddViewRotation` to yaw
-5461/+5461, pitch +5461/-5461, or zero respectively. It first resets the
rotation, so unmatched direction255 also keeps zero.

The underlying Engine.Pawn helper retains its native1010 blend/timer effects.
The override's successful path reaches Return/Nothing at PC207/208: its typed
Bool return therefore remains false, despite the property/animation changes.
When the base helper refuses a same-sequence turn with an exhausted timer, the
override skips both its rotation reset and Switch. These are original-bytecode
effects, not hand-written head-direction selection or live NPC behavior.

The original fixture seeds the existing animTimer slot and a nonzero view
rotation in a typed portable checkpoint. It checks all five directions plus
default, exact rotation/sequence, unaffected main/other blend channels, the
actual false return, short-circuit, selected-body write-budget rollback and
current-session view/clock save roundtrips. It is not a process-restart test.

## Verification and remaining work

- Generated synchronous VM controls pass 420 checks / 58 rejection controls;
  the complete VM test passes 677,351 / 242. These cover selector-directed
  conversions, aliases, native snapshots, nested/fallthrough/direct-label flow,
  original-style compact-reference normalization, malformed topology, precise
  instruction/write/depth/retained limits and enclosing OUT/native/RNG rollback.
- Persistent-state controls pass 208 checks / 32 rejection controls, including
  retained-versus-new local values and an effectful replacement Case that must
  never be visited while an enclosing old iterator is live. The final host suite
  passes all 48 ordinary tests; three optional original-data entries skip in
  CTest and all three pass separately with the explicit game root.
- Original table-only structural analysis passes all 8,208 functions / 70,611
  top-level statements across the initialized 38-package installation, including
  262 Switches and 1,599 Cases in 241 functions. All 261 states / 1,502 classes
  retain their existing label checks. This audit refuses effect/callee/variable
  access and uses per-program VM limits; it is not execution-feasibility proof
  or an arbitrary hostile-package sanitizer.
- The full original actor integration passes across Training, UNATCO Island and
  Intro, including the head-turn controls above, existing cold material checks,
  inventory/AI/state graphs and byte-identical rejected-startup rollback. Separate
  original runtime-save and all 996 class-default/lifecycle controls also pass.
- The inspector accepts original StartUp/Standing states and both head-turn
  Functions, including real state child-function chains. Negative/foreign/cyclic
  children and oversized child payloads are refused, not silently truncated.
- The final ARM64 build and APK Signature Scheme v2 verification pass. The APK's
  16 ZIP entries contain no commercial packages. The signed local archive and
  SHA-256 are in [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md); it is not installed over
  the accepted seated-player build and has no new Quest visual/performance claim.

Local evidence remains ignored under `artifacts/switch-case-20261010/`:
`vm-switch-final.log`, `original-actor-final.log`, `original-runtime-final.log`,
`original-defaults-final.log`, `original-state-head-inspection-bounded-final.log`,
`state-switch-guard-final.log`, `ctest-final.log`,
`host-build-final.log`, `host-tool-state-final.log`, `host-state-guard-final.log`,
`android-build-final.log`, `apk-archive-final.json` and `apk-signature-final.log`.
Initial failures remain retained: the missing generated analysis identity,
live-iterator selector guard ordering, inspector Function-child traversal, stale
startup assertion and an extra scenario accidentally added to the wrong test
loop. They were corrected; no rejection expectation was relaxed.

Actual explicit StartUp now reaches `Core.Object.FRand` native195 at
`DeusEx.ScriptedPawn.StartUp:9` (opcode195), inside the argument to Sleep256.
It fails Unsupported with complete rollback, not simulated startup success.
World/player startup, RNG persistence, AI processing, latent handlers, live
animation, campaign progression, per-map dynamic saves and headset performance
remain unfinished.
