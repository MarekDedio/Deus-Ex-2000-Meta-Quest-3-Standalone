# Original UObject hash and index ordering (2026-10-18)

Read-only static audit of the owned 32-bit `System/Core.dll`. No commercial
DLL/EXE was loaded or executed. Preferred PE virtual addresses and original
32-bit offsets below are evidence, not portable pointer/layout definitions.

SHA256 before and after inspection:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

Inspection used MinGW `objdump -p`, `objdump -d -Mintel` with explicit
`--start-address`/`--stop-address`, and bounded `objdump -s` data reads. The
export-name table identifies the thunks; their jump targets were disassembled
separately. This complements `ORIGINAL-PACKAGE-LOOKUP-AUTHORITY.md`.

## HashObject prepends, and StaticFindObject walks that order

Exported `UObject::HashObject` is thunk101012E9 ->10157730. Its complete body
10157730..10157757 does the following, without checking class, name collisions,
or object-index ordering:

1. Read actual Outer+18; take its object Index+4, or zero for null Outer
   (10157730..1015773C).
2. XOR actual Name+20 and mask `0xFFF` (1015773E..10157741).
3. Read the bucket head from `GObjHash1029E644[bucket]` (10157746).
4. Store this object as the bucket head (1015774D).
5. Store the previous head in this object's HashNext+8 (10157754).

Consequently a bucket is **most recently hashed first**, not ascending object
index and not necessarily most recently allocated first.

`StaticFindObject10151DE0..10151FD0` computes the same bucket at
10151E4C..10151E5F, reads its head at10151E64, compares Name+20 and exact actual
Outer+18 at10151E6F..10151E77, then applies the optional class filter. A rejected
candidate advances through HashNext+8 at10151EB2 and repeats. The first
qualifying candidate in this head-to-tail order returns.

Only after that chain misses does original `InOuter==ANY_PACKAGE(-1)` admit
the separate indexed fallback at10151EB7..10151F88. Its object iterator advances
numeric `GObjObjects` slots in ascending order, skipping null slots and applying
the actual Object-class ancestry admission. This fallback is not hash order.

### Null requested class differs between the two search phases

The exact hash phase tests the requested-class argument at10151E79 and, if it
is null, returns the matching Outer/Name candidate directly at10151E7B. That
branch does not read candidate Class+24 or apply an extra Core.Object filter.

The ANY_PACKAGE fallback has a separate iterator admission even when the
requested-class argument is null:

- 10151EC0..10151EC5 stores actual Core.Object class address1029E0D0 as the
  iterator's class filter;10151ECE stores initial iterator index-1. This class
  address is independently the exported `PrivateStaticClass@UObject` data
  object's VA (export RVA19E0D0).
- 10151EEA passes the iterator pair's address to thunk101028AB, called at
  10151EED; that thunk jumps to10143EE0. Its increment body10143EE0..10143F2E
  starts at index+1, ignores null GObjObjects slots, reads candidate Class+24,
  and follows actual Super+28 until it reaches the iterator's nonnull
  Core.Object filter (10143F04..10143F16). Null Class or a chain ending before
  Core.Object skips that slot (10143F18..10143F29).
- The first admitted iterator index/class are reloaded at10151EF2..10151EF5.
  Only then are Name and requested-class filtering applied at10151F0D..10151F45.
  Null requested class skips only this latter optional filter at10151F15..17;
  it does not undo the preceding iterator admission.
- Subsequent increments are inlined at10151F5B..10151F88. They likewise ignore
  null slots at10151F6B, read candidate Class+24 at10151F6F, and follow actual
  Super+28 until the same Core.Object filter matches at10151F76..10151F7A.

Therefore the fallback enumerates actual Core.Object-or-derived objects, not
arbitrary nonnull slots. Its iterator class is independent of the optional
requested class. In retail, genuine UObjects normally meet this admission;
that expectation must not justify admitting a portable export with missing
Class/Super authority. Such missing portable metadata needs explicit refusal
when it could affect the selected result, not an invented class or an
unqualified admission. This does not add a Core.Object admission to the
separate null-requested-class exact hash branch.

## AddObject publishes an indexed slot before hashing

`UObject::Register10152550` calls exported `AddObject` thunk10102473 at10152654.
That thunk jumps to10157830. For argument-1:

- A nonempty free-index array at1029E618 takes its **last** entry
  (10157866..1015789C), removes that entry, and checks its GObjObjects slot is
  null (101578FC..10157914).
- Otherwise it grows GObjObjects and uses its prior count
  (1015792F..10157964).

An explicit index argument takes the common publication branch directly.
10157971 writes `GObjObjects[index]=this`;10157974 writes this Index+4;10157977
calls HashObject101012E9. These are different identities/orderings: the global
slot determines fallback order, and each actual hash operation determines
bucket order. A freed lower index can be allocated after higher live indices.

## Unhash preserves all other chain ordering

Exported `UnhashObject` thunk101032D3 ->10157760 takes an explicit Outer-index
argument. It XORs that argument with the current Name+20 and masks0xFFF at
1015777B..10157786; it does **not** dereference Outer itself.

10157797..101577B3 is a pointer-to-link traversal starting at the bucket head.
An unequal object advances the link address to candidate HashNext+8. An equal
pointer is spliced out by replacing that link with its HashNext, and traversal
continues. It checks that at least one match was removed (101577B5..101577C8)
and that the count is exactly one (101577D0..101577E4). Surviving candidates
retain their relative order; this body does not clear the removed object's
private HashNext field.

## Rename can change hash precedence without changing object index

Exported `Rename` thunk1010303F ->1014F6A0:

- A supplied nonnull name is converted with FName(Add1) at1014F6D4..1014F6DA.
- A null name calls the unique-name service instead at1014F6E1..1014F6ED.
- It computes the old actual Outer index at1014F6F7..1014F705 and calls
  UnhashObject at1014F708.
- After the ordinary rename log, it writes the new Name+20 at1014F739 and
  calls HashObject at1014F73C.

The complete normal body through1014F751 has no explicit-name duplicate-object
lookup, no same-name early return, no Outer change and no Index change. Subject
to the ordinary calls returning, an explicit rename to an existing sibling's
name therefore permits two different pointers with the same actual
Outer/FName; the renamed pointer becomes the first hash candidate. Renaming an
object to its current name also rehashes it to the head. This is a direct
control-flow consequence, not a claim that a particular campaign script
executes such a rename.

Native `Register` does have its own any-class collision lookup:
1015261A..10152626 calls StaticFindObject(null requested class, actual Outer,
actual name text, Exact0); nonnull enters its error output at10152632..10152648.
That registration check must not be generalized into a universal uniqueness
invariant for every UObject operation.

`StaticAllocateObject101579C0` provides further bounded evidence:

- An optional global-gated branch10157BDB..10157C4F scans the bucket for same
  Outer/Name with a different actual class and emits the independently read
  literal `CONFLICT: %s - %s` at101AFACC.
- The ordinary lookup at10157C59..10157C64 passes the requested class and
  Exact0, rather than a null any-class constraint.
- An existing match goes through replacement; a differing actual concrete
  class enters the error-output path10157D34..10157D73.
- Replacement saves its original object index at10157D8B..10157D90, destroys
  the old object at10157DB9, removes that just-freed index from the free list,
  rebuilds the header, and calls AddObject with the saved index at
  10157EF6..10157EFF. It is thus rehashed at the head even though the numeric
  slot is reused.

This audit does not assign an unverified semantic name to the optional global
gate, or claim that diagnostic output always throws. Rename alone supplies
the concrete reason duplicate Outer/FName order cannot be discarded.

## Destruction removes hash membership before freeing the index

Exported `UObject::~UObject` thunk101028C4 ->10150040. In its normal initialized,
non-critical cleanup branch it checks index validity, calls ConditionalDestroy
at101500B8, then passes the cached field+14 to UnhashObject at101500BD..101500C3.
It subsequently clears `GObjObjects[this.Index]` at101500D6 and appends that
index to the free-index array at101500DD..10150117. The next AddObject(-1)
therefore reuses the most recently freed index first.

The cached value is established by base `UObject::Destroy` thunk101025F4
->1014FA50: after its destroy/linker work,1014FAA0..1014FAAA copies the actual
Outer's Index+4 to field+14, or1014FABE..1014FAC3 writes zero for null Outer.
ConditionalDestroy thunk10102270 ->1014FDB0 sets its conditional flag, invokes
the virtual Destroy at1014FE06 and checks the base-destroy marker afterward.
This is why UnhashObject need not dereference a possibly already destroyed
Outer during later destructor cleanup.

The corresponding portable weak-index cleanup should use the stored bucket
or cached Outer-index/name publication key and identity-check its entry. It
need not emulate native field+14 in reflected storage, and must not obtain
that key by dereferencing `object->outer` during destruction.

These destructor branches have initialization/error-state guards; their
existence is not permission for a portable weak registry to retain dangling
entries during normal shutdown or generation replacement.

## Consequences for the portable service

- Keep actual published object-index slots and hash publication order distinct.
  Sorting numeric indices cannot implement the exact-Outer phase.
- If using serials instead of linked buckets, serials must represent every
  successful hash/rehash event, not birth/allocation time. Undo/rollback must
  restore the actual published lookup state.
- Preserve explicit name and actual Outer identity independently of diagnostic
  path strings, including when Package-phase binding changes Outer.
- Cache the admitted bucket/removal identity so destruction cleanup never has
  to dereference a dead Outer. Identity-check removal before freeing/reusing a
  slot, and isolate retired engine generations.
- Test newer-hash precedence, class-filter skipping within a chain, collisions
  between different names/Outers in one bucket, head/middle/tail unlink, explicit
  same-name rehash, replacement with unchanged index, and ANY_PACKAGE fallback
  after lower-index slot reuse.

This document is original ordering evidence only. It does not implement
StaticFindObject, Rename, original allocation/default services or gameplay.
