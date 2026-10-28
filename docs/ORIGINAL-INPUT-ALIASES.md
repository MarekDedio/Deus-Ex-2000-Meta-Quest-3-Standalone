# Original Input alias dispatch tail

Verified 2026-10-28. This completes the alias-selection tail of the actual
constructed Input provider, not the entire viewport command chain or live input.

## Original contract

Read-only static evidence is retained in ignored artifacts/input-alias-static-
evidence-20261028.log and the matching import-proof/core-import-proof logs. No
commercial executable or DLL was loaded or executed.

After BUTTON/PULSE/TOGGLE/AXIS/KEYNAME/KEYBINDING, UInput.Exec10394132..1039422A
checks the process-wide recursion guard at10588A44. A set guard declines before
token/name/alias reads. ParseToken256 uses UseEscape0; FNAME_Find never interns
an unknown name. The first matching current alias among40 slots is selected.
The guard is set around a void ExecInputCommands call, then cleared; a matched
alias returns true even when its stored command is empty or children decline.

The callback is NOT another call to Exec. The Exec receiver is Input's FExec
subobject at+28; virtual+7C is invoked on the adjusted full Input UObject.
InputExt vtable1005C3EC+7C contains10054624, an import thunk through100592C4
to Engine.UInput.ExecInputCommands. Engine thunk10302789 leads to10393BF0.

That separate body tests actual GIsRunning, parses256-unit lines with Exact0,
and routes by actual Action: Press1 goes to viewport Exec; Release3 sends only
ONRELEASE-prefixed lines there; other lines go to Input Exec. Child handled
results are ignored. It is not an unconditional recursive alias or Core SET
handler. Missing GIsRunning/Action/viewport/line dispatch authority must refuse.

## Implemented scope

quest_input_alias_tail.h reuses the audited ParseToken256 with escapes disabled,
the real current40-slot order, name-find semantics and a shared synchronous
recursion guard. RAII restores the guard on callback/budget exceptions. Runtime
queries read actual constructor-owned aliases and use the existing shared work
and allocation admission. Earlier recognized action commands explicitly refuse
their missing typed service rather than falling through to aliases/providers.

A matching runtime alias currently reaches an explicit ExecInputCommands
service refusal. This does not report handled success or invoke the wrong
virtual. Unknown SET can genuinely decline Input after consulting aliases, but
the remaining viewport providers must still execute/decline before Core SET.

## Verification

Standalone259 checks/52 refusals cover all40 positions, duplicates, token
truncation/quotes/NUL/newlines/backslashes, non-interning, shared guard ordering,
empty/unhandled child semantics, throwing callbacks and work/scratch cuts.
The owned first-Training viewport fixture passes10315/24, including all four
action refusals, a real configured alias reaching the exact service boundary,
key queries and unchanged save/lookup/revision/GC state.

Desktop, strict ARM64 and Android-native builds pass. CTest231 executed out of
278 registered,47 explicit owned-data skips,zero failures,141.72s. Separate
owned Canvas150/14,Console5148/26,HUD11225,Core SET9548/6,GC155/16,
metadata13896/125,schema34355/1357 and all eight first-Training regressions pass.
Original two-Fire and door repeat2 diagnostics retain their exact FExec SET and
ReadInput boundaries with complete undo; they are NOT successful startup or
published gameplay. All45 original files/642 frozen compiler inputs retain
length,UTCmtime and SHA256; all build/test jobs are terminal.

No new APK, live VR crate/loot/gravity completion, other-map integration,
account UI, accepted layout change or resource allowance increase is claimed.
Next is actual input-command/provider routing and initialized input, followed
by the committing VR weapon/world/render bridge.
