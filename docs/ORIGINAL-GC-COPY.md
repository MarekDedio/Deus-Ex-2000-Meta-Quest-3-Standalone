# Original GC.CopyGC context-copy contract

Read-only retail contract audit, 2026-10-27. This is implementation evidence for
the first Training HUD dependency, not an implemented GC service, a completed
player startup or an APK/headset acceptance claim. No original commercial
executable or DLL was executed or loaded as code.

## Authority and actual declaration

The original decorated export `?CopyGC@XGC@@QAEXAAV1@@Z` has RVA `0x27760`,
VA `0x10027760`, in the locally owned System/Extension.dll. Its script wrapper
`?execCopyGC@XGC@@QAEXAAUFFrame@@QAX@Z` is at `0x1002BEF0`.
`artifacts/world-native-startup-20261011/extension-exports-imports.log`
records these exports. This audit re-read the whole native copy method through
its return at `0x100278C6` with GNU objdump, treating the DLL only as data.

The repository-owned data-only `script_bytecode_inspect` confirms the actual
Extension.u declaration:

- `Extension.GC.CopyGC`, native **1270**, function flags `0x401`, no result.
- One required parameter, `Copy`, an ObjectProperty referencing actual GC,
  dimension 1, flags `0x80`. It is neither OUT nor optional.
- Its native parameter bytecode contains that actual parameter declaration.
- The wrapper evaluates the source object expression, advances past the
  end-parameters token and directly calls the native copy. It does not replace
  None with a fresh/default context or check an equality shortcut.

For comparison, the same package declares `GC.PushGC` native 1271 with an Int
return (`0x580`), and `GC.PopGC` native 1272 with optional Int `gcNum` (`0x90`).
Those are separate protocols, not extra implicit side effects of CopyGC. Their
full order and original failure branches are recorded in
[ORIGINAL-GC-STACK.md](ORIGINAL-GC-STACK.md).

Original SHA256, rechecked during this audit:

| File | SHA256 |
| --- | --- |
| System/Extension.dll | `93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C` |
| System/Extension.u | `9984FAEDA55FB2100B1A15A37BFEB8CDB4878248B39AFF832002A799F1D787F7` |

## Exact copy order and operations

`CopyGC(destination, source)` is a context copy, not an UObject memcpy or an
invocation of all the corresponding public setters. The table follows the
original store order. Offsets below describe the observed 32-bit retail layout;
the portable host must use authenticated typed properties, not reuse those
offsets against an ARM64 C++ object.

| Order | Retail destination | Actual reflected semantic field | Copy operation |
| --- | --- | --- | --- |
| 1 | `+28` | Canvas | Copy object reference word |
| 2 | `+2C..+40` | gcClipRect | Copy six consecutive raw Float words, forward |
| 3 | `+44` | Style | Copy one raw Byte |
| 4 | `+48`, mask `01` | bSmoothed | Copy this source bit, preserve all other destination bits |
| 5 | `+48`, mask `02` | bDrawEnabled | Same masked-bit operation |
| 6 | `+48`, mask `04` | bMasked | Same masked-bit operation |
| 7 | `+48`, mask `08` | bTranslucent | Same masked-bit operation |
| 8 | `+48`, mask `10` | bModulated | Same masked-bit operation |
| 9 | `+48`, mask `20` | bTextTranslucent | Same masked-bit operation |
| 10 | `+4C` | PolyFlags | Copy raw Int word |
| 11 | `+50` | textPolyFlags | Copy raw Int word |
| 12 | `+54` | tileColor | Copy the complete four-byte Color |
| 13 | `+58,+5C,+60,+64` | tilePlane | Copy four raw Float words, forward |
| 14 | `+68` | TextColor | Copy the complete four-byte Color |
| 15 | `+6C,+70,+74,+78` | textPlane | Copy four raw Float words, forward |
| 16 | `+7C` | normalFont | Copy object reference word |
| 17 | `+80` | boldFont | Copy object reference word |
| 18 | `+84` | underlineTexture | Copy object reference word |
| 19 | `+88` | underlineHeight | Copy raw Float word |
| 20 | `+8C` | baselineOffset | Copy raw Float word |
| 21 | `+90` | textVSpacing | Copy raw Float word |
| 22 | `+94` | HAlign | Copy one raw Byte |
| 23 | `+95` | VAlign | Copy one raw Byte |
| 24 | `+98`, mask `01` | bWordWrap | Copy this source bit, preserve all other destination bits |
| 25 | `+98`, mask `02` | bParseMetachars | Same masked-bit operation |
| 26 | `+9C` | hMultiplier | Copy raw Int word |
| 27 | `+A0` | vMultiplier | Copy raw Int word |

Each masked operation is equivalent to
`destination ^= (source ^ destination) & mask`; it is not a wholesale copy of
either packed word. The method reads each source value in the displayed order.
It permits self-copy naturally: writes still occur, but context values and
protocol bookkeeping remain unchanged. There is no explicit self-copy return.

There are no floating-point arithmetic instructions in CopyGC. Float payloads,
including signed zero, infinities and NaN payloads, are copied as raw words.
Both Colors include their alpha byte; both Planes include W. A copied Plane is
not recalculated from its Color. Inconsistent authored/source Style, flags,
PolyFlags, Color/Plane or alignment values remain inconsistent after the copy;
using SetStyle, SetTextColor, SetFonts or a generic normalization pass would
introduce side effects absent from this method.

Bit identities are independently confirmed by the authentic native getters and
setters: smoothing `100270E0`, drawing `10027280`, masking `100272A0`,
translucency `10027330`, modulation `100273C0`, translucent text `10027610`,
word wrap `10027720`, and special-text parsing `10027740`. Fonts
`10027640`, baseline data `10027680`, alignments `100276D0`, spacing
`100276A0`, Canvas getter `10002CF0`, clip getter `10002D00`, Color getters
`10002D90/10002DA0` and Font getters `10002DD0` corroborate the named slots.
The complete clip copy is independent of per-member arithmetic or recomputation.

## Authentic property schema, not an opaque native rendering cache

The data-only inspector confirms every semantic field in the table is a real
dimension-1 property declared on Extension.GC. All have property flags zero
except Canvas, whose flags are `0x1000` (private). In particular:

- Canvas is ObjectProperty -> Engine.Canvas.
- gcClipRect is StructProperty -> ExtensionObject.ClipRect; its six named
  fields originX/originY/clipX/clipY/clipWidth/clipHeight are dimension-1
  FloatProperty, flags zero.
- Style is ByteProperty without an enum reference. HAlign/VAlign are
  ByteProperty referencing authentic ExtensionObject.EHAlign/EVAlign.
- The eight copied flags are BoolProperty. PolyFlags, textPolyFlags and both
  multipliers are IntProperty. The three scalar spacing/baseline/underline
  values are FloatProperty.
- tileColor/TextColor are StructProperty -> Core.Object.Color;
  tilePlane/textPlane are StructProperty -> Core.Object.Plane.
- normalFont/boldFont are ObjectProperty -> Engine.Font; underlineTexture is
  ObjectProperty -> Engine.Texture.

Consequently a portable adapter must copy these actual source properties and
journal writes to the actual destination properties in the same execution
root. A separate presentation context or detached snapshot is not their live
authority. Pointer fields require real owned/object references and ordinary GC
reachability; copying their textual paths into an untracked rendering cache
would not implement this contract. Private Canvas remains inaccessible to
arbitrary script writes even though the typed native copy can access it.

## Explicitly untouched protocol and object identity

CopyGC stops at `+A0`. It does **not** copy or initialize any of:

| Retail slot | Actual protocol property |
| --- | --- |
| `+A4`, mask `01` | bFree |
| `+A8` | gcCount |
| `+AC` | gcStack |
| `+B0` | gcFree |
| `+B4` | gcOwner |

All five are real dimension-1, flags-zero declarations: bFree BoolProperty,
gcCount IntProperty, and the three links ObjectProperty -> GC. Other bits in
`+A4` are untouched as well. UObject identity, class, outer, name and flags
before `+28`, padding around Style/alignments, and upper unspecified bits of
the two copied packed-flag words are likewise not copied. Source protocol
links never overwrite destination stack ownership or list membership.

PushGC/PopGC explicitly update these separate fields around context copies.
For example, PushGC at `100278D0` may copy into a reused free node or construct
a copy-initialized GC, then sets its owner/count/links and changes the owner's
stack/count. PopGC at `10027A80` unlinks actual saved contexts, updates counts
and free-list fields and finally calls CopyGC from the last popped context.
Replacing CopyGC with assignment of the entire object would corrupt that
protocol. This audit is not a claim that the complete stack service has been
implemented or tested.

## Implementation and test implications

The existing typed `NativeWindowWrite`/DescribeProperty/Touch execution path
can represent these semantic copies, provided it authenticates both receivers,
their declaration types and their live ownership. Source None is dereferenced
by the original method; the safe portable equivalent must reject it with a
transactional error, not fabricate success or a default context. Source and
destination errors, budget exhaustion or later callbacks must leave all
destination values, ownership/GC edges and root counters recoverable by the
same outer journal. The copy itself creates no GC nodes, releases no context,
invokes no script and queries no font/texture/Canvas service.

Before accepting an implementation, exercise distinct/self sources; every
Bool combination with unrelated destination flags retained; all Byte values;
raw scalar/Plane words; Color alpha; deliberately inconsistent Style/flags and
Color/Plane pairs; None/invalid/wrong-class sources; preserved protocol fields
and identity; live source reference replacement; exact write/work/beforeimage
cutoffs; cold/warm owned-root rollback; and object reachability/unload. Invoke
the authentic native 1270 and original GetGC/Push/Pop callers, not just a pure
detached struct-copy helper. This document adds no compiler input, runs no
gameplay regression and makes no APK claim.
