# Original Window preferred-width/height queries

Owned GOTY static audit,2026-10-24; next genuine first-Training startup boundary.
No commercial DLL/EXE was loaded/executed; our existing script inspector reads
owned package metadata/bytecode and objdump reads static DLL bytes. This adds
documentation only, not native1463/1464 implementation or completed startup.

Owned SHA256:

- `System/Extension.dll`:
  `93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C`.
- `System/Extension.u`:
  `9984FAEDA55FB2100B1A15A37BFEB8CDB4878248B39AFF832002A799F1D787F7`.

## Exact declarations and wrappers

| Native | Genuine declaring function | One required input | Return | Wrapper | Direct body |
| --- | --- | --- | --- | --- | --- |
| 1463 | Extension.Window.QueryPreferredWidth | queryHeight Float/flags0x80 | ReturnValue Float/flags0x580 |10053080..100530C9 |1004E5F0..1004E640 |
| 1464 | Extension.Window.QueryPreferredHeight | queryWidth Float/flags0x80 | ReturnValue Float/flags0x580 |100530F0..10053139 |1004E680..1004E6D0 |

Each function has native flags0x401, logical script length5 and scalar
arrayDimension1 for both input and result. Neither input is optional or OUT.
The wrapper zero-initializes a Float0 temporary, Steps the one supplied operand
using FFrame.Object, consumes EndFunctionParms, copies that Float by value to
the direct nonvirtual body, and always stores the returned x87 value to the
Float result destination with `fstp DWORD PTR [result]` (100530C6/10053136).
There is no normal warning, no-result or special NoWrite branch. An absent or
partial expression result affects the initialized Float temporary, not a stale
attached property alias. Finite signed values are not clamped: negative0 and
negative supplied constraints must retain their actual specified flag.

The width body calls the existing common native1004E790 with:

```text
widthSpecified=false, width=0, widthOutput=&localResult,
heightSpecified=true, height=queryHeight, heightOutput=nullptr
```

The height body calls it symmetrically:

```text
widthSpecified=true, width=queryWidth, widthOutput=nullptr,
heightSpecified=false, height=0, heightOutput=&localResult
```

They load that local Float result and return it. The remaining body tails are
ordinary engine exception-guard diagnostic/rethrow code, not fallback success.
The input copy is also reused as the local result storage by those bodies;
that implementation detail does not grant script OUT semantics.

## Reuse the existing real query service

The common1004E790..1004E9D1 routine is already represented by
`QueryNativeWindowPreferredSize` in `native/quest_native_window_query.h`.
Its width/height-specified and hardcoded-size promotion, cache predicates,
cache writes before callback, real dynamic ParentRequestedPreferredSize,
post-callback captured constraints and fallback are shared by1463/1464/1465.
The common routine computes/cache-updates/falls back **both dimensions even
when one output pointer is null**: output-pointer tests occur only at
1004E995..1004E9A8 after the fallback. Do not incorrectly skip the other
dimension's fields or fallback merely because only one scalar is returned.
When both dimensions are specified/promoted the existing early return bypasses
query/cache/fallback, including explicitly negative supplied values.

Consequently these are **not read-only queries**. They may journal cache state,
invoke selected script callbacks, recurse into child queries and receive real
OUT mutations. Existing providers that can be reused in the next increment:

- `RequireNativeWindow` at runtime.cpp5359 authenticates a genuinely constructed
  native Window; a same-shaped presentation object is not authority.
- `WindowNativeDeclaration` at3834 can gain the exact required Float and Float
  return schemas above; current1465 handling already validates declaring
  identities, native flags/index, scalar metadata and source.
- `NativeArgumentSeed` at5231 can register these wrappers' one Float0 input;
  they belong with ordinary copied native inputs, unlike1465's two separate
  copied OUT temporaries. Preserve direct/declaration/opcode entry, actual
  expression-time snapshot, NoWrite/partial-null and current operand receiver
  contracts. No uninitialized-result capability is required here.
- `NativeWindowQueryOperations` at5409 already supplies reflected cache reads
  and journaled writes; `FallbackSize` reads actual background texture USize/
  VSize or current Width/Height, refusing missing metadata.
- `NativeWindowPreferredSizeCallback` at5363 already invokes the actual selected
  ParentRequestedPreferredSize through the same Execution/root with
  Bool/Float-OUT/Bool/Float-OUT, owning OUT storage through rollback. It currently
  explicitly refuses non-Window native preferred-size virtual overrides;
  adding the scalar wrapper must not remove that refusal.
- `WindowPreferredSizeNative` at5448 demonstrates the common query binding.
  Scalar1463 should return
  `QueryNativeWindowPreferredSize(ops,false,0,true,input).width`;1464 should
  return `QueryNativeWindowPreferredSize(ops,true,input,false,0).height`.
  They return written Float values, not1465's Nothing or output-alias writes.

Line numbers are this checkpoint's snapshot and may shift during implementation.
No extra layout engine, viewport geometry assumption or synthetic size is
required to implement these exact wrappers. Any reached unsupported native
preferred-size override or callback must still fail the entire root honestly.

## Actual first-Training caller and dependency

Owned `DeusEx.DeusExHUD.ConfigurationChanged` (1969 logical bytes) reaches
native1463 at logical PC606 on `activeItems`. Its sole argument is the current
HUD Height minus the already queried beltHeight. The returned width is used
atPC630's original ConfigureChild, not merely displayed as a number.

The actual selected `DeusEx.HUDActiveItemsDisplay.ParentRequestedPreferredSize`
has82 logical bytes, native0, flags0x802 and the original four-parameter packet.
It queries `winAugsContainer` and `winItemsContainer` using1465, then adds their
widths and heights into its preferred OUT values. Its locals are not extra
parameters. The base Extension.Window callback has the same packet and an
empty body; that does not permit the selected override to be skipped. Child
containers can reach further native layout dependencies; those have not been
implemented or accepted by this audit. No startup prefix is published here.

Minimum next controls: width/height mirror argument packets and always-written
Float result; expression-time copied Float0 seeds for raw/declaration/direct
entries; cache hit/miss/callback mutations, signed0/negative constraints and
both-specified early return; both fallback dimensions despite unused output;
same-root callback failure/write cutoff rollback; exact current original
startup boundary transition. Existing pure-query tests cover the common helper,
not the missing1463/1464 wrapper integration.

Reproduce with no original executable code:

```powershell
& artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe 'D:\Steam\steamapps\common\Deus Ex' 'Extension.Window.QueryPreferredWidth' 'Extension.Window.QueryPreferredHeight' 'DeusEx.HUDActiveItemsDisplay.ParentRequestedPreferredSize'
& objdump -d -Mintel --start-address=0x10053080 --stop-address=0x1005313A 'D:\Steam\steamapps\common\Deus Ex\System\Extension.dll'
& objdump -d -Mintel --start-address=0x1004E5F0 --stop-address=0x1004E9D2 'D:\Steam\steamapps\common\Deus Ex\System\Extension.dll'
```
