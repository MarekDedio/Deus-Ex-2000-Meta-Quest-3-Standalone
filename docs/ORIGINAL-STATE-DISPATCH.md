# Original GOTY state selection and callback ancestry

This increment corrects shared dispatch metadata and lookup. It does not finish
the actor state-frame model, start maps, possess a player or enable NPC ticking.
The accepted hand-direction APK remains installed while this backend work is
tested. No original packages or DLLs are included in the repository or APK.

## Contract and implementation

The user's original Core.dll, SHA256
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`,
was inspected without running it. Original GotoState at image VA `1012e8f0`
uses the actual class Children/Next field iterator for Auto selection, starting
at `1012e977`, then traverses superclasses. It returns the first state carrying
flag `2` directly. Global FName comparison-index order is not traversal order;
the selected name is not re-resolved against derived overrides. No candidate
means class-backed/null named selection, not a literal non-Auto state named Auto.

The graph now retains each class's actual state child order and each state's
serialized SuperField path. Named selection still searches receiver classes
derived-to-base. Function lookup at an exact selected node follows its explicit
SuperField links, including differently named parent states, then falls back to
receiver-class functions. Global calls omit selected-state lookup. A diagnostic
current-state-name query treats a literal Auto name as a name, not an Auto
selection request.

Original FindObjectField at `10150b50` looks at the selected StateNode, not the
running Node, then the receiver class. This functions-only decoded graph does
not reconstruct every native hash-link precedence or non-function name-hiding
rule. Runtime virtual/event calls still use the legacy current-state-name query:
their context conflates selected state and running code after inherited label
jumps. Wiring that running path to the new exact-selected-node API would be a
regression when derived callbacks must remain selected. Exact-path dispatch is
ready at the shared graph layer, not yet at live portable overrides. Independent
identities, nullable Code, original live-mask transitions and callback/preemption
order remain required before world startup.

The detached graph rejects missing, cyclic, over-depth or foreign-owner state
parents; incomplete, duplicate or foreign child-order entries; and Class/State
path collisions. It owns path values and retains no native pointers. Class-backed
and receiver-ancestor-Class paths are allowed for detached metadata queries;
this allowance is not proof that every such path is a valid original live frame.

## Verification

Asset-free dispatch controls cover independent child/name/index ordering,
inherited Auto selection despite a derived same-name override, absent Auto,
literal Auto callbacks, different-name state inheritance, no manufactured
same-name inheritance, receiver/global fallback, Class-backed metadata and
malformed graph bounds.

The original-data audit independently decodes source Children/Next and SuperField
links for every indexed Class/State, compares them to detached runtime metadata,
and checks Auto and selected-node callback queries. It also structurally inspects
all original Function/State/Class programs and terminal label tables without
executing them or creating live script state. It is not an all-script feasibility
or campaign acceptance test. Package/path/name decoders are shared with the
runtime; this is source-link consistency, not an independent raw-package decoder
proof. Callback coverage uses each state's own functions plus Tick, BeginState,
EndState and an absent name, with the state-owning class as receiver. Synthetic
controls separately cover inherited-only callbacks and derived receivers with
an inherited selected state. Oracle ancestry has its own cycle/depth bounds.

Fresh evidence (2026-10-11): dispatch controls pass 63,597 checks with 36 expected
rejections. The source audit passes 1,502 classes, 261 states, 16,527 child fields,
100 explicit state parents, 667 Auto selections, 835 absent-Auto controls and
3,098 selected-node callback queries. Structural inspection covers all 8,208
functions (70,611 statements), all Class/State code (2,820 statements), 355 labels
and 159 terminal tables. These counts do not imply execution of every program.

All 50 ordinary host tests pass; three optional original-data integrations are
skipped by CTest and invoked separately with the owned installation. The focused
read-only original dispatch audit, original defaults, full actor and runtime
save/restore integrations pass. The Android ARM64 build and APK v2 signature
verification pass.
The new APK is archived, not installed, at
`artifacts/original-dispatch-20261011/DeusExQuestVR-original-dispatch-final-20261011.apk`
(18,919,145 bytes; SHA256
`EE008039DB2424FA1D3D841C6D9DA89D75D4A3AFD448188E54871C7FDCA034A1`).
Its 16 ZIP entries contain no original Deus Ex packages. The installed accepted
hand-direction APK was rechecked over USB and still matches SHA256
`D66572A71FE41751452908ED428B959CB215E65B4727F7CAD458770701899A82`.

```powershell
.\artifacts\prerequisites-20261009\build\quest_script_dispatch_test.exe
.\artifacts\prerequisites-20261009\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex' --dispatch-only
```

Ignored evidence: `artifacts/landing-state-20261010/` contains the bounded
original state-identity audit and DLL disassembly. Fresh host/build logs are
under `artifacts/original-dispatch-20261011/`. The source installation remains
read-only. See [world startup gaps](WORLD-STARTUP-GAPS.md) for the remaining
shared state-control and campaign integration work.
