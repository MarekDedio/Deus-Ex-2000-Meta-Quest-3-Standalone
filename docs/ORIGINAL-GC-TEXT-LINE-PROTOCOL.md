# Original GC text-line protocol: static implementation evidence

2026-10-27. This documents the next genuine TextWindow preferred-size dependency,
not an implemented text renderer, completed player startup or APK acceptance.
Only first Training integration is in scope. The original Extension.dll was
inspected as data with GNU objdump; no commercial executable/DLL was executed
or loaded as code. Its SHA256 is recorded in ORIGINAL-TEXTWINDOW-PREFERRED-SIZE.md.

## Units, state and entry points

The retail character scanner100294D0 consumes UTF16 code units between a current
pointer and an exclusive end pointer. Font glyph lookup is per code unit, not
UTF8 byte, Unicode scalar or normalized character. Existing UnrealString Decode
and CString supply transport/first-NUL semantics, not the layout algorithm.

The native text state copied by10027F70 is exactly20 bytes: four separate byte
flags followed by four raw Float plane words. Observed first-byte behavior is
bold; the second selects an explicit text color; the third records a consumed
linefeed; the fourth marks the next returned accelerator character. Do not
collapse the native state into a single persistent bold flag. Float plane W is
copied, but palette/hex RGB stores alone do not initialize it.

GetLine10029D80 takes the real text-state reference, copies that20-byte state by
value into10029AE0 and publishes selected output state/next pointer/length/width.
It returns the original line-start pointer only when the inner routine returns
true. This wrapper has a genuine null-input early return. The inner routine
uses signed/unsigned pointer differences as UTF16-unit offsets, not a count of
visible glyphs. A portable implementation must use bounded offsets instead of
reproducing raw Win32 addresses.

## GetNextChar100294D0

Each call clears only the state's accelerator byte before scanning. With
bParseMetachars false, an ordinary input code unit is returned and advances by
one unit. When the cursor reaches the exclusive end, the return is U+0000.
The final cursor is clamped to the end if a control path advanced beyond it.

With parsing enabled, U+007C introduces a control. The command's ASCII A-Z is
folded to a-z. An optional exclamation mark consumes one additional command unit
and selects the clearing/restoring variant. Recognized controls keep scanning
inside the same call until a character is returned or the end is reached:

- b sets bold true; !b sets it false. This is not a toggle.
- c invokes the actual RGB-hex parser; !c restores all four actual GC.textPlane
  Float words and clears the explicit-color byte.
- p followed by a digit0..7 writes the original eight-color palette's RGB,
  normalized using its actual binary64 reciprocal, and sets explicit color.
  !p restores all four GC.textPlane words and clears explicit color only when
  at least one unit remains after consuming p; it does not consume that next
  unit. A terminal |!p therefore does not restore, unlike terminal |!c or |!b.
- & marks the next returned character as an accelerator. Further controls in
  that same call do not clear this marker again.
- An unrecognized command returns that original code unit and consumes it;
  it does not emit the preceding pipe. Thus a doubled pipe returns one pipe.

For p without a following digit, the command has already been consumed. An
invalid next code unit is left to the next scan iteration and can become the
returned ordinary character. The exclamation prefix also precedes the command
dispatch; it is not a general text escape that discards all following text.

Trailing pipe/exclamation paths pass through original process-global command
scratch, but the cursor/end check skips command application and forces a zero
return. Apart from the per-call accelerator clear, no command is applied there.
Do not invent a visible trailing pipe or read outside bounded transport. A
portable state must separately account for originally uninitialized plane/
padding bytes rather than advertise their raw bits as deterministic retail data.

### RGB-hex parsing100292F0/100293D0

The channel parser starts its output byte at zero. It accepts ASCII0..9/a..f/A..F
as a high nibble, advances once, then accepts a low nibble and advances again.
Invalid high input does not advance. A valid high nibble followed by end or an
invalid low nibble leaves the stored high nibble and consumed first unit but
returns false. This is not atomic six-digit validation.

The RGB parser initializes three bytes to zero and attempts R, then G only if
R's parser succeeded, then B only if G succeeded. It stores all three normalized
Float outputs after those attempts, even if parsing was partial. The c scanner
path subsequently sets the explicit-color flag. Neither this parser nor palette
RGB assignment writes plane W. A permissive all-or-nothing replacement would
change input consumption, returned glyphs and wrapping boundaries.

## Inner line routine10029AE0: branch and rollback state

On entry, destWidth<=0 or unordered (x87 C0/C3) is replaced with exact binary32
500000, word0x48F42400. A positive width is not replaced. It remembers the
incoming linefeed flag, clears that flag in its local state and initializes
width, previous-whitespace width, overflow and first-character tracking.
The selected Font is actual normalFont or boldFont according to that state.

Before every GetNextChar call it snapshots the current cursor and full state.
Returned U+0000 or U+000A finishes the line. Whitespace boundaries use exactly
U+0009..U+000D or U+0020, after the separate U+000A check. Tabs, carriage returns,
vertical tabs and formfeeds are therefore not generic newline controls here;
their actual glyph widths can be measured. A space is not automatically a
fixed tab or skipped leading glyph.

It retains three distinct boundary records: the start of a whitespace run and
width before it; the start of the following word; and the full state before
that word. Leading whitespace starts with the at-whitespace flag already set.
When bold changes, it reselects the real Font before the glyph lookup. With a
None Font it does not add glyph width, but still advances first-character state.

For a non-None Font, candidate width is current stored Float plus the glyph's
signed integer USize. The comparison precedes the candidate's Float store:

- Equality/less/unordered does not overflow. An oversized first returned
  character is accepted, ensuring progress even when one glyph exceeds width.
- A later ordered overflow without a prior whitespace boundary restores the
  pre-character state and returns the previous width. The next cursor is that
  pre-character cursor, so the overflowing character is retried next line.
- With a prior whitespace boundary and a following word, it returns the width
  before that whitespace, restores the full word-start state and starts the
  next line at the word rather than publishing partially scanned word state.
- If overflowing within the whitespace run itself, it marks whitespace
  overflow and continues. At the next non-whitespace character it returns the
  pre-whitespace width/length while selecting that character's pre-scan cursor
  as the next cursor, restoring its pre-scan state.

At normal NUL/linefeed termination, whitespace-overflow similarly selects the
pre-whitespace width/length. Otherwise the line-length boundary is the cursor
saved **before the final GetNextChar call**, not its final advanced cursor.
Thus any trailing controls consumed by that call are excluded from the raw
returned segment, even though their state changes survive in the output state;
the terminating linefeed itself is likewise excluded. A consumed linefeed sets
the output linefeed byte and the next cursor is after it. The return is true
when the next cursor advanced or when the incoming linefeed flag was set. This
distinguishes an empty input from the final empty line after a terminal linefeed,
and ensures that the terminal empty line is produced only once.

Optional outputs are stored in original order: next cursor, full20-byte state,
UTF16-unit line length, then Float width. These are not interchangeable alias-
safe parallel assignments. The text-extent caller supplies distinct locals;
public/native adapters must preserve their authentic OUT evaluation behavior.

## Extent caller10027BB0

The caller computes the original CString end, starts bold/color/linefeed/
accelerator flags false and iterates GetLine. It then scans each returned raw
segment again with GetNextChar to measure actual maximum signed VSize, using
the Font selected by any bold changes and skipping linefeed for glyph height.
Crucially, GetLine publishes its selected **end-of-line state into the same
20-byte local used immediately by the second scan**. There is no restoration of
line-start state before rescanning. Separately, the selected Font register and
cached previous-bold byte persist from the preceding second scan (initially
normal Font/false). After a nonzero character is returned, they synchronize to
the current state's bold byte, before checking whether that character is LF.
A scan returning zero after controls does not synchronize that cached Font.
Consequently a prefix can be rescanned using the line-ending bold state, and a
controls-only/empty segment can use the preceding selected Font for its fallback.
For example, a line ending in |b can exclude that terminal control from its raw
segment while still starting the rescan with bold=true. These are literal
caller/state-store consequences, not permission to replace them with a cleaner
fresh-state measurement algorithm.
If that line's stored height compares below1 (x87 C0 includes unordered), a
nonnull selected Font supplies its space U+0020 height. It compares the line
width against accumulated maximum, adds actual textVSpacing and accumulates
height with the original Float store points. Outputs are X first, Y second.
There is no byte-width approximation or automatic extra horizontal pixel.

This is a static contract, not a tested replacement. Implementation must still
authenticate Font page/glyph authority, preserve per-page character counts,
use bounded work/memory admission for both scans and all snapshots, implement
real GC Push/Copy/Pop lifetime and whole-root rollback, and test malformed
controls, font-None/missing glyphs, whitespace/long-word/trailing-linefeed,
Float comparisons/store points and OUT aliases before admitting general sizing.

Evidence ranges re-read for this audit:100292F0..10029879,10029AE0..10029E40,
10027BB0..10027E44 and10027F70..10027FB5. Upstream UGC::FindTextBlocks/DrawText
uses a different parser/palette and is not substituted as retail authority.
