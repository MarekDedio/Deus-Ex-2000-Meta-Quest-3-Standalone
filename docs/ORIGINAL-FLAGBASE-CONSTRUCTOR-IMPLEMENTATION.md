# Original PlayerPawnExt / FlagBase constructor work (2026-10-29)

Implementation checkpoint, not completed startup/live Quest acceptance. The
source contract remains in ORIGINAL-PLAYER-FLAGBASE.md. This work uses only
original packages and static disassembly; commercial DLLs/EXEs are never run.

## Actual default and constructor authority

PreparePortableRuntimeFlagBaseClasses authenticates loaded original FlagBase and
ExtensionObject class/default identities, ancestry, class flags112, Within
Core.Object, ConfigNameSystem, empty defaults/patches/script and actual typed
field schema. It explicitly prepares a typed native CDO tail from the recovered
ExtensionObject28 -> FlagBase12C zero-extension rule, independently of the
instance constructor. All64 bucket heads and expiration are copied from that
prepared default, then constructor100239C9 rewrites expiration0.

The pawn field's own copy provenance is separately authenticated: Engine's
PlayerPawn registration103B7F74 supplies native size890; PlayerPawnExt
registration10037B45 supplies99C. The FlagBase pointer at890 is fully inside
that zero-extended tail. Original PlayerPawnExt class flags136, parent, Within,
zero script/default tags and actual10002 field declaration are checked, not
inferred from an absent map property. Nonnull concrete copied values are still
preserved only with authentic constructed native ownership.

SpawnNative runs the PlayerPawnExt constructor hook after actual class/header/
default/lookup admission, BEFORE owner/Spawned/initial-execution callbacks. A
nonnull copied FlagBase must identify an authenticated actual native object;
it is not replaced. Otherwise the constructor allocates a separate non-Actor
FlagBase with actual pawn Outer, requested flags0, concrete class counter and
original MakeUnique collision semantics. It is not a Window/Actor birth or
export/world slot. The typed pawn edge and bucket edges participate in GC.

Allocation joins the existing paged object-index root transaction before the
new pawn lease. Provisional roots/weak inventory, class counter, lookup aliases,
and pawn edge are unwound by the same host journal. Generic writes cannot patch
native pointer fields via script overlays. Cast/string/header reads recognize
constructed native objects rather than indexing a nonexistent authored export.

The new native graph deliberately refuses checkpoint capture/restore and map
travel until a real archive/purge/rebind service exists; the full campaign goal
still requires those services. Transient fields do not authorize losing state.
The production constructor is not replaced by a diagnostic object/root.
The standalone probe exposes an explicit caller-owned diagnostic GC lease in
Commit mode; releasing it permits normal collection and removes the weak entry.
Its initial failing commit-inventory assertion proved that no permanent root
existed; that discovery remains recorded, not counted as a passing test.

## CheckFlag and original hash

Native1120 authenticates original CheckFlag(Name80,Byte80)->Bool580,401/logical10.
The VM copies initialized Name0/Byte0 inputs in caller frame. Queries use actual
real FName identity and original first spelling, not a string-key pseudo-flag.

Core10149875..101498A8 constructs GCRCTable by shifting i<<24 left8 times with
polynomial04C11DB7, NOT the reflected EDB88320 polynomial used by ordinary CRC32.
Extension10024E60 then hashes ASCII-folded UTF16 low/high bytes with that table
and a right shift, initial0/no complement. Tests retain this unusual combination.
FindName uses64 buckets, signed hash comparison, exact FName index and unsigned
type comparison, preserving the true predecessor/insertion link on a miss.

The pure protocol handles chains longer than64, typed hits, collision misses,
signed/type early-exits, unavailable/stale providers and bounded cyclic/work
refusal. The initial runtime native table is genuinely empty from actual CDO
copy. Nonempty Flag/FlagInt graph handling explicitly refuses until its typed
constructors/destructors are implemented; it never fabricates a lookup miss.
GetInt/DeleteFlag hits/setters/expiration/iterators and native save/travel remain
required work, not inferred complete from the initial empty CheckFlag path.

## Current verification

Pure hash/lookup24 checks/4 refusal controls and generated VM701280/1039 pass.
The final focused desktop/strict ARM64/Android-native builds pass, and owned
constructor/query74 checks/9 refusals pass including lease release, actual
lookup/index/counter/GC/save rollback, typed fields and save/travel guards.
The final snapshot additionally authenticates the pawn zero-tail proof, charges
bucket validation and admits all snapshot strings/reallocation peaks before
allocations. Its1856 compiler inputs and45 original files are unchanged in the
length/UTC-timestamp/SHA256 audit. The first audit's string-versus-DateTime
comparison falsely reported timestamp differences; comparing actual UTC ticks
correctly verifies the same frozen files without refreshing the baseline.

Actual original first-Training discovery now constructs
00_Training.JCDentonMale0.FlagBase0, Outer=00_Training.JCDentonMale0,
objectIndex103162, flags0. It reaches the next unsupported native1455 GetTopChild
in HUDActiveAugsBorder.ClearAugmentationDisplay at PC15/opcode101, with266299
instructions/37893 writes after381.718s. The exit2 is a discovery refusal, NOT
successful startup. The probe verifies full save/state/revision/hash/RNG/client/
lookup/Actor/GC rollback,42 unchanged sources and independent cold Model ownership.

The initial full desktop run was interrupted at69%, before CTest. The initial
expected-boundary repeat2 reproduced the same1455 boundary/counts on attempt1,
then was interrupted during attempt2. Neither interrupted run is a passing
gate. A subsequent process inventory confirmed no surviving compiler/test jobs;
the unchanged-input incremental full build/CTest and fresh repeat2 now use the
separate flagbase-resumed-*-20261029 logs. The resumed full desktop build now
passes; CTest passes243 executed/294 registered/51 explicit owned-data skips,
zero failures,207.66s. The original-data repeat2 and separately invoked owned
Core SET9548 checks/6 refusals and Input ResetConfig9632/15 now pass on the
final snapshot. Both resumed Training attempts reach exactly PC15/native1455
with266299 instructions/37893 writes (428.827s and553.331749s), verify full
rollback and42 unchanged sources, and pass only the explicit expected-boundary
diagnostic. Actual constructor identity and cold Model-generation replacement
are checked. The final audit again verifies1856 compiler inputs and45 originals
unchanged and no live compiler/test processes before commit.
No new APK or live Quest crate/loot/gravity acceptance is claimed.
