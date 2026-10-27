# Original GC state and TextWindow sizing implementation

2026-10-27, verified implementation checkpoint. First Training only.
This is not successful player startup, live VR crates, or a packaged APK claim.
The source contracts remain ORIGINAL-GC-COPY.md, ORIGINAL-GC-STACK.md,
ORIGINAL-GC-TEXT-LINE-PROTOCOL.md and ORIGINAL-TEXTWINDOW-PREFERRED-SIZE.md.
Commercial executables/DLLs are inspected only as static data, never executed.

## Actual context graph

The native adapter authenticates real constructed Extension.GC objects in the
current Window ownership domain, exact function/property declarations and
current lookup identity. CopyGC1270 copies the27 semantic fields in original
order through existing typed writes; it does not normalize style/colors or
overwrite the five stack/protocol fields. Scalar/Color/Plane/ClipRect payloads
retain raw words. Object values maintain actual markable property edges.

PushGC1271 uses genuine constructor-owned GC nodes, Outer=receiver, original
class naming/identity and a copying constructor, not the ordinary default
constructor followed by invented initialization. PopGC1272 restores the final
popped context and retains the nodes on the actual reusable free list. The
signed count arithmetic wraps as defined32-bit words. Owner-bound no-op,
threshold/no-exact-match and original optional/explicit-1 branches are distinct.
Context construction, references, name counters, lists and beforeimages share
the caller's one transaction and existing byte/object/native-work budgets.
Cycles/shared nodes/malformed membership are explicit charged failures.

GetGC1485 pushes actual Window.gGc then copies actual current Window.wGc;
ReleaseGC1486 pops the passed GC, not an invented token or a freshly reloaded
Window context. None branches do not fabricate a drawing service. Source None
for CopyGC is an explicit safe transactional error, not a default context.
Native input temporaries are snapshotted by the VM; Copy/Release seed Object
None and Pop seeds Int-1, matching the inspected wrappers.

Ordinary runtime collection/unload is not arbitrary original explicit GC
Destroy. The counterintuitive free-head Destroy branch remains a separate
unsupported scope; this adapter does not silently replace its original stores.
No Canvas or render backend is constructed by admitting these state methods.

## Font resource authority and measurement

The metrics-only reader retains actual Font page counts, per-page character
counts, signed glyph rectangles, charactersPerPage and atlas references. It
authenticates bounded source export/class/outer/tag/HasStack structure, local
atlas/Palette provenance, mip dimensions/spans and glyph rectangles. It streams
index-domain validation for palettes smaller than256 rather than allocating
RGBA atlases. Existing explicit resource-validity refusals are not converted
to guessed glyph metrics. Missing glyph lookup returns zero via the original
masked UTF16 quotient/remainder; there is no uppercase/space substitution.

The host caches only current actual immutable Font identities within its one
execution owner. Actual cache capacities and loader scratch/result overlap are
admitted and charged, with no enlarged live/snapshot limit. Cache entries are
released on commit/rollback and cannot authorize a replacement map/resource.
Text is borrowed from the actual private field until admitted UTF16 decoding;
its scratch remains charged through nested measurements and script fallback.
It is not decoded before GetGC's null early return.

The scanner/line/extent algorithm follows the original control/partial-hex/
palette, whitespace and long-word branches, first-character overflow, terminal
linefeed, full text-state snapshots and unusual end-state rescan behavior.
It uses the existing explicit PC64 scalar profile and original Float spills,
including the markup reciprocal distinct from SetTextColor. GetFontHeight
queries both fonts' actual space glyphs and optional current spacing.

TextWindow's inherited preferred-size virtual now uses these real services:
all text/background/base-script branches, original inconsistent line-bound
ordering, preserved width in constrained wrapped measurement, ordered margin/
output writes, exact background reload and final C0 MinWidth test. Null GC
does not measure, clamp, call script or release. The base Window callback still
copies width/height into distinct script packet cells and copies them out in
order, even when the outer native outputs alias (original10050390..10050417).
Existing query/cache/native layout admission remains a separate service.

## Verification

The three generated policy suites pass:

- GC context:331148 checks/934 refusal controls.
- Text measurement:171541 checks/227 refusal controls.
- Preferred sizing:25831 checks/81 refusal controls.

These use explicit providers and mock root undo, not actual UObject startup.
The first targeted build needed CMake regeneration for new targets; its first
compiler failure was a copied pair loop under-Werror, corrected to const&.
Font metrics pass132810 checks/79 refusals, including independent full-
decoder comparisons of three original DeusExUI fonts. Their retained size is
4288..4292 bytes and peak working memory12480..12484 bytes. The extended VM
suite passes696473 checks/918 refusals, including the new GC input snapshots.

Original owned HUD discovery now reaches HUDMissionStartTextDisplay.InitWindow
PC159/opcode101,13211 writes, unsupported native1483, rather than the old
preferred-size failure. Its diagnostic snapshot initially exceeded the same
4MiB cap after copying173 objects; that was not the full graph count. Audit
found RetainedValue already counts its root Value while
the enclosing field charge counted it again. The replacement accounts exact
field-vector capacities and overlapping growth, counts each inline Value once
and retains the unchanged cap. The rebuilt capture now succeeds with the actual
178-object graph and all27 GC context/five protocol fields, including the real
saved/free GC node. The owned HUD suite passes9389 controls against this exact
prefix, including original AugmentationDisplayWindow.Lower sibling order and
the mission-start children's copied CDO FontLocation value. An unchanged copied
CDO scalar does not fabricate an instance-native property-root write.

The real GC fixture's context/stack/native/edge controls reached its final
post-save-reload comparison. That assertion incorrectly held the pre-load
world revision; the explicit load intentionally publishes one new revision.
The fixture now verifies that one increment separately and checks subsequent
rollback against it. The expanded nonempty real-font fixture then exposed an
over-strict host Font guard: Engine.Font is an authentic registered asset class
without a serialized/materialized UClass pointer on these resources. The host
now authenticates actual immutable export/import cast identity and the typed
metrics reader instead of requiring that pointer or fabricating a UClass.
The final real GC integration passes155 checks/16 refusals:10584 writes,
186245 native-work units, fresh/reused Push44/35 writes and three nonempty
real-font preferred-size cases. Cold/warm, malformed ingress, early/late write
and native-work failures, explicit save/reload and complete save/GC/object/name
counter rollback are verified. No failed intermediate is accepted evidence.

Actual two-Fire startup initially exposed a real32MiB accounting failure, not
a text callback error. List validation now proves cycle/excess membership from
the finite authenticated object domain with constant scratch rather than a
32KiB seen array. The native Window reference index retains only nonempty
reachability edges; typed scalar/None property storage and whole-map beforeimage
swap/undo remain unchanged. No live/snapshot cap or save format was enlarged.
An optimized GNU string-concatenation warning was removed by explicit key
construction; the final build is clean.

The full desktop build, current Android-native library link and strict ARM64
syntax check pass. Final CTest registers269:225 executed,44 explicit owned-data
skips,zero failures144.57s. A separate post-build actor-update repeat passes.
Separately rebuilt owned first-Training crate415/5,deferred429/21,animation
588/24,FastTrace252/0,Input classes10413/38,viewport10310/20,materials and1232
interaction/save-route controls all pass. All45 originals and20 frozen compiler
inputs match length/UTC modification time/SHA256 baselines; all jobs are terminal.

Actual cold/warm two-Fire repeat2 now reaches the later native1483 boundary at
253048 instructions/29924 writes. Both retain two attack notifies,four traces,
two crate changes,nine births,48 weapon observations and full world/save/GC undo.
Door repeat2 retains the precise unprepared ReadInput refusal at239536/19432,
both original56-unit moves/two endpoints and full undo. Only first Training is
loaded; none of these diagnostics publishes a playable player or live physics.
No new APK has been packaged or installed.

## Next actually reached service, not implemented by this checkpoint

Owned HUD reached Window.EnableTranslucentText native1483, flags0x401, optional
bEnable BoolProperty dim1/flags0x90. Static exec10053F10..10053F53 seeds true,
not false. Window1004DC90..1004DCFB compares the current bTextTranslucent bit
with the native INT. For canonical equal Bool it does nothing, even if the GC
flag is inconsistent. On change it stores only that Window flag, then if wGc
exists calls GC.EnableTranslucentText10027610..1002763A with the stored Bool.
That GC method always stores bTextTranslucent and textPolyFlags0x804(true) or
0x802(false). No gGc update, layout/style/color/font callback is present.
The original DLL was read only as static data. This is a next-service contract,
not its implementation or proof that startup beyond the call is complete.
