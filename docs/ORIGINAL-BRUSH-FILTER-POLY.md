# Original moving-brush polygon filtering and fast plane split

Static audit, 2026-10-30. This is a source contract for the initial moving-brush
tracker Update selected by first `Maps/00_Training.dx`, not an implementation or
passing startup/door/crate/APK result. No original DLL was loaded or executed;
no compiler input was edited and no build/test was run for this audit.

Authority: read-only original `D:/Steam/steamapps/common/Deus Ex/System/Engine.dll`,
fresh SHA256 `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Addresses below are that PE32's virtual addresses, not portable object offsets.
See `ORIGINAL-MOVING-BRUSH-TRACKER.md` for actual world Model ownership and caller
rollback/publication requirements. This document does not authorize skipping
that tracker or its pending dependencies.

## Authenticated helpers

External objdump export names/EAT and jump bodies establish:

- `FPoly::SplitWithPlaneFast(FPlane, FPoly*, FPoly*)`, EAT name index1559,
  export RVA1910, thunk `10301910 -> 10386AD0`.
- `FPoly::SplitInHalf(FPoly*)`, EAT name index1556, export RVA3828,
  thunk `10303828 -> 10386F70`.
- Tracker FilterPoly thunk `103030B2 -> 1037C0C0`, returning at `1037C403`.
- AddPolyToBsp thunk `10302D29 -> 1037BB00`, called with five stack arguments.

Do not substitute the separately exported slower `SplitWithPlane` overload;
the selected helper receives a by-value four-Float FPlane, not base/normal
vectors plus a precision switch.

## FilterPoly argument and local correspondence

After its16-byte local frame and four register saves, `1037C0C0` reads:

| Stack offset | Value |
| --- | --- |
| `24` | surface index |
| `28` | current world BSP node index |
| `2C` | carried coplanar node index |
| `30` | mutable scratch FPoly pointer |
| `34` | incoming outside Int/Bool |
| `10/14` | front/back output scratch FPoly pointers |
| `18` | saved tracker pointer |
| `1C` | current node byte offset, `nodeIndex*64` |

The node is reacquired from actual `Level.Model.Nodes.Data` each loop
(`1037C147..15E`). It is also reacquired after front-fragment recursion/insertion
(`1037C2F4..316`), rather than treating a stale pointer as guaranteed stable.
Scratch allocations use original global memory stack, size`0x110`, alignment8;
only the sixteen embedded FVector constructors are explicitly called. This is
not proof that all remaining scratch FPoly fields are zero initialized.

### Half splitting before BSP classification

When current polygon NumVertices is **at least14** (`1037C160..16B`), FilterPoly
allocates another FPoly, calls `current.SplitInHalf(output)`, recursively filters
the output with the same surface/node/coplanar/outside arguments, then continues
filtering the mutated current polygon. Output is therefore processed FIRST.
It is not a vertex-cap check followed by discarding surplus vertices.

`SplitInHalf` (`10386F70..1038707B`) checks `3 < n <= 16`, otherwise calling the
original error provider. It computes signed integer division `m=n/2`, copies
all272 bytes from current to output, then:

```
current.NumVertices = m + 1
output.NumVertices  = n - m + 1
output.Vertices[0 .. n-m-1] = current.Vertices[m .. n-1]
output.Vertices[n-m]        = current.Vertices[0]
current.PolyFlags |= 0x80000000
output.PolyFlags  |= 0x80000000
```

Current retained vertices are indices0..m. Both halves share the original
vertex0/vertexm diagonal. No geometric midpoint interpolation is performed.

### Node classification, outside state, and insertion

For the currently selected node, define exactly the native CSG predicate:

```
csg = (node.NumVertices > 0) && ((node.NodeFlags & 0x21) == 0)
frontOutside = incomingOutside != 0 || csg
backOutside  = incomingOutside != 0 && !csg
```

Unsigned byte NumVertices is tested against zero. `0x21` is the actual mask;
do not replace it with a test of one guessed flag.

1. NodeFlags`0x40` selects FRONT immediately (`1037C1DE..1E3`). Otherwise
   NodeFlags`0x80` selects BACK immediately. If both bits are set, FRONT wins.
2. Otherwise call `current.SplitWithPlaneFast(copy(node.Plane),front,back)`.
   Return1 selects FRONT;2 selects BACK;0 is COPLANAR;3 is SPLIT. Any other
   integer exits without a node insertion (`1037C207..21C`,`1037C281..284`).
3. COPLANAR computes `(tracker.Normal.z*node.Plane.z +
   tracker.Normal.y*node.Plane.y) + node.Plane.x*tracker.Normal.x` in the shown
   x87 order (`1037C21E..234`). If this result is **not less than zero**, replace
   the carried coplanar index with current node index. Either orientation then
   takes FRONT, not BACK. Zero therefore updates coplanar index. The zero
   constant at `10429838` is double0, not an epsilon.
4. FRONT replaces outside with `frontOutside`. If node.Front`+24 != -1`, iterate
   at that child. Otherwise insert current only if new outside is nonzero:
   `AddPolyToBsp(surface,nodeIndex,coplanarIndex,1,current)` and return
   (`1037C249..27C`,`1037C3A8..3D4`).
5. BACK replaces outside with `backOutside`. If node.Back`+20 != -1`, iterate at
   that child. Otherwise insert current only if new outside is nonzero:
   `AddPolyToBsp(surface,nodeIndex,coplanarIndex,0,current)` and return
   (`1037C374..3A3`,`1037C3D7..403`).

SPLIT processes front before back (`1037C28A..370`):

- With an existing Front child, recursively filter front scratch using that
  child, carried coplanar index and `frontOutside`.
- With no Front child, insert front scratch only if `frontOutside` is true,
  passing placement argument1 and the current node as parent.
- Replace the current polygon argument with the **old back scratch pointer**;
  reacquire the actual node; allocate a NEW back scratch buffer so the next
  plane split cannot overwrite its current input. Then take BACK using the
  original incoming outside value, producing `backOutside` normally.

The current coplanar index is carried through split fragments. Front recursion
does not replace the caller's coplanar variable. Tail traversal of the back
fragment is not an arbitrary order-independent work queue.

## SplitWithPlaneFast: classification and exact stores

`10386AD0..10386E74` receives six stack words: Plane(X,Y,Z,W), front pointer,
back pointer. It uses a sixteen-entry side array; the supported native FPoly
capacity remains16. No output buffers are written for nonsplit classifications.

For each vertex in original order, evaluate the x87 sequence

```
distance = ((Plane.Z * vertex.Z + Plane.X * vertex.X)
            + Plane.Y * vertex.Y) - Plane.W
side = distance < 0 ? 1 : 0
hasFront |= distance > +0.25
hasBack  |= distance < -0.25
```

This is a retained extended-register expression, not a stored Float distance.
Thresholds at `1042D528/1042D518` are doubles `+0.25/-0.25` respectively
(bytes`000000000000D03F`/`000000000000D0BF`). The strict comparisons mean vertices
at exactly +/-0.25 do not set the corresponding hasFront/hasBack bit. Their
side still follows the sign against zero. Thus tolerance classification and
edge-side classification are deliberately different.

Return values (`10386B71..BB1`) are:

| hasFront | hasBack | Return |
| --- | --- | --- |
| false | false | 0, coplanar |
| true | false | 1, front |
| false | true | 2, back |
| true | true | 3, split |

Zero vertices also returns0. If split and front pointer is null, it returns3
without constructing fragments; otherwise both output pointers are consumed.
There is no independent back-null guard in the constructing path.

### Edge traversal and intersection arithmetic

Start previous at the last source vertex, then visit current0..n-1. Equal side
bits copy current into that side's output. Different bits insert one identical
computed intersection into BOTH outputs, then append current to its own side.
No near-plane point welding, collinearity cleanup, `Fix`, or extra epsilon is
called in this helper.

The precise x87/storage graph at `10386C20..CF7` is important. Below `F32(expr)`
means the explicit single-precision `fst/fstp` memory store in the original;
unmarked arithmetic retains the ambient original x87 precision/rounding, to be
implemented using the project's separately audited numeric machinery:

```
dxExtended = current.X - previous.X
dx = F32(dxExtended)
dy = F32(current.Y - previous.Y)
dz = F32(current.Z - previous.Z)

numerator   = Plane.W - ((Plane.X * previous.X + Plane.Z * previous.Z)
                        + Plane.Y * previous.Y)
denominator = (Plane.Z * dz + Plane.Y * dy) + dx * Plane.X
t = F32(numerator / denominator)

xOffsetExtended = dxExtended * t
yOffset = F32(dy * t)
zOffset = F32(dz * t)
intersection.X = F32(xOffsetExtended + previous.X)
intersection.Y = F32(yOffset + previous.Y)
intersection.Z = F32(zOffset + previous.Z)
```

In particular, X's offset uses the retained extended `dxExtended`, while
denominator uses the stored single-precision `dx`. Y/Z offsets are rounded
BEFORE adding previous. Replacing this graph with a generic `previous + delta*t`
Float/vector operation, symmetric component implementation, reused distance
ratio, fused multiply-add, clamped t, or added denominator epsilon is not the
recovered operation. The copied Float intersection bits are identical in both
outputs; computation is not repeated separately.

### Fragment fields actually written

After constructing vertices, the helper writes each output's NumVertices and
copies only source Base`00..08`, Normal`0C..14`, and PolyFlags`F0`
(`10386DD7..E5F`). It does NOT initialize/copy TextureU/V, Actor, Texture,
ItemName, LinkIndex, BrushPolyIndex or pans in these scratch outputs. The caller
carries the actual surface index separately. Do not claim complete native
FPoly copying/initialization for this helper; do not read unspecified fields
as if they had authentic zero/default values.

## Bounded reproduction (static only)

```powershell
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1037c0c0 --stop-address=0x1037c406 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x10386ad0 --stop-address=0x10386e75 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x10386f70 --stop-address=0x1038707c 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -s --start-address=0x1042d518 --stop-address=0x1042d530 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
```

This establishes the selected filter/split graph. AddPolyToBsp's allocator,
zone/leaf topology, failure behavior and point ownership, plus noninitial
flush/dependency propagation, still need their own concrete implementation and
verification. This audit does not advance the runtime startup guard.
