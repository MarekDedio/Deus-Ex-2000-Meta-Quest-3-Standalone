# Original package and lookup authority (2026-10-18)

Read-only static prerequisites for actual Package instances and config class
lookup. This document does not implement those services, native defaults,
registration replay, module loading or Engine/session initialization. No owned
commercial DLL/EXE was loaded or executed. Addresses below are preferred PE VAs
and offsets in the original 32-bit ABI, not portable pointers/layout.

Owned Core.dll SHA256, rechecked before/after inspection:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

## StaticFindObject is an actual Outer/name lookup

`StaticFindObject10151DE0..10151FD0` saves original InOuter at10151DFE and maps
ANY_PACKAGE(-1) to a local null Outer before calling ResolveName10154AD0 with
Create0/Throw0. It does not compare a preassembled canonical object-path string.

For each dotted prefix, ResolveName searches under its current actual Outer:

1. Find a Core.Package-or-subclass object at10154CAA..10154CC0.
2. Otherwise find a Core.Object-or-subclass object at10154CC7..10154CDA.
3. Fail if neither exists, at10154CDF. Otherwise store that actual object as
   the next Outer at10154CE8 and advance beyond the dot.

The leaf uses FName(FNAME_Find) at10151E33..10151E46; absent/None FName returns
null without adding a name. The exact resolved-Outer hash search is first:
`(Outer.Index xor Name.Index)&0xFFF`, with null Outer contributing0, at
10151E4C..10151E64. Candidates must match actual Outer pointer and FName, then
the requested exact concrete class or actual Class/Super ancestry at
10151E6F..10151EB5. Null requested class imposes no class filter in this exact
hash phase (direct return at10151E79..10151E7B).

Important ANY_PACKAGE nuance: only after this hash search misses, and only if
the ORIGINAL InOuter was-1,10151EB7..10151F88 scans nonnull indexed GObjects in
ascending index order. A separate iterator first admits actual
Core.Object-or-derived objects, even when the requested class is null:
10151EC0..10151ECE sets the iterator filter to actual Core.Object1029E0D0 and
index-1;10151EED calls thunk101028AB ->10143EE0, whose first increment follows
candidate Class+24/Super+28 at10143F04..10143F16. Subsequent increments inline
the same ancestry admission at10151F6F..10151F88. Null Class is not an admitted
iterator object. The admitted candidates then match leaf FName and the optional
requested-class filter, but do NOT enforce the resolved Outer. The null
requested-class shortcut at10151F15..10151F17 skips only that latter filter.
See `ORIGINAL-OBJECT-HASH-ORDER.md` for the complete phase distinction. Thus:

- Unqualified ANY_PACKAGE first prefers the matching null-Outer hash entry,
  before ascending global fallback.
- A qualified Engine.Foo query must resolve an actual Engine prefix first.
  Failure there returns null before any leaf fallback.
- After Engine resolves, an ANY_PACKAGE leaf miss may find another package's
  Foo. Do not silently restrict that fallback to Engine.

ResolveName also has a distinct `ini:`/`usr:` branch10154B19..10154C4C, using
GConfig and verified System/User filename literals101AF18C/101AF19C. A bounded
portable profile must implement this branch or explicitly refuse it rather
than silently treating it as an ordinary name. StaticFindObject itself does
not create/load packages, execute constructors or grant class capability to
an entry found only in a string-path map.

## Original native registration and serialized loader binding

Native UObject.Register10152550 receives the native-construction package-name
string temporarily stored in Outer.1015259A..101525A8 calls
CreatePackage(NULL, stored package name), then101525B0 replaces Outer with the
returned actual UPackage. It makes the actual object FName at101525B3..101525C7,
rejects an existing same-name/Outer object at1015260E..10152630 and adds the
registered object to indexed/hash authority at10152650..10152654. This is
evidence for converting the audited registration's package literal into a real
Outer, not permission to infer Outer from an arbitrary display-path prefix.

Original ULinkerLoad.CreateExport1012E230 distinguishes serialized ObjOuter0
from a null runtime Outer.1012E32B..1012E335 resolves nonzero export.ObjOuter
through IndexToObject;1012E33E instead selects actual LinkerRoot at linker+28
when export.ObjOuter is zero.1012E34C..1012E36A passes that actual Outer into
object construction. Consequently a top-level loaded Engine.Console has the
actual Engine Package as Outer. Preserve file ObjOuter0 as provenance, not as
proof that the loaded object's runtime Outer must remain null.

IndexToObject10102C11 ->1012E550 handles positive export(index-1), negative
import(-index-1) and zero null. The zero-Outer-to-LinkerRoot rule belongs to
CreateExport; do not apply it to general zero serialized object references.

## CreatePackage, defaults and constructor state

CreatePackage10101CCB ->101549F0 resolves with Create1/Throw0, finds an actual
Core.Package-or-subclass under the resulting actual Outer/name, and returns an
existing match. The missing case10154A47..10154A6E allocates native size34 hex,
actual resolved Outer, FName(Add1) and requested UObject flags4, then invokes
UPackage's actual constructor. Core.StaticInit's separately audited Transient
creation uses flags0; do not reuse that different request for Core/Engine
packages. StaticAllocateObject10157ADA..10157AE7 permits null Outer for exact
Core.Package; other classes require an actual Within-compatible Outer.

The native Package initializer1015A710..1015A75B supplies size34, Super and
Within actual Core.Object, config System, owning package Core, supplied class
flags0 -> constructor12 and class-object flags04084004. Package and Object
share StaticConstructor10101041 ->1014F690, a single RET. Core.Object's native
initializer10150CC0..10150D0C supplies native size28.

These particular native sizes and the original default-copy phase establish
Package's three fresh scalar tail values; a generic property.zero substitute
does not. UClass.Register allocates the real default buffer at10127851..1012789F
and invokes InitClassDefaultObject at101278A2. Its body10152300 zeros the
28-byte header, binds actual Class/index/vtable, then calls InitProperties
with actual Super at10152346..10152355. InitProperties101520D0 copies inherited
default payload after header28 and zeroes only the newly extended tail at
10152149..101521A7. Package's equal inherited static-constructor pointer skips
a redundant call at101278A7..101278BE. This audited Object(size28) ->
Package(size34) profile therefore begins with:

| Package offset | Meaning | Fresh prepared default |
| --- | --- | --- |
| 28 | Native library handle | 0 |
| 2C | Binding attempted | 0 |
| 30 | PackageFlags | 0 |

Instance StaticAllocateObject calls InitProperties with the prepared actual
class defaults at10157EDD..10157EF1, before the instance constructor runs.
The Package constructor1015A3E0 writes its vtable at1015A40E, calls
BindPackage at1015A418 and unconditionally returns this at1015A423..1015A432.
It does not itself initialize these tail fields or run a module-gated allocation
rollback. Complete typed default/instance preparation is still required before
claiming this original allocation path is implemented in the port.

## Missing native libraries are a legitimate Package state

BindPackage101017E9 ->10152FF0 is exported as a VOID function. It does nothing
if handle+28 is nonzero, Outer is nonnull or attempted+2C is nonzero
(1015301E..10153039). Otherwise it forms the platform native-library path,
sets attempted1 at10153082, calls appGetDllHandle and stores its result in+28
at1015308D. A zero result jumps directly to the normal return at10153098..CA;
there is no failure assertion or Package destruction. A successful result logs
and processes native registrants at101530C5. PackageFlags+30 is not modified.

appGetDllHandle1010181B ->1016C150..1016C455 actually calls LoadLibraryW/A
(IAT102AB3F4/102AB3F0), with path variants, and normally returns zero if all
attempts fail. It has no portable static-native registry in this original
body. This audit NEVER invokes that function or loads the original DLLs.

A real Package may therefore exist with handleNone/attempted1. That state
permits package identity/Outer lookup but does not grant native symbol or
constructor capability. GetDllExport1015A5A0 distinguishes absent handle and
missing export, returning null or reporting a required-symbol error depending
on its caller's requirement flag; a usable package name is not a module handle.

Portable integration must explicitly identify its platform policy. A safe
missing-module result may leave legacy handleNone and record its completed
portable binding attempt, without claiming that Windows LoadLibrary ran.
If statically implemented port-native modules are bound, use actual validated
registry entries/callback ownership and a separately typed port-native binding
result; never fabricate an HMODULE, mark every package successfully bound,
or treat a class-only metadata registration domain as a module-symbol registry.
The explicit policy must preserve attempted/no-repeat behavior and distinguish
ordinary missing-module outcome from missing required native capability.
This document supplies evidence, not that registry/service implementation.

UPackage.Serialize1015A470 only calls UObject.Serialize; its handle, attempted
and PackageFlags tail are not serialized UObject-reference edges by that body.

## GC and unloading: identity is not an automatic permanent root

CreatePackage and UObject.Register do not call AddToRoot on every package.
Transient is different: Core.StaticInit stores its actual object in
GObjTransientPkg102A2660 and roots it at1015283D..10152843. AddToRoot10157400
and RemoveFromRoot101574B0 manipulate actual GObjRoot1029E5F8.

UObject.Serialize traces actual Outer+18 and Class+24 during reference/GC
serialization at10150278..10150289. A retained native UClass therefore has a
real reachability edge to its owning Package once correctly bound; a string
canonical path cannot supply that edge. ULinker.Serialize1011DF90 explicitly
traces LinkerRoot+28 at1011E01B..1011E021, in addition to its import references.

GObjLoaders1029E608 is an actual loader list, not by itself proof that each
loader/package is a permanent GC root. GC's SerializeRootSet101580B0 traces
GObjRoot at101580D4..101580E1, then actual indexed objects admitted by caller
keep-flags and required flags at10158148..1015816E. Package survival must follow
real current roots, object edges and admitted flags; do not infer it merely
from the package's name, a loader-list entry or a map being previously loaded.

ResetLoaders10103319 ->10155750 iterates real loaders, filters by exact actual
LinkerRoot at101557F1..101557FB, optionally detaches applicable exports at
10155804..10155838, optionally closes the loader and destroys it at
1015583A..10155854. It does not directly delete the Package object or erase
all descendants simply because a map is leaving. Later Package lifetime remains
the reachability/GC question. This bounded Core audit does not establish the
entire GameEngine travel caller sequence or its precise keep-flags policy.

A portable package domain must therefore keep persistent native class/package
roots separate from map-scoped loader/export ownership, trace genuine Outer
and Class edges, and revoke obsolete loader/map ownership without destroying
packages still retained elsewhere. Root/reload/GC tests must demonstrate the
implemented embedding lifetime rather than claim uninspected retail travel.

## UClass serialized Within and ConfigName

UClass.Serialize101280D0 archives version>=62 Within at this+49C through the
UObject-reference slot+18 at10128166..10128176, then ConfigName at this+4A0
through the FName slot+1C at10128179..10128184. Within is an actual serialized
object reference, not an unrelated raw package integer to discard. Preserve
its compact signed reference and resolve/validate actual UClass authority.
There is no version>=62 null-to-Object fallback.

Version<62 does not serialize Within and creates ConfigName System from the
verified literal101A9DDC at10128189..1012819A. On LOADING version<=61 only,
101282E3..101282EF sets Within to actual Core.Object1029E0D0. Do not apply this
legacy behavior to v68 null references, and do not confuse Within references
with CreateExport's separate serialized Outer0-to-LinkerRoot rule.
