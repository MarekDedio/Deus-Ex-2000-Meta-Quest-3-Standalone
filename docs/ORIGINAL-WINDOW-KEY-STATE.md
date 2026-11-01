# Original Window.IsKeyDown native1470 (2026-11-01)

This service reads the actual terminal RootWindow's current keyDownMap. It is
not Quest event mapping, Root.Process, keyboard/mouse dispatch, Input.Init or
playable original startup. Those integrations remain required.

## Read-only original authority

Owned Extension.dll SHA256:
`93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C`.
External objdump reads its bytes; no commercial DLL is loaded or executed.

- execIsKeyDown100535F0 initializes one Byte local to0 at100535F8, Steps the
  argument once, masks its low Byte at10053621 and calls1004C980.
- IsKeyDown1004C980 first calls actual GetRoot1004C5A0, then checks0<=key<255.
  A genuine absent terminal Root or out-of-range key returns0. Otherwise it
  returns the current byte at Root+630+key. Script Bool observes nonzero.
- The actual UFunction is native1470/flags401/logical5, owned by
  Extension.Window. Its Key is Byte/flags80/dimension1 with actual enum
  Extension.ExtensionObject.EInputKey; ReturnValue is Bool/flags580/dimension1.
- RootWindow.keyDownMap is a255-element ByteProperty/flags0 with no enum.

## Runtime binding

WindowNativeDeclaration authenticates the owned function identity, exact five-
byte NativeParm program, parameter metadata and actual EInputKey reference.
Both indexed-expression and declared/direct VM calls use copied Byte operand
capture. The host supplies the source-proven initialized Byte0 seed; explicit
Nothing/NoWrite preserves it and a full one-byte ZeroFill clears it. Missing
required arguments, Int coercion, malformed Byte payloads and foreign native
declarations refuse rather than returning false.

WindowKeyDownNative reuses NativeWindowRoot's live parent traversal, cycle/work
checks, terminal windowType and safe actual Root ancestry. It does so even for
key255, preserving original order. It then authenticates and reads the actual
qualified Root keyDownMap slot, not Input.Keys, a cached Root or a mirror map.
There are no writes or callbacks in the getter.

## Owned integration verification

portable_window_key_state_test explicitly prepares genuine FlagBase Class/CDO
authority, then creates an actual dormant JCDenton from the real first Training
map INSIDE the same rollback-only root as actual configured InitRootWindow.
It selects the real first PlayerStart from ascending live Actor slots and uses
its current qualified Location/Rotation. The real PlayerPawnExt constructor
also creates its native FlagBase. Neither birth is committed before taking the
external baseline: live FlagBase checkpoint/archive remains unsupported and is
not bypassed here. It queries the actual Root and a real child produced by
that original script/native initialization. No initialized bit or startup gate
is bypassed. The checks cover all255 cleared/written key bytes, values0/1/255,
later live changes including modifier keys, key255, typed Byte capture,
NoWrite/ZeroFill/Nothing and live terminal-type changes. A separate genuine
rootless base Window/child constructor fixture verifies absent-Root false.

Negative controls include wrong receiver/Int/malformed Byte/missing argument, malformed native
flags, an actual foreign EDrawStyle-bearing parameter field, parent cycle,
unsafe terminal-type cast, caller write/native-work limits and a late failure.
The foreign field control rejects its identity/program/schema, not an isolated
mutation of the original enum metadata. Every attempt requires complete native
window/player/FlagBase/counter/GC/lookup/collision/world-revision/save rollback,
including repeated birth-name identity and revoked player lookup; owned source
fingerprints and final shutdown ownership are checked. This does not establish
Quest controller processing or successful original startup. Actual build/test
outcomes belong in STATUS.md after the frozen combined gates.

The corrected owned run passes1137 checks/16 refusals,1042 queries and56584
native work. Its real Spawn+Root prefix performs16026 writes; the264 explicit
key/type fixture writes bring the total to16290, below the unchanged default
16384 limit. Repeated positive, late-failure, malformed ingress/authority,
rootless/cycle/type and shared write/work controls all restore the baseline.
The first fixture run failed on missing explicit FlagBase preparation; that
failure was not hidden by a weaker pawn or a native-save bypass. Strict ARM64,
Android-native and full desktop builds pass. Full-suite/audit evidence belongs
in STATUS.md. These results remain offline component proof, not Quest events.
