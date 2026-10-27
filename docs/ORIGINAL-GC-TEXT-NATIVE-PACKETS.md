# Original GC font/text native packets and hidden layout request

2026-10-27. This implements original startup dependencies, not VR HUD-layout
changes or completed live crate/loot/gravity integration. Original commercial
DLLs were inspected only as static data; no original native code was loaded or
executed. Only the first Training map is used for the integration controls.

## Authenticated native packets

native/portable_gc_text_natives.inl validates the real Extension.u function
identity, native index/flags, source, scalar slot flags, array dimensions and
actual property metadata before native ingress or initialized-input authority.

- GC.SetFont1242 has one required Object80/Engine.Font input, initialized None
  and copied by Step. It authenticates a non-None immutable actual Font asset,
  then always writes normalFont followed by boldFont, even for equal inputs or
  None. No layout, text spacing, drawing GC or style setter is substituted.
- GC.GetTextExtent1280 copies Float0 destWidth, captures separate Float0
  xExtent/yExtent temporaries and their actual OUT references, then copies its
  initialized-empty FString input. Ordinary width/String aliases are detached;
  the OUT aliases remain live. The algorithm writes actual X then Y targets at
  its final store points, not a post-hoc pair publication. Missing references
  select disposable temporaries. Direct ref.write uses the VM's existing guard
  exactly once; another Execution.WriteReference wrapper would double-journal.
- GC.GetFontHeight1281 uses the original optional-false Bool, maximum actual
  normal/bold SPACE glyph height and optional raw textVSpacing. None fonts
  produce zero glyph height, not guessed fallback font metrics.
- Window.ConvertScriptString1469 copies an initialized-empty FString and
  returns an independently owned FString after the existing original UTF16
  converter. Only lower-case |n becomes LF; other pairs/trailing bar, embedded
  CString NUL and surrogate-unit transport retain the established semantics.

The extent wrapper audit is in ignored
artifacts/gc-text-extent-wrapper-static-20261027.txt; the String wrapper/result
copy audit is in window-convert-script-string-wrapper-static-20261027.txt.
The actual Font cache, PC64 scanner/line/extent arithmetic, bounded scratch,
caller-root journal/work limits and scope cleanup are reused. No memory limit,
save wire format, original game data or accepted VR presentation was changed.

## Output order, copies and failure handling

quest_native_text_measurement.h exposes GetTextExtentTo callbacks at the exact
existing final X/Y steps; the original reference-output API delegates to it.
Y uses computed locals, so output aliases to GC.textVSpacing cannot change the
answer by rereading spacing after X. X failure stops before Y; a failure at Y
preserves X's preceding store until the enclosing transaction rolls it back.

The VM separately copies native1280 ordinary slots0/3 and captures slots1/2
once, retaining both Step values and OUT references. Declared, native-index and
direct Execution entry share these rules. Default seeds come from actual host
declaration/receiver authority, not native-index-only global defaults.

The generated measurement policy passes171650 checks/259 refusal controls.
The VM suite passes700012 checks/998 refusals, including3390/68 new GC/text
packet controls: source-frame receivers, one-read copies, later mutations,
ordered/shared/missing OUT references, NoWrite/zero-fill bounds, optional-false,
authenticated seed failure, late rollback and expired guards. These generated
tests use explicit fixtures, not original Font or headset evidence.

## Native Window1460 and actual original-data evidence

After the text calls, original HUDActiveItemsDisplay.SetVisibility reaches
Window.AskParentForReconfigure1460. Its owned declaration is native1460,
flags401, no parameters/return. Exec10052F30 advances EndFunctionParms and
calls1004E060. The new native ingress reuses the already-audited implementation:
always mark Self.bNeedsReconfigure/bNeedsQuery, then defer at any hidden Self/
ancestor. It does not invent parent callbacks, visible layout or successful
root activation. Unknown/all-visible paths retain their explicit refusal.
Static audit: ignored artifacts/window-reconfigure-wrapper-static-20261027.txt.

Owned GC integration passes155 top-level assertions/16 failed-root controls,
including68 Window and43 text cases,10842 writes/193900 shared native-work units.
It checks real Font glyphs, None/equal font stores, conversion ownership, shared
and discarded OUT references, wrong receiver/packet/type preconditions, repeated
layout dirty stores, cold/warm/save-reload and complete save/GC/counter undo.
The final write-budget cut fails at the actual final1280 Y store after X; whole-
root undo still restores both. Direct ingress guards are not separate failed VM
roots and are not counted as such.

The dormant original root now gets through mission-start preferred sizing,
HUD UpdateSettings and scope creation/Lower/final cursor selection. It reaches
Core.Object.GetEnum PC0/opcode0 in ConditionalBindMultiplayerKeys,15477 writes,
198 actual native objects and an empty inspection error within the unchanged
4MiB snapshot cap. All three saved text-query contexts are now released, not
one active mission-start stack. The diagnostic captures the new scope's actual
Player and Color fields. Original ammo/health SetVisibility only write script
bVisible, unlike the actual Window.Show users; their Window visibility stays
true. Later root activation/input/live publication are still not implemented.

Next contract: ORIGINAL-CORE-GETENUM-GAP.md. The committing live VR weapon,
frame/physics and actor publication work remains in TRAINING-FIRST-PRIORITY.md.
No account/character-creation UI, other-map integration or new APK is claimed.

## Final built regression evidence

The expanded owned HUD snapshot/rollback suite passes10175 controls. It keeps
all198 native objects and all27 GC context fields, checks the completed hidden
scope's Player/Color/alignments/Lower sibling order and original selected cursor,
and verifies the exact later GetEnum boundary rather than the old1242 refusal.
The change in total assertion count reflects the now-completed/released context
branch, not a smaller snapshot or omitted field set.

Full desktop build,current Android-native library link and strict ARM64 pass.
CTest registers269 tests:225 executed,44 explicit owned-data skips,zero failures,
164.27s. The separate original-data tests supply owned sources where applicable;
CTest's44 skips are not reported as executed coverage.

Original crowbar two-Fire cold/warm repeat2 reaches GetEnum at271515 instructions/
32539 writes, retaining48 observations,two attack notifications,four traces,two
crate changes,nine births and full undo. Door repeat2 retains both56-unit moves/
two endpoints and the exact unprepared ReadInput refusal at239536/19432. These
are explicitly rollback-only diagnostics, not live-controller/headset claims.

Ignored evidence uses artifacts/gc-text-native-*20261027.log. Earlier discovery
logs intentionally retain the old-boundary/snapshot expectation failures; the
passing HUD result is gc-text-native-hud-prefix3-20261027.log. The source snapshot
used for the final checks is gc-text-native-final-frozen-inputs-20261027.json.

All eight separate rebuilt owned first-Training regressions pass:crate415/5,
deferred429/21,animation588/24,FastTrace252/0,Input classes10413/38,viewport
10310/20,1232 interaction/save-route controls and material inheritance with1308
actors/227 selected materials/zero null materials. The explicit procedural
FireTexture fallback remains; this is not a claim that every visual issue is
fixed. Final audit verifies all45 originals/24 frozen compiler inputs unchanged
by length,UTC modification time and SHA256,with all jobs terminal.
