# Original ScriptedPawn agitation/fear decay helpers

This is a bounded native-helper implementation and provider contract, not a
replacement NPC simulation, completed `ScriptedPawn.Tick`, Pawn physics, NPC
animation acceptance, or a first-Training playthrough result. Runtime admission
must still execute the selected original Tick continuation and all reached
services in the same caller root. Nothing here permits returning successful
no-ops for other native NPC work.

## Owned source evidence

Read-only inspection on 2026-10-26 used the owned installation at
`D:/Steam/steamapps/common/Deus Ex`. No retail EXE/DLL was executed, loaded as a
module, patched or linked. The port's existing bytecode inspector reads package
data; GNU objdump statically reads the PE32 bytes.

`System/DeusEx.dll` SHA256:
`B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04`.

The native bodies are `UpdateAgitation` at `10019E30..10019F55` and
`UpdateFear` at `10019F60..1001A005`. Independent static `.rdata` inspection
at `10028350` gives bytes `00000000 0000F03F` (Double1), followed at
`10028358` by `00000000` (Float positive0). These are actual instruction
operand formats, not permission to turn every operand into a host Double.

The scalar/packed property mappings are independently derived in
`ORIGINAL-SCRIPTEDPAWN-TICK-PREREQUISITES.md`: `AgitationDecayRate` native+720,
`AgitationTimer`+724, `AgitationCheckTimer`+728, `FearDecayRate`+734,
`FearTimer`+738, `FearLevel`+73C and `bAlliancesChanged`+650 bit8. Native
offsets refer to the original PE32 object layout, never a portable C++ object.

The owned `DeusEx.u` also contains similarly named compiled script bodies:
`UpdateAgitation` has native0/flags2/logical393 bytes and `UpdateFear`
native0/flags2/logical198 bytes. Their Float locals `Mult` and `decrement`
introduce spills absent from these C++ helpers. The native custom Tick calls
its local C++ helpers; interpreting the identically named script functions is
not an exact substitute for this path.

### Alliance entry identity correction

The DWORD tested at entry agitation-address minus8 is **an FName index, not
an Actor/UObject pointer**. The earlier raw-disassembly prerequisite paragraph
called it a pointer before this package/schema corroboration was available.
The actual `UpdateAgitation` script's array/struct accesses establish:

| Actual declaration | Package reference | Meaning |
| --- | --- | --- |
| `ScriptedPawn.AlliancesEx` |276 | Fixed array of16 `AllianceInfoEx` entries |
| `AllianceInfoEx.AllianceName` |1249 | `NameProperty`, tested `!= None` |
| `AllianceInfoEx.AgitationLevel` |1783 | `FloatProperty` |
| `AllianceInfoEx.bPermanent` |4028 | `BoolProperty`, negated |

The native loop's last agitation address is actor+8E4, its stride10 hex,
its name displacement-8 and permanent-bit displacement+4. Together these
give array base+7EC, last entry base+8DC, agitation at entry+8 and
`bPermanent` at entry+C bit1. Runtime authority must validate the actual
declared struct, complete16-element aggregate and member types. A displayed
string, arbitrary UObject pointer or truncated/dynamic array is not equivalent
to the original FName/aggregate contract.

## Exact reached operations

All comparisons below describe the original status-bit predicate, not C++
unordered comparisons. The positive tests use `AH & 41 == 0` (strict ordered
positive); the less-than/negative tests use C0 alone, which is also set for
unordered. This matters for NaNs and the final zero clamps.

`UpdateAgitation`:

1. Read check timer for positivity; if positive, read it again and subtract
   Float delta. Float-store the result, then compare the **retained** PC64
   value and raw-store positive0 when C0 is set.
2. Initialize retained decay0. Read agitation timer for positivity. If positive,
   read it again for timer/delta comparison. Read it a third time in either
   selected arithmetic path. A short timer divides by Float delta, stores
   timer0, performs `Double1 - ratio`, loads Float delta, multiplies by the
   **fresh memory** rate and multiplies the two retained registers. A sustained
   timer instead Float-stores timer-minus-delta, leaving decay0. A nonpositive
   or unordered timer computes retained delta-times-fresh-memory-rate.
3. Read `bAlliancesChanged` **before** testing decay positivity. Only true plus
   ordered-positive decay clears the flag and enters the fixed16-entry loop.
4. Visit indices15 through0. Read each name; only a non-None name reads its
   permanent flag, and only nonpermanent entries read agitation for positivity.
   For each eligible entry, freshly set `bAlliancesChanged`, reread agitation,
   subtract retained decay, Float-store, then C0-clamp the retained result to0.

`UpdateFear`:

1. Read fear timer for positivity. A positive timer is read again for the
   timer/delta comparison and **a third time before the branch** (original
   `FLD` at10019F7F before `FNSTSW`). A sustained timer Float-stores the
   third-loaded timer-minus-delta and immediately returns without querying
   rate or fear level.
2. A short timer divides the third loaded timer by Float delta, stores timer0,
   then computes `Double1 - ratio`, delta-times-fresh-memory-rate and the
   retained product. A nonpositive/unordered timer computes delta-times-rate.
3. Only ordered-positive decay reads fear level for positivity. A positive
   level is read again as the **raw Float memory operand** of `FSUBR`, with
   retained decay. Float-store the result before its retained C0 clamp to0.

Arithmetic uses `quest_native_scalar.h`'s explicit masked x87 PC64,
nearest-even profile. Original `FLD` uses `Load`, actual memory arithmetic
operands use `Memory`, and only original Float stores use `StoreFloat`.
There is no host-Double/Float algebraic simplification or blanket `max(0,x)`.
The retained witness delta1, delay2^-25, rate1, level1 leaves level2^-25;
eagerly spilling native decay to Float instead incorrectly leaves0.

## Header and caller contract

`native/quest_scripted_pawn_decay.h` exports:

- `UpdateOriginalScriptedPawnAgitation(actor,delta,queries)`;
- `UpdateOriginalScriptedPawnFear(actor,delta,queries)`;
- `OriginalScriptedPawnDecayField`, `OriginalScriptedPawnDecayQueries` and
  `OriginalScriptedPawnDecayStats`.

The queries expose the six typed scalar identities, logical
`bAlliancesChanged`, the validated fixed-array name/permanent/agitation
members and mandatory shared `work`. A raw Float query must preserve all32
bits, including signaling NaN payloads; it must not perform an eager FLD-like
conversion. The logical Bool store updates its declared packed bit while
preserving unrelated properties, not an assumed complete host DWORD.

A nonnull receiver is required. Missing providers are refused only when
reached; missing work authority is refused before any provider. Each reached
read/store first charges the caller's work service. Native delta is not
independently clamped, made finite or converted to an unrelated timestep here.
All reached provider/store failures propagate. Earlier writes and nested
provider effects remain the caller root's responsibility: no local
transaction, journal commit, rollback or swallowed exception exists in these
helpers. Returned counters describe a successful path, not an admission gate
or permission to suppress unimplemented work.

## Authored tests and scope

`desktop/quest_scripted_pawn_decay_test.cpp` covers exact transcripts/counters,
all16 descending entries, lazy absent fields, permanent/None entries, timer and
agitation rereads, timer0-before-rate ordering, retained-decay precision,
unordered store-then-clamp, null receiver/missing providers/missing work,
every reached work/provider failure prefix, and explicit mock-caller rollback.
The mock rollback test is not proof of the production runtime journal.

The independent oracle uses its own GNU x87 instructions and actual80-bit
register-value storage, masked PC64/nearest-even control, literal FCOM status
bits, original m32/m64/reverse operands and Float-store trace. It does not
invoke production scalar arithmetic to calculate expected results. The
deterministic24-word Cartesian corpus includes signed zero, subnormal/normal
boundaries, extreme finite values, infinities and signed quiet/signaling NaNs;
4096 seeded raw-bit states add coverage. Both helpers run for every state,
giving35840 full-helper oracle cases. Unsupported oracle hosts fail the test
instead of silently skipping numeric acceptance.

At this handoff the new sources are authored but intentionally **not built or
executed** pending the root agent's shared-input freeze. Runtime binding,
whole-root evidence, source/installed-APK identity and headset acceptance are
separate requirements, not results asserted by this document.
