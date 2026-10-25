# Original native Tile preferred-size/layout audit

Read-only owned GOTY audit, 2026-10-25. This document is **not an
implementation or a completed HUD/startup claim**. No commercial EXE/DLL was
loaded or executed. Desktop/Android compiler inputs remained frozen during
this audit; only this new document was written.

The next actual first-Training HUD refusal is the Tile native preferred-size
virtual, reached from
`DeusEx.HUDActiveItemsBorderBase.ParentRequestedPreferredSize` PC9/opcode101.
The pending graph contains132 objects/8890 writes. The actual two-crowbar
diagnostic still completes its original attacks before the same later HUD
refusal; that does not publish a player session. See
[the scalar-query audit](ORIGINAL-WINDOW-PREFERRED-AXIS-QUERY.md).

## Primary owned evidence and reproduction

SHA256 of the files inspected afresh:

- `System/Extension.dll`:
  `93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C`.
- `System/Extension.u`:
  `9984FAEDA55FB2100B1A15A37BFEB8CDB4878248B39AFF832002A799F1D787F7`.

Existing ignored evidence paths:

- `artifacts/world-native-startup-20261011/extension-exports-imports.log`:
  exports425,447,947,961 and285 establish the symbols/RVAs below, and the
  Core/MSVCRT imports.
- `artifacts/hud-query-preferred-body-audit-20261015.log`: the shared Window
  query service and its direct width/height wrappers.
- `artifacts/hud-window-query-field-metadata-20261015.log`: owned Window
  query-field metadata.
- `artifacts/window-axis-query-hud-discovery-20261025.log` and
  `artifacts/window-axis-query-fire-discovery-20261025.log`: the actual
  reached HUD/attack diagnostic boundary, not Tile implementation proof.

The fresh complete Tile body/vtable disassembly was read directly during
this audit, without creating another artifact under the document-only task.
It is reproducible with the following static commands:

```powershell
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x100487e0 --stop-address=0x10048860 'D:/Steam/steamapps/common/Deus Ex/System/Extension.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x10048a90 --stop-address=0x100491c0 'D:/Steam/steamapps/common/Deus Ex/System/Extension.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -s --start-address=0x1005b760 --stop-address=0x1005b800 'D:/Steam/steamapps/common/Deus Ex/System/Extension.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1004c650 --stop-address=0x1004c7d0 'D:/Steam/steamapps/common/Deus Ex/System/Extension.dll'
```

## Native dispatch, not another script callback

| Identity | Owned address / behavior |
| --- | --- |
| Export961 `XTileWindow::ParentRequestedPreferredSize(int,float&,int,float&)` | RVA487E0, body100487E0..1004882D |
| Export425 private `XTileWindow::ComputeChildSizes(int,float&,int,float&)` | RVA48A90, body10048A90..10049181 |
| Export285 `XTileWindow` vtable |1005B760; slot+7C at1005B7DC contains100487E0 |
| Export447 `XTileWindow::ConfigurationChanged()` |100489C0; calls the same ComputeChildSizes with both specified and references to current Width/Height |
| Export947 `XTileWindow::ParentRequestedGranularity(float&,float&)` |10048860; a separate method, not required by this reached preferred-size virtual |

The Tile preferred-size virtual only forwards its four arguments to
ComputeChildSizes at10048818. It does **not** call the base Window virtual
10050390/ProcessScript or dispatch a script override of its own preferred-size
event. Children still use their own genuine query/native-virtual/script
selection. Treating Tile as ordinary Window silently omits native layout.

ConfigurationChanged subsequently configures visible children with their held
geometry through actual ConfigureChild1004EC50; the preferred-size virtual
does not perform those ConfigureChild calls. Supporting this sizing method
alone does not authorize the separate Tile configuration/lifecycle override.

## Native fields and row storage

| Owned offset | Meaning |
| --- | --- |
| Window+30 bit1 | local `bIsVisible`, not effective ancestor visibility |
| Window+80 / +84 | current `Width` / `Height` |
| Window+A4 / +A8 / +AC / +B0 | `holdX` / `holdY` / `holdWidth` / `holdHeight` |
| Window+13C / +140 | `firstChild` / `lastChild` |
| Window+144 / +148 | `prevSibling` / `nextSibling` |
| Tile+14C / +14D / +14E | `orientation` / `hDirection` / `vDirection` Bytes |
| Tile+150 / +154 | `hMargin` / `vMargin` Floats |
| Tile+158 / +15C | `minorSpacing` / `majorSpacing` Floats |
| Tile+160 / +161 | `hChildAlign` / `vChildAlign` Bytes |
| Tile+164 bits1/2/4/8 | `bWrap` / `bFillParent` / `bEqualWidth` / `bEqualHeight` |
| Tile+168 / +16C / +170 | native row-array data pointer / count / capacity |

Orientation1 is vertical. Every other Byte follows the horizontal branches;
there is no enum-range refusal. Direction1 reverses its axis; other values do
not. Alignment1/2/3 means center/far/fill on the applicable cross axis; other
values keep the initial near placement.

Each row is exactly12 bytes, three signed INT words:
`{crossSize, mainExtent, childCount}`. These are not window pointers or
script DynamicArray data.10048CC2..10048CDA clears the native array using
FArray.Empty(elementSize12). Rows are appended with FArray.Add(1,12) through
IAT100590A4 at10048D65/10048DF4. Their three words are written explicitly.

The current runtime owns only `RuntimeNativeWindowState::emptyTileRows` as
opaque constructor support. It has no genuine row records yet. A general
implementation needs a typed native row vector and complete journal/budget
handling. Existing Tile setters currently use that bool as initialized-storage
authority: do not turn it false on population and thereby reject every later
setter. Separate initialized Tile storage from actual row emptiness.

## Ordered sizing pass

Input names below are `widthSpecified`, `W` reference, `heightSpecified`, `H`
reference. Ordinary simulation PC64/nearest arithmetic and actual Float stores
are required; no renderer FastMath mode is inferred.

1.10048AB5 captures the initial packed Tile flags once. Local fixed-width and
   fixed-height flags, and local cross-fill Float constraint, start at0.
   Only **not wrapping and filling parent** enters the fill-constraint branch.
   For orientation1 with width specified, enable fixed child width and compute
   `W-(hMargin+hMargin)`. For other orientations with height specified, enable
   fixed child height and compute `H-(vMargin+vMargin)`. Store the result to
   Float, then compare the still-retained PC64 result against0; if negative,
   overwrite the local with positive0. Other constraints/fields are inactive.
   In particular, vertical layout with unspecified width does not substitute a
   supplied height as the fill constraint.10048AC9..10048B31.
2. Visit **lastChild to prevSibling**, through live links, starting10048B38.
   Test each child's local visibility when reached. Do not use first-child
   order, a sorted list, ancestor visibility or a precollected visible vector.
3. For a visible child, fixed-width mode writes its holdWidth constraint
   **before** calling actual QueryPreferredHeight1004E680, then Float-stores
   the returned height into holdHeight. Fixed-height mode writes holdHeight
   first, calls actual QueryPreferredWidth1004E5F0, then writes holdWidth.
   Otherwise actual QueryPreferredSize1004E710 writes holdWidth then holdHeight.
   Child queries retain the shared query cache, actual virtual dispatch and
   selected script callbacks.10048B5E..10048BC3.
4. After that query, freshly read the parent's packed equality flags. Maxima
   begin at positive0. With the applicable flag set, replace a maximum only
   when the child's current held Float is strictly greater. Width precedes
   height. The child query can change parent flags/child state; a global entry
   snapshot is not equivalent.10048BC3..10048C05.
5. A child whose initial local visibility test was false gets positive0 stores
   to holdWidth, holdHeight, holdX, holdY, in that order, without a query.
   Then reread its live previous-sibling link.10048C0A..10048C2A.
6. After the reverse pass, test current equality flags. If either is enabled,
   traverse currently visible children **firstChild to nextSibling** and write
   applicable maxima to holdWidth then holdHeight.10048C2F..10048C75.

GetFirstChild(true)1004C740 uses firstChild+13C and skips hidden siblings via
GetNextSibling(true)1004C680, whose link is nextSibling+148. These are local
visibility filters. Initial reverse queries intentionally use different order.

## Wrap bound and exact integer rows

The wrap bound starts at Float1,000,000, bits49742400. With current `bWrap`
enabled, a horizontal layout with width specified replaces it with the Float
store of `W-2*hMargin`; a vertical layout with height specified instead uses
the Float store of `H-2*vMargin`. Unlike the fill constraint, this bound is
**not clamped**. Inputs and fields are read after child queries, not all at
function entry.10048C75..10048CC2.

After clearing row storage, the code converts the saved fill constraint using
`_ftol` to initial signed row crossSize, and initializes running mainExtent and
count to0. This conversion happens even when no visible children remain.
10048CE0..10048CF9. The logical row operation is:

- Visit currently visible children forward. Capture held cross/main Floats
  for that child: vertical uses holdWidth/holdHeight; horizontal swaps them.
- If count>0 and **PC64(INT runningExtent + childMainFloat) > wrapBoundFloat**,
  append the previous row with extent obtained by `_ftol(INT runningExtent -
  minorSpacing)`, then reset crossSize from the original saved fill constraint
  and runningExtent/count to0. Equality does not wrap.10048D35..10048D98.
- Set runningExtent to `_ftol((INT runningExtent + minorSpacing) + childMain)`.
  Both additions retain PC64 before conversion. Do not accumulate all children
  as Float and truncate only once.10048D98..10048DA9.
- If current signed crossSize INT is less than childCross Float, set crossSize
  to `_ftol(childCross)`. Increment count, then follow next visible sibling.
 10048DA9..10048DCE.
- Append the final row only when count>0, again removing minorSpacing through
  `_ftol` first. No fabricated empty row is appended.10048DD3..10048E10.

The wrap comparison is unconditional in row construction. `bWrap=false`
retains the finite million-unit sentinel; it is **not** an infinite/no-wrap
branch. An oversized first child is accepted into its row without producing
an empty preceding row. Negative finite margins/spacings/sizes retain their
actual arithmetic; do not introduce nonnegative clamps beyond the one proved
fill-constraint branch.

`_ftol` calls use1005480A -> IAT100593A4 -> MSVCRT._ftol. The caller consumes
low32 EAX as signed INT, not a nearest-integer Float result. Existing static
CRT conversion evidence establishes truncation toward zero. Portable safety
refusal for a reached nonfinite or out-of-supported integer conversion is
distinct from native behavior; never silently clamp or substitute0. Full CRT
exceptional/indefinite conversion and unbounded native integer overflow are
not proved by this audit.

## Both-specified placement

When both specified flags are nonzero,10048E10 takes the placement path.
It does not write preferred W/H outputs. Cursor X/Y start from Float hMargin/
vMargin and row index0. Each visible child forward receives raw held geometry
stores; there are no ConfigureChild calls or script callbacks in this pass.

For vertical layout10048E4D..10048F84:

- holdY is cursorY, or `H-(cursorY+childHeight)` for vDirection1.
- holdX is cursorX, or `W-(rowCrossINT+cursorX)` for hDirection1.
- hChildAlign1 adds signed integer `trunc(trunc(rowCross-childWidth)/2)` to
  holdX. The first truncation is `_ftol`, the second is signed integer divide
  by2 using CDQ/SUB/SAR. Alignment2 adds retained PC64 `rowCross-childWidth`;
  alignment3 sets holdWidth to the Float conversion of rowCross INT.
- CursorY becomes Float `(minorSpacing+childHeight)+cursorY`. Decrement this
  row's stored childCount. At count<1, reset cursorY to vMargin and advance
  cursorX as Float `(rowCross+majorSpacing)+cursorX`, then advance row index.

For horizontal layout10048F89..100490A6:

- holdY is cursorY, or `H-(rowCrossINT+cursorY)` for vDirection1.
- holdX is cursorX, or `W-(cursorX+childWidth)` for hDirection1.
- vChildAlign1/2/3 performs the corresponding center/far/fill operation on
  holdY/holdHeight, with the same two-stage integer center truncation.
- CursorX advances with minorSpacing and childWidth. At row completion reset
  it to hMargin and advance cursorY using rowCross and majorSpacing.

Placement consumes stored row counts to0. A preferred-size-only query retains
them. The visible iteration and raw writes remain live; do not call a layout
callback while replacing these stores or convert held geometry into current
rendered geometry early. Current W/H references are reread at their actual
uses, important if the shared method is later reused by ConfigurationChanged
with references to mutable current Window dimensions.

## Preferred output arithmetic

If either dimension is unspecified,100490AB skips placement and computes
content size from the actual rows. Row counts are not consumed. Width Float
and height PC64 accumulators start at positive0.

- Vertical: width is Float-stored after adding each rowCross INT, and, for
  row index>0, is separately Float-stored again after adding majorSpacing.
  Each row mainExtent INT is **spilled to Float before** the height-maximum
  comparison; the maximum remains retained PC64.100490C2..10049107.
- Horizontal: compare width's current Float against exact row mainExtent INT;
  replace with a Float store only when greater. Height adds rowCross INT and,
  for row index>0, majorSpacing, retaining PC64 across the entire loop with
  no intervening Float spill.10049109..10049143.
- Only when width is unspecified, write Float `(hMargin+hMargin)+width` to W.
  Then only when height is unspecified, write Float
  `(vMargin+vMargin)+retainedHeight` to H. Double margins first; do not rewrite
  this as two sequential margin additions.10049145..1004916F.

The normal method returns at10049181.10049184..100491AF is exception-guard
diagnostic/rethrow machinery, not fallback success.

## Required runtime integration and tests

The smallest faithful **general** provider is the complete ComputeChildSizes
protocol with audited PC64/Float/INT math, live Window/Tile operations, actual
recursive child query dispatch, and typed row state. An empty-only stub or
ordinary Window fallback is not sufficient. Even the reached Training icon
Tile has ten locally hidden augmentation children: the general initial pass
must perform their forty held-geometry zero stores before yielding no rows.

Implementation needs:

- Nearest RF_Native Tile virtual authentication at the existing preferred-size
  dispatch seam. Keep other unaudited native preferred-size overrides refused.
- Actual qualified held fields and Tile field schemas. Preserve Const2 native
  authority without exposing arbitrary script writes to those fields.
- Row-vector before-images in native Window transaction snapshots and actual
  capacity accounting in live/rollback retained-state budgets. Rows have no
  UObject edges, but surrounding window/child graphs still require genuine
  existing GC roots and lifetime handling.
- One supplied Execution for parent query caches, all child callbacks/queries,
  held geometry, row clears/appends/count writes and final outputs. No nested
  public execution, reset work allowance, post-failure callbacks or local
  rollback that replaces the original first failure.
- Preserve actual output-reference order. Do not globally copy out all held
  fields after callbacks: fixed held constraints are visible before child
  queries; hidden child writes are ordered; later passes see live state.

Minimum controls should include:

- Empty and all-hidden trees; hidden held-field zero order; one/multiple
  visible children; reverse query order versus forward row order.
- Both orientations, every raw Byte direction/alignment, wrapping/filling/
  equal-size combinations and each specified-dimension combination. Exercise
  million-unit sentinel wrapping even with bWrap false.
- Fractional/negative/signed-zero margins, spacings and held sizes; exact wrap
  equality versus retained-PC64 greater; per-child integer truncation;
  Float-vs-retained extrema and totals; integer-center negative odd gaps.
- Independent test-owned x87 oracle for every operation/spill/conversion
  boundary, never commercial code execution or production math as expected.
- Actual selected child script callbacks/cache hits; query callbacks changing
  parent flags/orientation, child visibility/held fields and sibling links;
  repeated calls replacing old row state. Malformed/cyclic/lifetime-invalid
  graphs must propagate an honest bounded refusal, not silently skip nodes.
- Shared write/work/retained-memory cuts in the reverse queries, equality
  stores, row reset/append and placement; late child failure and late root
  failure after valid layout; full held/row/cache/name/root/GC/save/revision
  undo, including caught first-failure poisoning.
- Genuine original Training HUD recursion and normal startup after the pure
  and isolated native fixtures. Observe the next actual dependency before
  changing exact-boundary assertions. Passing this provider is not accepted
  Root visibility, window destruction, input/rendering or player possession.

Not yet proved/implemented here: a portable general Tile provider, native row
publication, Tile ConfigurationChanged integration, complete destructive window
reentry/lifecycle behavior, other native preferred-size virtuals, full CRT
exceptional conversions, and any on-headset acceptance. Existing explicit
refusals for those services must not be removed by this documentation.
