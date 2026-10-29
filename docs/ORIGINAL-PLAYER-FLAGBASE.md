# Original constructor-owned player FlagBase: remaining service (2026-10-29)

Source evidence for the next first-Training startup dependency, not an
implemented constructor/storage service. Only static package/PE inspection;
never load or execute commercial DLLs/EXEs. An absent serialized FlagBase tag is
NOT authority for native None, empty flags or successful skill restoration.

## Construction and storage

APlayerPawnExt constructor Extension10037BA0..10037D0F calls its superclass,
member constructors and Extension name bootstrap. At10037CC0..10037CFD, it reads
the copied pointer at pawn+890. When null, operator1001B500 allocates an actual
FlagBase (size12C, StaticClass1009DBE8), Outer=actual pawn, NAME_None0, requested
flags0. FlagBase constructor100239A0 is invoked; pointer is stored at10037CF7.
Do not create this object lazily during CheckFlag, or use the vendored transient-
package/RFTransient workaround instead of the actual pawn/Outer contract.

Class registration10023900 supplies size12C, supplied class flags0, object flags
04084004, native constructor1001B5A0; standard UClass construction ORs12.
CLASS_Transient8 is not established by the requested instance flags0.

FlagBase has64 bucket-head pointers at+28 (hashTable transient10000,dim64) and
defaultFlagExpiration Int at+128 (transient10000). Constructor100239A0 calls
UObject construction, installs its vtable and explicitly stores expiration0;
it does NOT explicitly clear hashTable. Authentic allocator/CDO copying, now
established below, supplies those initial buckets. This is64 BUCKETS, not64
total flag slots.

## Authenticated initial-empty default and instance authority

ExtensionObject registration100378B0..100378FA supplies native size28, Super/
Within=Core.Object through10059004, native constructor10002770 and inherited
UObject.StaticConstructor through10059008 (Core10101041 ->1014F690, RET).
FlagBase derives from this actual ExtensionObject, not directly from Core.Object.

Core InitClassDefaultObject10152300 clears the header28, selects actual Super+28,
queries native destination size through class virtual+64, then calls
InitProperties101520D0 with explicit-templateNone. That copies superclass CDO
payload after header28 and zero-extends the remaining native tail at10152185..A7.
The superclass size is28, so ALL FlagBase payload bytes+28..+12B are genuinely
zero-initialized by this default allocation service before the native constructor.

Original Extension.u FlagBase export71 (offset37624,length88) has base18
ExtensionObject, flags112, logical0, Within=-8 Core.Object, ConfigName index4
System, two dependencies and two package imports. Its first/final default Name0
None is at byte87, ending at88: no serialized default tags overwrite the tail.
Config4, Localized20 and Transient8 class flags are not set; config/localization
loading therefore cannot supply hidden nonzero buckets for this original class.

Instance StaticAllocateObject10157EDD..10157EF1 calls InitProperties with actual
FlagBase class, explicit-templateNone and size12C, copying this authenticated
zero CDO tail. The constructor at100239C9 then stores expiration0 again. This
establishes empty64-bucket authority through actual allocation/default copying,
not by assuming an absent saved tag means an initialized native service.

Ordinary UObject constructor Core101021C6 ->1014F340..348 only installs its
vtable and returns; it does not add04000000 or Transient. With requested flags0
and class flags112, the instance remains flags0 (the allocator's Transient8 gate
at10157E9E is false). Registration-object flags are a separate context.
NAME_None invokes actual MakeUniqueObjectName10157BB9..10157BD3: preserve class
counter/Outer collision semantics, not a fixed synthetic FlagBase0 identity.

A Flag contains Name+28, signed hash+2C, owner FlagBase+30, nextFlag+34,
type Byte+38 and expiration Int+3C. Int payload+40 belongs to the actual
Extension.FlagInt subclass, not generic Flag; GetInt hits must authenticate
that native subclass/schema/constructor. Distinct XFlagInt constructor,
GetValue, vtable and class exports establish the separation. The collector edges
include PlayerPawnExt.FlagBase (ObjectProperty flags10002,dim1,FlagBase type),
bucket heads, each Flag.nextFlag and Flag.FlagBase backpointer, plus Class/Outer.
These native transient links do not establish save-archive reconstruction.

## Actual operations reached by RestoreSkillPoints

Native1120 is Extension.FlagBase.CheckFlag (401/logical10): Name FlagName80,
Byte flagType80 constrained by ExtensionObject.EFlagType, Bool ReturnValue580.
Wrapper10025AA0 initializes Name0/Byte0, Steps/copies both, then100247D0 calls
GetFlag10023BE0/FindName10024E60. Type2 selects Int. CheckFlag tests NODE
EXISTENCE, not integer truth/value, and does not call CheckName/GError validation.

Native1112 GetInt (401/logical5) takes Name80 and returns Int580. Wrapper10025790
selects type2 and returns0 on a genuine lookup miss, otherwise actual Int payload.
Native1121 DeleteFlag (401/logical10) takes Name80/Byte80 and returns Bool580.
It invokes the matched node's scalar-deleting destructor(flags1), not a slot-clear
shortcut. Flag.Destroy10023870 calls UObject.Destroy then owner.RemoveFlag10023A80,
which unlinks the node and clears its owner/next. FlagBase.Destroy100239F0 drains
all buckets using that actual ownership protocol.

FindName10024E60 computes a casefolded UTF16 CRC using actual GCRCTable through
IAT1005911C: initial hash0, ASCII a-z to uppercase, process low byte then high
byte with h=table[(h^byte)&255]^(h>>8); no initial/final complement. bucket=h&63.
Traversal matches signed hash, actual FName identity and unsigned type. Sorted
hash/type early exits preserve an OUT insertion-link pointing to the actual
bucket head or previous nextFlag, including a miss. Ordinary host CRC helpers
and a flat vector of64 entries are not substitutes.

RestoreSkillPoints first obtains SKTemp_SkillPointsAvail via real native1024,
then reads constructor-owned FlagBase and CheckFlag(Name,2); on a hit, GetInt and
DeleteFlag restore/remove it. It repeats for SKTemp_SkillPointsTotal. Missing
constructor authority must explicitly refuse, not allow null Context to skip
those calls and count a later script location as source-faithful progress.

## Next runtime integration seam (not implemented by StringToName batch)

SpawnNative currently sets the actor's actual header/default-copy and publishes
its provisional lookup entry before initial Actor stores, ownership callbacks,
InitializeActorExecution and Spawned/PreBeginPlay. There is no native
PlayerPawnExt constructor branch there. The constructor-owned FlagBase hook
belongs after that actual header/default-copy/lookup admission and BEFORE any
owner/Spawned callback or actor execution, not in InitRootWindow or a field read.

Use a dedicated non-Actor allocation protocol (compare ConListItem/DumpLocation),
not Actor Spawn/runtimeBirth/world slots/window ownership. Actual pawn Outer,
class counter/unique name, flags0 and zero-CDO authority must reach real lookup
and object-index registration. Share the existing root index transaction used
by constructed objects and Actor births; never open a competing pool journal.

Typed storage needs FlagBase's64 real bucket pointers/expiration and each Flag's
genuine NameString/hash/owner/next/type/expiration, with subtype-owned value
payloads such as FlagInt. The pawn owns the native
FlagBase edge. Mark all forward/back links through RuntimeObject::Mark, not a
global root shortcut. Beforeimages must retain the pawn edge and deep native
payloads; DeleteFlag must use actual node destruction/unlink/index retirement.

Constructed-object string/cast/class-query routes must recognize these markers
before authored-export fallback (exportIndex=max is not a package export).
Native reads must precede ordinary script overlays; generic writes cannot gain
authority over readonly/pointer links. Retained budgets must cover weak inventory,
native payloads, provisional roots, aliases, counters and beforeimages.

Archive/travel/checkpoint/load/unload routes need actual native-state ownership
policy. Transient reflected links are not permission to silently drop FlagBase
progress. Until a real archive/rebind service exists, explicitly report the
unsupported preservation operation instead of claiming working saves/travel for
a new native flag graph. Full campaign preservation remains required by the goal.
