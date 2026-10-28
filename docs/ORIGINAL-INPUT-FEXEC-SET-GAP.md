# Reached first-Training SET/config persistence dependency

2026-10-27. After original GetEnum is implemented,the bound original two-Fire
diagnostic reaches Engine.PlayerPawn.ConsoleCommand/native0 at271677 instructions/
32553 writes,with the exact refusal:
`Original viewport FExec fallback is not implemented: SET InputExt T Talk`.
Cold/warm repeat2 preserves both attacks,four traces,two crate changes,nine
births and full rollback. The separate unbound root completes its script and
reaches native root activation. These are different paths,not interchangeable
proof of successful startup. No new APK or live crate/loot/gravity is claimed.

## Preserve the real provider route

The route is actual Input.Exec ->RenDev.Exec ->viewport commands ->
Console.ProcessConsoleExec+48 ->UPlayer provider chain ->actual GameEngine.Exec
->UEngine.Exec ->Core.StaticExec. UEngine offers StaticExec at10380F44..4F via
IAT10599EB0. Do not jump directly from an unknown Input query to Core SET while
earlier providers are still unbound; retain an explicit refusal until each
required provider's authority and decline behavior is implemented.

The prior ORIGINAL-PLAYER-INPUT-CONSOLE.md +64 Console label was incorrect.
Viewport+64 is RenDev (assert literal1047BFE0,"RenDev",at10373775/10373973).
Canvas is+5C; actual Console is+34,constructed using UConsole105823E0 at
103896D5..10389702. Main-agent static inspection independently confirms the
RenDev literal. No original commercial code was loaded or executed.

## SET is global mutation plus a real save/flush

Core.StaticExec101532DA..101533B6 parses actual class/property tokens,uses
StaticFindObject(Core.Class,ANY_PACKAGE,Exact0),then inherited actual UProperty
resolution. It does not load a package or hard-code InputExt. It skips only
ASCII spaces before passing the entire remaining value text to import.

ParseToken10102B8F ->10147BA0..10147C81 is not the port's generic ReadToken:
spaces/tabs are skipped,capacity256 retains at most255 units; unquoted overflow
consumes the whole token,quoted overflow stops at capacity; quoted backslash
escapes the next unit when UseEscape1; an empty token returns false.

GlobalSetProperty101523A0..1015251A then:

1. Iterates actual indexed live objects in ascending index order,refreshing
   the count. For each target-class/descendant instance,ImportText(flags1),then
   its actual PostEditChange. Portable defaultOwner aliases are not live
   instances; original defaults have index-1.
2. Imports the target Class CDO separately.
3. Calls that CDO's SaveConfig(0x4000,None).

SaveConfig10151540..10151AAF exports EVERY Config field into the actual class
section Extension.InputExt/config User,not merely T. It uses GConfig.SetString,
then Flush(false,None) at10151930..3C. Afterwards it reloads CDOs under the highest
Config ancestor Engine.Input; there is no corresponding live-instance reload.
Ignored static evidence:core-getenum-next-set-static-20261027.txt.

UStr ImportText101679A9..10167A2A flags1 retains the whole CString; only bit2
tokenizes. Existing NativeConfigImport::String can be reused for this behavior.
UStr ExportTextItem10102356 ->101678A0 emits plain CString for flags0; bit2
uses literal quoted %s,without escaping. Alias Struct export10101AF0 ->10169710
iterates actual inherited members and each slot,using ExportText flags2; its
result form is (Alias=...,Command="..."). Config export must preserve these
rules rather than JSON-escaping values into a purported original INI.

## Required port implementation boundary

Reuse authenticated native Input fields,CopyInputDefault,Alias import and
LoadInputDefaultConfig. The existing config document is immutable and current
VM Touch/Before does not journal native Input/default/config mutations. Add
bounded beforeimages,ordered mutable config export and staged flush publication,
preserving live-instance ->CDO ->save/flush ->CDO-reload ordering.

Bind an explicit app-owned configuration backend. NEVER write the owned Steam
installation User.ini or mutate commercial source data to satisfy this call.
Read-only/rollback diagnostics must leave original sources and committed config
unchanged; successful game settings must persist through the actual port-owned
backend. Do not silently skip flush,mutate only the current viewport's binding,
or return fake success when persistence/provider authority is absent.

Actual Input.Init/Reset/platform/ReadInput and root activation/PreRender remain
separate required services; preserve original Init/Possess/Reset order. Their
current contracts are in ORIGINAL-ROOT-ACTIVATION-GAP.md and
ORIGINAL-TRAINING-CONFIGURED-INPUT-GAP.md. Immediate integration remains first
Training; no account/character-creation UI or other-map integration is needed.

## Follow-up implementation audit (2026-10-28)

Read-only review identifies these additional concrete backend constraints; none
is implemented by the root-activation batch:

- OrderedIni currently exposes immutable entries. A controlled copy-on-write
  SetString/export must retain ordering and duplicates, not rebuild from Scalar
  lookups. The explicit static Editor-profile implementation1021BF90..1021C2A3
  updates the newest matching duplicate, retains earlier entries and ignores
  case-only value changes through appStricmp. Its serializer1021A770 emits raw
  section/key/value CRLF records without JSON escaping. This profile is not
  proof of the packed retail launcher's config-factory binding.
- VM Touch/Before does not capture nativeInputInstance payloads, prepared native
  defaults, the domain config document or staged persistence generations. Add
  dedicated bounded first-touch beforeimages and allocation-free restoration
  before provisional objects are retired. Count both retained old state and
  new/staged serialization bytes in the shared state budget.
- Input schema passes==3 and InputExt constructor configReloads==2/configPasses==4
  are bootstrap evidence. Do not increment them as running SET counters and
  invalidate all existing schema/snapshot checks; use separate mutation/flush
  revisions and local per-reload invocation statistics.
- PublishSaveBundleFileAtomically's bool is not a transactional config decision:
  on POSIX, rename may publish successfully before directory fsync/close fails.
  Distinguish NotPublished, PublishedDurabilityUnknown and Durable, and finish
  memory commit without throwing once publication occurred. Do not roll old
  memory back over a newly published file. Rollback-only probes never stage or
  publish configuration at all.

Bind a canonical port-owned writable directory separately from the immutable
seed, rejecting unsafe links or equivalence to original files. Preserve genuine
live-instance -> target CDO -> export all Config fields -> flush -> ancestor-CDO
reload ordering; a one-binding mutation is not this service.
