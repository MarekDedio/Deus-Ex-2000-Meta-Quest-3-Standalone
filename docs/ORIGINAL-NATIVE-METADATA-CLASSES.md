# Indexed native Core metadata classes (2026-10-18)

## Implemented phase

`PreparePortableRuntimeNativeMetadataClasses()` explicitly requires the existing
five-class native Engine registration phase. It adds the eleven audited Core
metadata UClasses from ORIGINAL-NATIVE-METADATA-REGISTRATION.md as a separate
closed, rooted, retained-state-accounted domain; it is not a missing-type factory.

Each wrapper has a real process-local UObject allocation-index lease and actual
indexed superclass, Within and Core.Class pointers. Core.Class is its own
metaclass; Class.Within is the existing Core.Package; property-class Within is
Core.Field. Native size and constructor flag baselines remain separate from
serialized class flags. There is no fabricated source/export, descriptor, CDO,
package Outer or native-instance byte buffer.

The phase stages wrappers, both indexes, dependencies, a detached snapshot and
retained-state admission before publishing anything. Existing serialized UClass
metaclasses are bound from cached actual ObjClass-zero authority, not a "Class"
leaf-name guess. Metadata-only Class exports in subsequent maps are included.
Existing native Client/Viewport classes receive the same actual metaclass;
classes created later bind it during their own staged preparation.

Direct GC edges retain class/base/Within relationships, including cycles. Map
unload and checkpoint replacement do not remove or replay the registration.
Shutdown clears its root with the remaining runtime; failed admission collects
only staged allocations. Both index-growth counts are admitted before allocation
against the existing one-million lookup limit. Loaded FName display casing does
not change identity comparisons.

## Preserved unsupported boundaries

The five-class Engine snapshot remains five records. Static native UClasses are
not map exports, Actors, dispatch classes, default blocks or save-manifest births.
Their reflected/default storage, class-default aliases and authored descriptors
remain explicitly unavailable. Existing source-package/default field chains are
not reconstructed or altered. Cast/string/typed-value admission recognizes the
new actual wrappers without trying to open an absent source package.

The native class-only phase is an explicit CPU API, not yet an application
startup/default/physics coordinator. It does not establish the four missing
config constraint classes, final native field/default storage or an Engine and
Cylinder instance. Those remain necessary before the original collision hash,
Move/support/startup/possession, falling and live original crate damage can run.
No generic crate health/hide or cosmetic falling is substituted.

## Verification

The owned integration test uses independent original header literals, actual
shared relationship indices, full checkpoint/source fingerprints and GC/world
observations. It covers all eleven records, class self-identity, idempotence,
detached snapshots, map unload/reload and warm save replacement, a later map
Class without a descriptor, session classes prepared before/after this phase,
missing prerequisites and a late final-name collision with atomic rollback.
The late-name collision adds a name and export only to an in-memory table backed
by an isolated unchanged Core copy; it never alters original package bytes.

The owned test passes 10,994 checks with 94 explicit refusals. The original
isolated crate/fragment regression passes 1,116 checks with four refusals. Strict
ARM64 syntax and actual Android native module/link pass. The full coherent host
rebuild passes. CTest ran 148 registered tests: 129 passed, 19 owned-data fixtures
explicitly skipped, zero failures in 54.69 seconds. Relevant owned fixtures above
were executed separately against actual data; the full skipped set is not claimed
as executed. Owned Training primitive regression also passes 74,660 checks with
246 explicit refusals. These checks are class/lifetime evidence, not playable
crates or headset physics acceptance.
