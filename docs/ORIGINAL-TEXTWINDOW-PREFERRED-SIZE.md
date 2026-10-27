# Original TextWindow preferred size: reached Training dependency

Read-only source contract,2026-10-27. This is not an implemented service or a
successful startup/headset claim. Only the first Training map is in scope.
No original commercial executable/DLL was executed or loaded as code.

## Actual reached call, not a guessed next boundary

`artifacts/textlog-snapshots-owned-hud-discovery-20261027.log` observes
`DeusEx.HUDLogDisplay.ParentRequestedPreferredSize` atPC129/native1464 with
165 pending objects/11767 writes. The actual Training two-Fire run in
`artifacts/textlog-snapshots-training-fire-20261027.log` reaches the same
`Native preferred-size virtual override is not implemented` error after
240624 instructions/28196 writes. The script operand is winLog and the input
is its computed logWidth. Its actual receiver is Extension.TextLogWindow.
Whole-root rollback diagnostics are not live controller/render publication.

The original script first queries winIcon, computes windowStart/logWidth and
then invokes winLog.QueryPreferredHeight1464; it subsequently adds logMargin
and applies the authored minimum-height expression. The host must return the
actual native query result, not substitute an arbitrary HUD height.

Native1464's existing query/cache protocol selects the receiver's native
preferred-size virtual. Static vtables1005B63C (TextLog) and1005A458 (Text)
both have slot+7C=100465A0, TextWindow.ParentRequestedPreferredSize. This is
not TextLog.ConfigurationChanged10047910, plain Window10050390 or Tile layout.

## Original input/output packet and fields

The virtual packet is `(Bool widthSpecified, Float& width, Bool heightSpecified,
Float& height)`. The actual script base event declaration has four dimension1
parameters: flags`0x80`, `0x180`, `0x80`, `0x180` respectively. An inherited
native override is not itself a substitute script declaration or a new native
index. Existing1463/1464/1465 wrapper/caching behavior remains separate.

Relevant authentic TextWindow fields: HAlign/VAlign Byte enum flags2,
hMargin/vMargin/MinWidth Float flags2, minLines/MaxLines signed Int flags2,
bWordWrap Bool flags2, and private Text FString flags`0x1000`. Window.Background
is an actual Texture reference; Window.normalFont/boldFont and its actual wGc/
gGc must retain their existing identity and ownership. Do not infer native
offsets or class constraints merely from a same-named presentation property.

## Complete outer branch/order contract at100465A0

All output operations below refer to actual live Float storage. Specified flags
do not universally suppress output writes. Preserve original store rounding,
ordering and any output alias behavior; do not preload all outputs and apply a
generic final clamp. No reflected query/cache/GC writes may escape outer rollback.

1. **Always obtain GC first:** call Window.GetGC1004DDC0. A null result returns
   immediately, leaving both outputs unchanged. It does not perform a MinWidth
   clamp, background fallback, script callback or ReleaseGC in this branch.
2. **Test actual Text C string:** use FString's current nonempty data when its
   count permits, otherwise original empty string10099EA8. Original string
   comparison stops at first NUL. Owned UTF8 transport must be decoded to
   original UTF16 units/CString; hidden suffix bytes do not make text nonempty.
3. If text is nonempty, execute one of the two measurement branches below,
   then the nonempty-only margin stores.
4. If text is empty, execute the background-or-base-script branch below. It
   does not pass through the nonempty margin stores.
5. On all nonnull-GC branches, if widthSpecified is false, compare current
   output width with MinWidth. Original x87 C0 is tested: less-than **or
   unordered** stores the raw MinWidth Float word. Otherwise leave width.
   There is no matching final minimum-height or maximum-width clamp.
6. Release the same actual GC through Window.ReleaseGC1004DE50, then return.
   This is a real context-pop protocol, not an optional cosmetic cleanup.

### Nonempty Text, constrained-height branch1004660A..100466FC

Select only when heightSpecified is false and at least one of minLines or
MaxLines is signed-nonnegative. The constructor's -1/-1 values bypass this
branch; SetLines1558's effective nonnegative results can enter it.

- If widthSpecified and bWordWrap are both true, pass exact
  `width - 2*hMargin` to GetTextExtent10027BB0, with measured X in a local
  temporary and measured Y written to actual output height. The measured X is
  discarded: output width retains its prior supplied value at this point.
- Otherwise pass destWidth0, measured X to actual output width and measured Y
  to actual output height. Zero is the original unrestricted-width request,
  not a caller-side arbitrary huge constant or a zero-extent shortcut.
- Call GetFontHeight10027D60 with includeSpacing=true and store its returned
  result to a Float temporary before multiplying line counts.
- Retain measured height as the candidate. If minLines>=0, calculate
  `minLines*lineHeight` using original x87 arithmetic/store points. Compare the
  product against the measured height; only ordered greater replaces candidate
  with the stored Float product. Equal/less/unordered do not replace it.
- If MaxLines>=0, calculate/store `MaxLines*lineHeight`; compare the original
  measured output height against that stored maximum. Only ordered greater
  replaces the current candidate with the maximum. This original reread is
  not a comparison against a newly written min-clamped output. Do not rewrite
  as a different clamp when direct field values are inconsistent.
- Finally store the selected height candidate once to actual output height.

### Nonempty Text, ordinary branch100466FE..10046768

This branch covers heightSpecified=true or both line bounds negative.

- If widthSpecified and bWordWrap, call GetTextExtent with exact
  `width - 2*hMargin`, writing both measured X/Y directly to output width/height.
- Otherwise call it with destWidth0 and both actual output references.
- No GetFontHeight or line-bound clamp runs here.

Both nonempty branches then add/store `2*hMargin` to current output width
**first**, and add/store `2*vMargin` to current output height **second**, even
when the corresponding specified flag is true. The constrained/wrapped branch
therefore adds horizontal margins to its preserved supplied width. Preserve
the original x87 intermediates and binary32 stores, not eager Float rounding.

### Empty Text branch10046788..1004680A

The pointer at Window+EC is **Background Texture**, not a Font.
GetBackground10003270 independently confirms this identity.

- If Background exists, first store its signed VSize converted to Float into
  output height, even when heightSpecified=true.
- Only if heightSpecified=false: if minLines>=0, use
  `minLines*GetFontHeight(true)` as height; otherwise, if MaxLines>=0, use
  `MaxLines*GetFontHeight(true)`. Each font-height return is stored to a Float
  temporary before the product; min takes precedence when both are present.
  If both are negative, preserve the just-written Background VSize.
- Then store Background's signed USize converted to Float into output width,
  even when widthSpecified=true. There is no margin addition in this branch.
- If Background is None, call the ordinary Window.ParentRequestedPreferredSize
  native10050390 with the original flags and output aliases. It forwards the
  optional actual most-derived script event via ProcessScript; its original
  base Extension.Window event is an empty Return/Nothing. A script subclass's
  real override must not be replaced with that empty base body.
- Continue to the final unspecified-width MinWidth test and real GC release.

## GC acquisition/copy/release: actual shared state

GetGC1004DDC0 reads Window.gGc at+134. If nonnull, it calls that GC's
PushGC100278D0, then CopyGC10027760 with Window.wGc at+130. CopyGC is the
decorated export name (not a separate fabricated renderer SetGC). The result is
the gGc used by measurement; wGc is its source context, not the measured object.

CopyGC copies actual Canvas, clip, style flags, polygon flags, colors/planes,
normalFont/boldFont, underline properties, spacing/alignment/wrapping/metachar
state and multipliers, preserving protocol-owned stack bookkeeping. Its exact
field list/store order and authentic declarations are now audited in
ORIGINAL-GC-COPY.md; the typed adapter remains unimplemented. A nonnull gGc
with invalid wGc must not silently become a
default context: original CopyGC dereferences its actual source.

PushGC uses current gcOwner/gcStack/gcFree/gcCount. It can reuse a real free
context or construct a native GC node, copy context and link stack/owner/count.
If already owner-bound, its native branch is distinct. ReleaseGC1004DE50 calls
the returned GC's PopGC100279F0; that selects the matching stack/count and invokes
PopGC(count)10027A80. Pop restores the saved context and moves real nodes onto
the free list with their original counts/owner/free flags. These operations
require owned nodes, GC edges, capacity/work admission and same-root undo.
Having two constructor-created GC objects does not implement this lifecycle.
The full push/pop, allocation/reuse, sentinel and lifetime contract is recorded
in ORIGINAL-GC-STACK.md, including the original explicit-Destroy boundary.

Font setters already target actual wGc normal/bold fonts; GetGC's copy makes
those the measurement context. Do not use a global presentation font, root's
unrelated font, cached character height or gGc's stale pre-copy state.

## GetFontHeight and exact glyph authority

GetFontHeight10027D60 starts local height at positive zero. For each nonnull
normal Font atGC+7C and bold Font at+80, it requests **space U+0020** via glyph
lookup10026880, converts the glyph's signed VSize to a Float comparison and
retains the greatest height. It considers both fonts regardless of current
bold text state. It is not the height of X or the first atlas/line.
When includeSpacing is true it adds actual textVSpacing atGC+90 and stores the
Float result; otherwise no spacing is added. Two None fonts therefore yield
zero plus optional spacing, not a fabricated default glyph height.

Glyph lookup10026950 masks to one UTF16 code unit, divides by the actual Font's
charactersPerPage, uses quotient as page and remainder as page character index,
and validates each against actual page/character counts. Missing lookup returns
zero-width/height metrics through100268F4. There is **no** lower-to-uppercase or
space substitution in this retail Extension path. Surrogates are code units,
not combined Unicode scalars; no Unicode-normalization replacement is inferred.

## Authentic GetTextExtent dependency, not width guessing

GC.GetTextExtent10027BB0 takes destWidth, Float OUT xExtent/yExtent and original
TCHAR C string. It initializes a local text state (bold/color/underline/
accelerator indicators), width/height totals and line-height state, then calls
GetLine10029D80 repeatedly. Each returned segment is decoded using
GetNextChar100294D0; normal/bold changes select actual respective fonts. It
tracks maximum glyph height per line, falls back to the selected Font's space
height when line height is ordered below1.0f (constant1005AA18), adds actual textVSpacing, accumulates
height and tracks maximum reported line width. Finally it stores X first, then
Y. No unconditional guessed one-pixel width increment appears in this method.

GetLine10029D80 delegates to the by-value20-byte text-state line routine10029AE0.
That routine has real whitespace/word boundaries, UTF16 offsets, state copies,
wrapping rollback and first-character overflow behavior. Its nonpositive or
unordered destWidth branch substitutes exact Float500000 (`0x48F42400`) inside
the original routine. Do not substitute another huge width in the caller.
GetNextChar has bParseMetachars-controlled original `|` sequences, bold toggles,
color/restore/palette state and accelerator flags; state can change without
returning a visible glyph. A generic UTF8-byte sum cannot reproduce this path.

This audit establishes the dependency and principal entry/output order, not a
complete tested line parser. ORIGINAL-GC-TEXT-LINE-PROTOCOL.md now records the
additional static scanner/partial-hex/control/whitespace/word-wrap and terminal-
linefeed contract, including the extent caller's unusual reuse of end-of-line
state for its second scan. Implementation and verification still require all
those branches, font-None/glyph-range cases, original x87 comparisons and Float
stores, output aliasing, GC failure/recovery and bounded lifetime/lookup/undo.
Do not execute only an empty Text branch and advertise general sizing complete.

## Reusable owned loader and explicit incompatibilities

`native/surreal_portable_package_tables.h` declares PortableBitmapFont/Glyph and
LoadPortableBitmapFont/DecodePortableBitmapFont; implementations are around
1016..1168 in the .cpp. The actual v64+ Font payload decoder authenticates
Engine.Font, bounded page/glyph records and atlas references/rectangles. It also
decodes full RGBA atlas images and Palette data, which is heavier than a future
bounded metrics-only reader. Preserve its resource/class/source checks if
factoring metrics out; do not make approximate metrics for failed imports.

The current PortableBitmapFont retains each glyph's pageIndex, dimensions and
charactersPerPage, but flattens page records; a quotient/remainder adapter must
retain/validate actual per-page counts instead of assuming every page full.
Its GetPortableBitmapGlyph helper concatenates indices and substitutes uppercase/
space. That helper is **not** the observed Extension glyph lookup. Its
lineHeight is the X glyph height and **not** original GetFontHeight's result.
MeasurePortableBitmapText sums raw bytes on one line and cannot replace
UTF16/bold/metachar/word-wrap/spacing measurement.

`native/quest_unreal_string.h` provides the existing UTF8/WTF8 to UTF16 code-unit
transport and first-NUL CString view. Those are suitable transport primitives,
not a replacement layout policy. PersonaUiTextWidth/Persona paint wrapping and
bundled upstream UGC DrawText/UTextWindow use different behavior; they remain
presentation/reference code, not retail authority for the new native service.

## Deferred ConfigurationChanged, not the observed failure

TextLog.ConfigurationChanged10047910 remains a separate genuine override. Its
non-null GC branch measures Text at Window.Width-2*hMargin, compares
Height-2*vMargin against returned Y with x87 C0 (less or unordered sets tooTall),
releases GC and then optionally invokes actual ConfigurationChanged script.
No-GC preserves tooTall but still invokes the script event. It does not call
the inherited Window native first. This shares future GC/measurement providers,
but the current actual refusal is preferred size, not proof this override ran.
AddLog/ClearLog/expiry Tick/Draw are likewise separate from query construction.

## Evidence and bounded next implementation

Original System/Extension.dll SHA256 remains
`93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C`.
Original Extension.u/DeusEx.u hashes and baseline locations are recorded in
ORIGINAL-TEXTLOG-STARTUP.md. Durable decorated export/RVA evidence is
`artifacts/world-native-startup-20261011/extension-exports-imports.log`.

This audit re-read original .u declarations and HUDLogDisplay callback using
the repository-owned data-only script_bytecode_inspect, and statically inspected
Extension.dll with GNU objdump:100465A0..10046843 (whole preferred method),
10003270..10003277 (Background identity),1004DDC0..1004DED0 (Get/ReleaseGC),
10027760..10027BB0 (copy/stack),10027BB0..10027E44 (extent/font height),
10026880..100269A8 (glyph),10029AE0..10029E40 (line protocol),
100294D0..10029879 (character/markup),10047910..100479D3 (TextLog config), and
vtable data1005B63C/1005A458. No raw disassembly or commercial binary is added.

Implement complete owned GC stack/copy lifetime and exact metrics/measurement
providers, then the branch-accurate inherited native sizing adapter in the
existing VM/query/root. Test all branch/output/alias and resource failures with
genuine native declarations and generated inputs; repeat actual first-Training
startup and two-Fire diagnostics to discover the next honest boundary. Preserve
the existing memory limits and already accepted VR UI layout. This documentation
was written while compiler inputs were frozen; no build/test or source edit
was performed for the audit, and no new APK acceptance is claimed.
