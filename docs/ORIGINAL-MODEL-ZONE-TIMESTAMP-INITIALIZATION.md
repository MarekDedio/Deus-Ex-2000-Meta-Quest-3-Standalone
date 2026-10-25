# Original Model zone timestamp initialization

Read-only owned-GOTY static audit, 2026-10-25. A **fresh exact native
Engine.Model** obtains Float `+0` in all64 native zone LastRenderTime slots
from actual class-default preparation and instance allocation. This is not
inferred from a missing serialized field, a zeroed portable container, or
EmptyModel's omission of a timestamp store.

This closes the initial-value question left open in
[the stasis prerequisite audit](ORIGINAL-ACTOR-STASIS-PREREQUISITES.md).
It does not implement timestamp storage, renderer publication, InStasis,
Actor/Level Tick or automatic first-Training crate/loot gravity.

## Scope and source identity

No commercial DLL/EXE was loaded or executed. External MinGW objdump read
static PE tables, import cells, constants and bounded disassembly. Rechecked
owned sources under `D:/Steam/steamapps/common/Deus Ex` have these hashes:

| Source | SHA256 |
| --- | --- |
| System/Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| System/Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| System/Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

The hexadecimal addresses/offsets below describe original PE32 evidence, not
host pointers or a portable C++ object layout. The conclusion is restricted
to the actual native Model allocation/default chain, not arbitrary subclasses
or any object whose leaf name happens to be Model.

## 1. Actual registered Model and its default payload

The complete Model native registration is `Engine103AD260..103AD2C6`:

- Actual UClass is `10589D58`; instance size `0x704` is pushed at
  `103AD2A0`; actual superclass UPrimitive `1058D368` at `103AD297`.
- Class-name UTF16 Model is at `10480C86`. The registration supplies actual
  Core.Object Within through import `105999F8`, zero Guid and supplied class
  flags0. Package/config are the actual Engine/System literals, not a
  replacement script class.
- `103AD262/103AD26A` load/push actual UObject.StaticConstructor through
  import `105999FC`. That IAT cell contains Hint/Name RVA `0029A432`, naming
  `?StaticConstructor@UObject@@QAEXXZ`.
- Core export-table index1290 identifies thunk `10101041`, which jumps to
  `1014F690`. That complete body is a single `RET`.

[The native primitive dispatch audit](ORIGINAL-NATIVE-PRIMITIVE-DISPATCH.md)
independently records this registration and that Model has no serialized
Class export in the owned Engine.u. Model therefore has no authored
Engine.u class-default tags supplying an alternate native zone timestamp.
[The Engine default audit](ORIGINAL-ENGINE-DEFAULT-LINK.md) establishes
UPrimitive's actual native size `0x54` and default preparation.

Core UClass.Register `10127851..101278A2` allocates/resizes its actual
class-size default block and calls UObject.InitClassDefaultObject through
thunk `10101226`. The latter complete body `10152300..1015236F` initializes
the UObject header, obtains the actual destination native size and reads
the actual superclass at `10152346`. It passes that superclass, not the
destination class itself, to InitProperties at `10152355`.

InitProperties `101520D0` then:

1. Selects the supplied class's actual default pointer/size at
   `10152123..10152144` when no explicit template is supplied.
2. Copies the inherited payload beyond the `0x28` UObject header at
   `10152149..10152176`.
3. Zero-extends the remainder of the larger destination at
   `10152185..101521A7` using zero EAX and REP STOSD/STOSB.

For Model default preparation, the inherited extent is Primitive `0x54`
and the destination extent is Model `0x704`. Consequently the complete
Model extension `+0x54..+0x703` starts with zero bits. Register's subsequent
same-superclass callback check at `101278A7..101278BE` skips a shared static
constructor; even an invocation of the supplied UObject callback would do
nothing. There is no Model-specific static-constructor timestamp override.

The independently audited native zone layout has64 entries of24 bytes,
starting at Model `+0x104`, with each LastRenderTime Float at
`+0x108 + 0x18*z`. For `z=0..63`, the timestamp addresses range from
`+0x108` through `+0x6F0` inclusive (last byte `+0x6F3`). Every timestamp is
inside the proven zero-extended Model default tail. Zero bits mean Float
positive zero, not `-0`, `-10`, native Level time or Actor.LastRenderTime.

## 2. Fresh map export copies those prepared defaults

The original missing-export-object CreateExport path is
`Core1012E230`, with allocation arguments at `1012E34C..1012E36A`.
At `1012E364` it explicitly pushes literal0 as the **template** argument
to actual StaticConstructObject (`10101771 -> 10157FF0`). This is not
permission to assume every caller supplies a null template.

StaticConstructObject `10158036..1015804D` forwards that template into
StaticAllocateObject (`10102EC3 -> 101579C0`). On the fresh allocation path,
the latter prepares the actual header, selects actual class/native size and
calls InitProperties at `10157EF1`. With the real Model class and null
explicit template, InitProperties copies the prepared Model default payload,
including all64 zero timestamps, into the fresh instance.

Only afterwards does StaticConstructObject call that actual class's
InternalConstructor at `1015805C`. For Model the chain is
`103036ED -> 1031E3C0 -> 1030122B -> 1031E3E0`. The complete default
constructor `1031E3E0..1031E548` constructs its members, installs the Model
vtable and calls EmptyModel(1,0), but writes no zone timestamp.

The absence of a constructor store is significant **only after** the
allocation/default-copy baseline above is established. The stasis audit
already bounds EmptyModel `103AD3E0..103AD690`, Model zone serialization
`103AED30..103AED92` and PostLoad `103ACE90..103ACF2E`: none initializes or
archives LastRenderTime. Primitive.Serialize, called first by
Model.Serialize at `103ACACC`, resolves `10303C0B -> 103D82D0`; its ordinary
body archives Primitive bounds/sphere, not the Model zone table.

Thus fresh native Model loading preserves the copied zero timestamps until
a subsequent real publisher changes them. The native Render.dll publications
identified in the stasis audit use actual LevelInfo.TimeSeconds Float;
initial zero is not a substitute for implementing those later publications.

## Bounded reproduction

These commands only parse the owned files; none loads their executable code.
The export tables name the actual functions and native UClass identity.

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll','D:\Steam\steamapps\common\Deus Ex\System\Engine.dll','D:\Steam\steamapps\common\Deus Ex\System\Engine.u'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -p 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll' | Select-String -Pattern 'InitClassDefaultObject|InitProperties|StaticAllocateObject|StaticConstructObject|StaticConstructor@UObject'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -p 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll' | Select-String -Pattern 'PrivateStaticClass@UModel|InternalConstructor@UModel|\?\?0UModel'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103AD260 --stop-address=0x103AD2C7 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -s --start-address=0x105999F8 --stop-address=0x10599A00 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -s --start-address=0x1059A432 --stop-address=0x1059A458 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10101041 --stop-address=0x10101046 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x1014F690 --stop-address=0x1014F691 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10127851 --stop-address=0x101278F3 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10152300 --stop-address=0x10152370 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x101520D0 --stop-address=0x10152245 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x1012E34C --stop-address=0x1012E37C 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10157EB0 --stop-address=0x10157EF6 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10158036 --stop-address=0x10158065 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x1031E3E0 --stop-address=0x1031E549 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103D82D0 --stop-address=0x103D83D4 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
```

## Integration limits and remaining work

- Establish the actual exact Model/current loader generation before using this
  baseline. Unknown native subclasses, explicit nonnull templates or arbitrary
  reused live objects are not covered by the fresh allocation proof.
- Do not reset timestamps to zero when EmptyModel is called, on each frame,
  on renderer refresh, or merely because a save/archive omitted the values.
  A genuinely newly constructed object and a retained live object have
  different lifetime authority; this audit does not supply a general reload
  or portable checkpoint reset policy.
- All64 native slots have this allocation baseline independently of serialized
  NumZones. Out-of-range stored ZoneNumber still requires an explicit portable
  refusal, not a wrap or fabricated fallback timestamp.
- Future runtime storage must preserve genuine Model identity, shared journal/
  rollback/checkpoint lifetime, and later renderer writes. It must not alias
  Actor.LastRenderTime, synthesize values from visible mesh submissions or use
  the HMD clock in place of native timing.

Only this audit document was authored. No compiler input, package or original
binary was modified; no build, runtime test, APK, automatic gravity, complete
startup or headset acceptance is claimed.
