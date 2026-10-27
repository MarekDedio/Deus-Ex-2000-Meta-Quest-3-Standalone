# Original TextWindow content and accelerators (2026-10-27)

First-Training native1550 implementation. This is original startup dependency
work, not completed startup, live VR crate/loot/gravity acceptance or an APK.
Commercial original DLLs/EXEs are inspected only as static data, never executed.
The accepted VR presentation HUD layout and all save formats remain unchanged.

## Audited original contract

Extension.TextWindow.SetText is native1550, flags0x401. Its one required
NewText StrProperty is dimension1, flags0x400880. Wrapper10046A30 constructs
an empty FString before Step, consumes the input in the caller frame, calls
Window.ConvertScriptString10050710 and dispatches virtual+0x11C. NoWrite
therefore preserves the empty String temporary rather than a stale argument.

The actual+0x11C entries are Text5A574,TextLog5B758,Button5A440 and
Toggle5A8E4, all pointing to XTextWindow.SetText10045D50. Edit5B628 points
to1001E260 and LargeText5B504 to1002D6B0; those separate native overrides
are not silently treated as TextWindow. Original native class/receiver,
declaration/source and private field identities are authenticated at ingress.

ConvertScriptString10050710..10050957 scans UTF16 CString code units. Only
lowercase |n becomes LF. Other bar pairs consume/preserve both units, so ||n
does not become a newline; a trailing bar is retained. An embedded NUL ends
the input. Original1024-unit chunk boundaries do not change these results.
The port converts in place in a bounded per-call owned UTF16 buffer, because
no original DLL/global heap pointer is exposed to this synchronous VM call.
It retains UTF16/WTF8 transport semantics rather than indexing UTF8 bytes.

XTextWindow.SetText10045D50 first calls actual Core.appStricmp (IAT100590C8,
Core thunk1010239C ->10123560 ->MSVCRT._wcsicmp). It uses the same explicit
default-C-locale profile already documented in ORIGINAL-NATIVE-DEFAULT-CONFIG.md
and STARTUP-PRIMITIVES.md, not a dynamic Unicode/locale substitution. Equal
case-insensitive CStrings cause no text store, accelerator update or layout.
Changed text replaces private Text (StrProperty flags0x1000), then reads
bTextIsAccelerator (Bool flags2). If enabled, SetAcceleratorText1004F310
scans for the first |& pair, respecting skipped nonmatching bar pairs. The
following UTF16 unit is the key; missing or unsigned values>=255 select zero.
The first invalid key still wins; scanning does not skip ahead to a later key.

SetAccelerator1004F3B0 always stores the complete Int acceleratorKey, even
equal. Only a changed key calls1004C1D0, which marks the actual enclosing
ModalWindow.bDirtyAccelerators. Native reconfiguration1004E060 follows the
text/accelerator sequence. Current hidden ancestors defer layout; the separate
unimplemented visible-layout branch still propagates its explicit failure.
There is no invented rendering, shortcut binding or local catch/rollback.

## Ownership and integration

The pure helper quest_native_text_content.h implements code-unit conversion,
accelerator scanning and setter read/write/callback ordering. The actual host
adapter portable_text_content.inl uses existing typed private-field writes,
Modal ownership and native reconfiguration in the caller's same Execution.
VM ordinary-input snapshots and genuine empty-String seed cover both declared
and native-index calls. Shadowed private Text, foreign declarations, malformed
input/receiver and unaudited native virtual overrides fail explicitly.

Borrowed source text is not copied before admission. A scoped shared scratch
reservation covers copied input, old/new UTF16 decoding, conversion, UTF8
encoding and overlapping growth through nested writes/callbacks. Byte addition/
multiplication is checked, work is charged and existing root/live/snapshot caps
remain unchanged. The transaction owns native fields, graphs, callbacks and
whole-state beforeimages; the helper never restores only its own local fields.

## Accepted targeted evidence

The expanded existing setter policy suite passes886077 checks/34 explicit
refusals. It includes111111 generated bar/NUL/newline/surrogate inputs, original
chunk-boundary comparisons, all65536 accelerator code units,65536 byte-pair
case/equality comparisons, exact event order, equal/disabled accelerator keys,
callback propagation and work/write cuts. Providers and outer undo are explicit
mocks; these counts alone do not prove native UObject or headset behavior.

The real GC/Text fixture passes155 top-level assertions/16 failed-root controls,
now10739 writes/190525 work, with18 text cases. Actual SetText supplies the
nonempty original Font sizing fixture. It also covers native declaration/type/
receiver guards, conversion, unchanged case, first key, disabled accelerator,
NoWrite empty temporary, cold/warm save/reload, early/late root cuts and full
save/object/GC/counter rollback. Direct precondition guards are not advertised
as independent failed VM roots.

VM696529 checks/922 refusal controls pass. The complete193-object owned HUD
prefix passes9679 controls within the unchanged4MiB snapshot budget. The
received-item label contains its actual localized CDO string, all HUD controls
complete, and Root.hud is assigned. Original UpdateSettings then reaches
HUDObjectBelt.SetVisibility PC0/opcode101, unsupported Show1413, at14117 writes.
The older1550 expectation was updated only after observing this exact boundary;
failed discovery runs are not passing evidence. Strict ARM64, current Android
native library link and full desktop build pass. Registered CTest passes225
executed/269 registered/44 explicit owned-data skips/zero failures165.61s.

Original two-Fire cold/warm repeat2 reaches the later1413 boundary at254688
instructions/30914 writes. Both retain48 observations,two attack notifies,
four traces,two crate changes,nine births and full save/world/GC undo. Door
repeat2 retains the exact unprepared ReadInput refusal at239536/19432, both
56-unit moves/two endpoints and full undo. Only first Training is loaded.
All eight separate rebuilt owned first-Training regressions pass:crate415/5,
deferred429/21,animation588/24,FastTrace252/0,Input classes10413/38,viewport
10310/20,materials and1232 interaction/save-route controls. The final audit
confirms all45 originals/23 frozen compiler inputs match length,UTC modification
time and SHA256 baselines. All compiler/test/probe jobs are terminal.

## Next actually reached service

Extension.Window.Show1413 has flags0x401, optional bShow Bool dim1/flags0x90.
Original exec10051960..100519A3 seeds INT1, then calls SetVisibility1004C7D0,
not the distinct exported no-argument XWindow.Show convenience function.
SetVisibility compares the canonical local visible bit first. Equal input
returns without parent notification. A changed request dispatches actual parent
virtual+0x84 under the original callback context, or calls direct1004EE20 when
unparented. The existing hidden-window visibility/parent protocol must be
reused/audited with these exact ingress/default semantics; no unconditional
show, equal-input callback or fabricated effective-visible service is justified.

No new APK was packaged or installed. Original startup/input and committing VR
weapon/frame/publication remain unfinished. No account/character-creation UI
or other-map integration is being added.
