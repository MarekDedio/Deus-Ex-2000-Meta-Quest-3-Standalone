# Native config constraint classes (2026-10-18)

The next explicit CPU phase prepares real indexed native UClasses for
Core.Language, Engine.AudioSubsystem, Engine.RenderDevice and Engine.NetDriver.
It requires the existing five-class Engine and eleven-class Core metadata phases;
neither prerequisite is silently prepared. Original registration evidence is in
ORIGINAL-NATIVE-METADATA-REGISTRATION.md. The existing counts remain five and
eleven; this domain has four wrappers and one owning GC object.

## Genuine class and Console authority

Each wrapper has an actual process-local allocation-index lease, original native
size/constructor flag baseline, indexed superclass/Within and actual Core.Class
metaclass. The shared class-only tag and fixed-record snapshot validation do not
introduce a fabricated serialized descriptor, export, default block or instance.
All allocations, copied indexes, detached return data and retained-state admission
precede the single nonthrowing publication. Failed preparation publishes no prefix.

Engine.Console is NOT replaced by a native wrapper. The phase requires its real
serialized Class, direct Core.Object base, actual Core.Class metaclass, original
ClassFlags13A/UObject flags040F0004, decoded Within and config FName. Within must
resolve through its original declaring table as an actual Class reference to the
indexed Core.Object; config is System. Only that validated native Within pointer
is bound on the existing Console at publication. Descriptor, flags, Children/Next,
default object and authored defaults remain unchanged.

Native classes persist through checkpoint and map replacement, with genuine GC
class/base/Within edges; shutdown removes their root. Both lookup-count bounds
are checked before allocation. Shared retained-state accounting now includes the
heap-owned GCRoot wrapper and its separate GCRootNode, fixing a32-byte omission
per class domain on64-bit hosts.

## Serialized Within/config retention

PortableClassDescriptor now preserves optional classWithinReference and configName.
For version>=62, the first is a bounded signed object reference and the second a
validated FName. Explicit reference0/config None remain present, distinct from
pre62 serialized absence. The raw decoder does not invent runtime fallback or
resolve a referenced object's class. The typed Console binding performs that
separate authority check.

The original UClass.Serialize reference/FName calls and actual pre62 loader
fallback are recorded in ORIGINAL-PACKAGE-LOOKUP-AUTHORITY.md. The previous
"package table index, not object reference" comment was incorrect.

## Verification

- Generated descriptor tests:704 checks/196 rejection controls, including
  version61/62 boundaries, explicit null/None, local/import/multibyte indices,
  invalid signed bounds/names and truncated class suffix/defaults.
- Independent owned suffix audit: all1502 serialized Classes across38 System
  packages, plus261 actual States. The existing map census inspected88 maps
  (no Class/State exports); this is not a playable-campaign audit.
- Owned config-class integration:9971 checks/48 explicit refusals. Actual
  indexed relationships, exactly+5 GC objects, original Console state/defaults,
  dormant complete checkpoints/model/world/clock/revision, warm restore and
  map/shutdown lifetime, missing prerequisites/Console and late-name rollback.
- Existing owned Core metadata regression:10994 checks/94 refusals.
- Strict ARM64 syntax and actual Android native module/link pass. Full coherent
  host build/CTest results are recorded in STATUS.md when finished.

The late-name sentinel exists only in an in-memory table with no on-disk export
slot. Its cast therefore explicitly refuses source provenance; allowing it as
a constructed Engine instance would be an invalid fixture assumption. No source
file is modified to manufacture a successful cast or native Class.

## Required next boundary

This does not implement actual Package instances/Outers or StaticFindObject.
The new package audit proves those are required: original export Outer0 becomes
actual LinkerRoot, and native registration creates real packages. Qualified
lookup resolves actual Outer objects before its distinct ANY_PACKAGE fallback.
A string-qualified class map cannot stand in for that ownership/lookup service.

Configured GlideDrv/Galaxy/IpDrv subclasses, Language=int resolution, native
fields/default storage, frozen instance copies, Engine/Transient/Cylinder and
Level.Engine lifetime remain unfinished. Collision hash/Move/support/startup/
player/falling and the original live crate damage graph still follow. These
class and decoder checks are not headset physics acceptance; no new APK is
produced for this explicit CPU-only phase.
