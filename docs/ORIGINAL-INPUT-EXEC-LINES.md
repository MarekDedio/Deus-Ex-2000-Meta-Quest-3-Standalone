# Input.ExecInputCommands and ParseLine source contract (2026-11-01)

Read-only static audit for the next real input service. This document is NOT
an implemented parser/provider, completed Init/ReadInput, startup, frame or APK.
Only external objdump read original PE bytes; no commercial module executed.

Owned source SHA256:

- Core.dll: `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
- Engine.dll: `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.

## Exact selected entry points

Engine.UInput.ExecInputCommands thunk10302789 leads to10393BF0..10393CE5.
Engine IAT10599D98 imports actual Core.GIsRunning;10599DE4 imports the BUFFER
overload `ParseLine(TCHAR const**,TCHAR*,int,int)`, not FString ParseLine.
Core's exported buffer thunk10103125 leads to10147E60..10147F4C. The call at
10393C41 supplies a256-UTF16-unit local buffer and Exact0. The line buffer is
512 bytes plus the caller's real lexical frame; it is not a UTF8 byte buffer.
Engine IAT10599D8C selects ParseCommand. UTF16 literal1047F334 is `OnRelease`.

## ParseLine256 Exact0, instruction-derived algorithm

At entry output is empty; per-call gotStream, quoted and ignore flags start0.
The input cursor is the actual caller-owned stream, not a newly trimmed string.
For each current UTF16 unit:

1. NUL ends the scan. CR or LF ends it even inside quotes.
2. Decrement remaining capacity BEFORE processing the current unit. Stop if
   it becomes nonpositive. Starting256 admits at most255 SCANNED units, not
  255 output units; stripped comment text consumes the same allowance.
3. If not quoted, a current`//` pair sets ignore=1 for the rest of this call.
   No leading-whitespace requirement exists. A current unquoted`|` ends the
   scan before gotStream is set for that separator.
4. Toggle quoted iff the current unit is`"`. There is NO backslash-escape
   handling, and quote toggling still occurs while ignore=1. Quotes are copied
   normally while not ignoring; they are not removed by ParseLine.
5. Set gotStream=1. Copy the current unit only when ignore=0, then advance
   the input cursor by exactly one UTF16 unit. Repeat unless the next unit is
   NUL. The two comment slashes are not copied, but both are scanned/charged.

After leaving the scan, Exact0 advances through ALL consecutive CR, LF or`|`
units, regardless of the quoted flag at that point. It does not skip spaces,
tabs, the remainder of comments, or the remainder of a truncated line. Append
one NUL to output. Return true iff the resulting cursor is not at NUL OR
gotStream is true. An empty output is NOT synonymous with false.

Important capacity consequence: when the255th unit has been scanned and the
next unit is ordinary text, the cursor STAYS at that next unit. The following
ParseLine call starts there with fresh quote/comment flags. Long commands are
therefore split into subsequent chunks, not silently consumed as one truncated
command, joined, rejected, or skipped to the next physical line. Comment text
can also be split: a suffix after the capacity boundary has fresh ignore=0.
Reproducing original behavior does not authorize a higher root work allowance.

Examples below describe source behavior, not runtime-test results:

| Source | First output | First remaining stream | True? |
| --- | --- | --- | --- |
| empty | empty | empty | no |
| `A\|B` | `A` | `B` | yes |
| `\|B` | empty | `B` | yes |
| only CR/LF/pipes | empty | empty | no |
| `//comment` | empty | empty | yes |
| `A//comment\|B` | `A` | `B` | yes |
| `"A\|B"` | `"A\|B"` | empty | yes |
|256 ordinary units | first255 units | final unit | yes |

The FString overload10147F50 uses a4096-unit temporary and is NOT the selected
Input parser. Exact1 is also not selected: it copies pipes and comment markers,
but still stops at CR/LF/capacity; afterwards it consumes at most one CR and
then at most one LF, unlike Exact0's repeated CR/LF/pipe skipper. This audit
does not propose broadening the next helper to unneeded overloads.

## ExecInputCommands routing, order and live reads

The concrete body allocates its local frame then checks actual GIsRunning
ONCE,10393C0E..10393C26. Genuine false returns without parsing/dispatch. A child
callback later changing running does NOT cause this body to recheck it between
lines. Missing running authority must refuse; forcing false is not a provider.

While ParseLine256 Exact0 returns true:

1. Reset a local line pointer to the start of the current output buffer.
2. Reread Input.Action(+EA8),10393C5B. Do not capture one Action for the whole
   command string; a child can change it before a later line.
3. Action1: reread actual Input.Viewport(+EA0) and call that Viewport's FExec
   at+2C with the current line and the SAME output device,10393C90..10393CA1.
4. Action3: call actual ParseCommand(`OnRelease`) on the local line pointer.
   A match selects the viewport route with the advanced remainder, even if
   that remainder is empty. A mismatch selects Input FExec with ParseCommand's
   CURRENT pointer, not necessarily the buffer start. ParseCommand skips
   leading spaces/tabs even on mismatch, so this difference is observable.
5. Every other Action selects Input's own FExec at+28 with the unchanged line
   pointer,10393C81..10393C8C. There is no whole-viewport fallback for that
   route. Action3's non-OnRelease line also uses this same Input route.
6. Ignore each child Exec's handled result and parse the next line. The native
   operation is void; an alias match cannot use a child's false to decline.

All original empty-line returns also enter that Action/dispatch sequence. Do
not trim or skip empty/comment/whitespace lines as an optimization. OnRelease
recognition reuses the already audited Core ParseCommand10147A20..10147B42:
ASCII case-insensitive comparison, space/tab skipping, rejection when the next
unit is an ASCII letter or digit, but NOT rejection for underscore/punctuation.
Thus `OnReleaseX` mismatches while `OnRelease_X` matches with remainder`_X`.

## Current implementation seams and honest remaining providers

No reusable ParseLine was found in native/desktop or the vendored engine source;
existing CoreSetCommand.ParseToken256 has DIFFERENT overflow/quote/whitespace
semantics and cannot substitute. NativeInputKeyQueries.ParseCommand is the
appropriate existing OnRelease recognizer once shared work/admission is charged.

NativeInputQueryOperations.ExecInputCommands currently explicitly refuses the
missing service. The alias-tail coordinator already invokes that distinct
operation under its shared recursion guard; it must not be changed into another
Input.Exec call. Existing ExecuteNativeInputQuery is constructor-authenticated
and still refuses BUTTON/PULSE/TOGGLE/AXIS; it is not yet initialized Input.Exec.
ViewportExecAfterInput begins AFTER the Input offer, so using it directly for
the viewport branch would omit the original first Input offer.

A faithful next typed coordinator needs genuine running, live Action, actual
Input/Viewport FExec and output-device callbacks. All reached callbacks must
authenticate the current native payload/lifetime and borrow the same outer VM
root, write/undo journal, cumulative work and simultaneous scratch budget.
The512-byte line storage and recursive invocation frames remain admitted until
their lexical calls return. The alias's source command must have actual bounded
caller-owned storage; no invented command, neutral sample or dummy output sink
is permitted. Cursor advancement belongs to that invocation's local stream,
not the Quest observation-consumption cursor.

The runtime currently has no authenticated port GIsRunning lifetime state for
this service, and complete initialized Input action commands and viewport FExec
providers remain missing. A generated parser/coordinator test must be labeled
as such; it cannot complete the real Quest reconciliation merely by supplying
test callbacks that return true or false.
