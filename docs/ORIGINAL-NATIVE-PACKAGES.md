# Actual native Packages and source ownership (2026-10-18)

This implements the bounded Package dependency described in
ORIGINAL-PACKAGE-LOOKUP-AUTHORITY.md. Original commercial executables/libraries
are neither executed nor loaded. It is not Engine initialization, world
startup, physics or a complete implementation of StaticFindObject.

The audited original Core.dll was re-fingerprinted read-only for this phase:
SHA256 `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

## Explicit setup phase

`PreparePortableRuntimeNativePackages` requires the actual five-class Engine
and eleven-class Core metadata phases. It does not silently prepare either.
It allocates a genuine class-owned Package default RuntimeObject and actual
Core.Package instances with independent process-local allocation leases.
Packages are not export-owner `RuntimePackage` containers, exported UClasses,
map Actors, path hashes or fabricated serialized records.

The closed typed default-copy helper validates actual Object/Package IDs,
Super/Within relationships and the audited Object28h -> Package34h profile,
including their shared empty static constructor. Its immutable prepared tail
is handleAbsent/attempted0/PackageFlags0. Instance construction copies those
defaults before applying its explicit binding policy. This is not a generic
zero-filled default buffer or permission to initialize unrelated native types.

`NoLegacyModuleProvider` records a completed missing-module attempt for a
top-level Package: handleAbsent/attempted1/PackageFlags0, requested object
flags4. Repeated binds do not retry. The standalone helper also tests the
different Transient flags0 request and nested-Outer no-bind behavior; this
runtime phase does not create a Transient Package. No native module handle,
symbol registry, configured driver constructor or successful DLL binding is
invented. Mutable reflected Package storage remains explicitly unavailable.

All default allocation, identities, relationship validation, index copies,
detached return snapshots and retained-state admission precede publication.
Repeated preparation returns the existing graph. Detached snapshots do not
mutate it. The class-owned default is not inserted as a named export or Package
lookup candidate.

## Real Outer and linker-root authority

Native registration records now carry their independently audited package
literal. Core, Engine, Extension and DeusEx registrations bind actual shared
Package objects through that literal, not by splitting arbitrary display paths.
Later config-class and native Client/Viewport class registrations preserve this
ownership after the phase is active.

The integration test also exposed an older metadata binding gap: native Core
field classes were registered after loading their serialized instances, leaving
those instances' actual Class pointer null (including Engine.Engine.Cylinder's
ObjectProperty). Loading now caches a closed native metadata-class import ID
only after validating the actual table Class reference. The explicit metadata
phase stages and binds those imports to its genuine registered UClasses. This
does not turn a leaf metaclass spelling into authority, synthesize unregistered
types or change the eleven-class registration count.

Initial source-linker identities are captured from explicit loader inputs.
The exact serialized ObjOuter0 provenance is cached while original tables are
available. Only those exports receive that source's actual LinkerRoot. Nested
nonzero Outer references retain their resolved object; an unresolved nonzero
reference is never reinterpreted as a top-level root. General zero Class/Base/
object references remain null. Source reflection text is not rewritten into
ownership authority. Runtime births copied/restored in an active map use its
authenticated current-map root.

`RuntimeObject::Mark` directly traces Outer, as well as Class and existing
relationships. Initial source linkers and the current map linker trace their
real roots. The package index and canonical/folded aliases are non-owning;
merely being known by name does not permanently root every visited map.

## Retirement and failures

Map unload removes aliases by actual retired export identity, rather than
deleting every name with the same prefix. It revokes map-linker ownership even
for an empty source. A Package still reached by a retained native UClass Outer
survives; an otherwise unreachable map-only Package is collected. Its destructor
removes weak registry/lookup entries only when they still identify that object.
Cleanup control is disabled before shutdown replaces the lookup maps, preventing
an old session's destruction from erasing a replacement identity.

Map names colliding with initial script linkers, and export names colliding with
retained non-map objects, refuse before replacing the current map. Decode
exceptions after replacement retire newly published exports/root ownership.
This does not promise transactional restoration of the previous map for every
possible subsequent I/O/allocation failure; per-map archives/full travel remain
separate work.

The embedding lifetime above is explicit. It does not claim the uninspected
retail GameEngine travel sequence or its keep-flags policy. A collected index
slot can be reused by a later allocation; it is not a persistent identity.

## Verification scope

- `quest_native_package_test`: independent exact defaults, class/outer/flag
  profiles, constructor ordering, absent module/no-repeat, nested/no-bind,
  detached copies and malformed profile/request refusals.
- `portable_native_packages_test --game-root OWNED_ROOT`: actual copied original
  Core/Engine metadata plus read-only Training export ownership. Also exercises
  synthetic nested/dotted-leaf/unresolved-nonzero Outer identities, map-only and
  native-retained roots, warm checkpoint restore, empty/failed loads, name
  collisions, repeated phase/GC and shutdown.

Owned integration passes 11,377 checks with 13 explicit refusals. The Training
census is 3,347 source-root exports and zero nested exports; nested ownership is
independently exercised by original Engine fields and synthetic map records,
not falsely attributed to Training. The minimal fixture adds exactly five GC
objects: four Packages and one actual class-owned default. The independent
default/binding policy test passes 41,356 checks with 31 refusals. Original source
bytes/size/mtime and isolated source copies are unchanged.

The final-source owned Core metadata regression passes 10,994 checks/94 refusals
and config-class regression passes 9,971/48. Strict ARM64 runtime/test syntax and
the actual Android native module/link pass. The source-only setup phase does not
require or produce a replacement APK.
The final-source original isolated crate/fragment regression also passes 1,116
checks with four explicit rejection controls; it does not demonstrate live VR
Fire, begun-world physics or contents/loot callbacks.

The broad regression initially found one outdated FastTrace fixture contract:
it expected a late malformed map to leave Actors behind until explicit unload.
Immediate failed-load retirement now requires no map Actors, exact original GC
baseline, an unavailable Driver receiver, no native Model/cache authority and a
zero-export second unload. The updated strict fixture passes independently;
valid same-filename reload must still recover its actual Model and geometry.

The final coherent full host build passes. Fresh CTest runs all 151 registered
tests: 130 pass, 21 owned-data fixtures explicitly skip, zero failures in 33.89s.
The relevant owned fixtures above were executed separately; this does not claim
every skipped original-game fixture ran.

These are CPU identity/default/ownership tests, not playable Training acceptance.
The explicit phase is not yet called by Quest startup. Full original lookup,
configured native subclass/default import, actual Engine/Cylinder construction,
world BeginPlay, movement/gravity and live original crate Fire remain unfinished.
No APK is needed to claim this CPU-only setup API as verified.
