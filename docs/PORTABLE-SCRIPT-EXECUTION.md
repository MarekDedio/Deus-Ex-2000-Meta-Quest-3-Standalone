# Bounded original script execution

The portable interpreter executes the normalized bytecode from original UE1
packages; it does not substitute hand-written idle selection for the game's
compiled functions. This is an execution foundation, not campaign completion.
Quest links the same core, but automatic startup, AI states and live animation
ticks are not yet enabled.

## Implemented path

Typed locals, ordered parameters, out-parameter copyback, returns, assignments,
fixed-array indices, struct members, conditional/unconditional branches,
Self/object context, virtual/global/final calls, selected conversions and
numeric/vector operators are implemented. Calls resolve references and names
using the executing function's own package, not the map or caller's table.
Native boolean AND/OR are lazy; call arguments evaluate on caller Self before
invocation on the Context receiver. Native omitted arguments retain Nothing,
while script optional arguments initialize typed-zero locals.

The actor host stages actual PlayAnim (259), LoopAnim (260), TweenAnim (294),
animation queries and IsA. Actor properties and captured tween history are
visible in snapshots and the shared mesh sampler. Region.Zone is calculated
from the original BSP plane/front/back/zone/leaf records, with the original
LevelInfo fallback; it is not a guessed dry-room value.

Every root call is one bounded transaction. Failure rolls back nested actor
writes, out aliases and native animation commands. Unavailable natives are
errors, never successful no-ops. Limits cover bytecode, nodes, expression/call
depth, instructions, arguments, writes, strings, local elements and aggregate
retained value bytes. Diagnostics identify the function, logical offset,
opcode and call stack. Native argument aliases are synchronous and expire
after execution; native return values are detached snapshots.

## Animation clock contract

`quest_actor_animation_clock.h` implements command properties, main-channel
event boundaries, four independent blend slots and FinishAnim wait state.
The bridge currently dispatches only the three main non-latent commands above.
The clock is not yet connected to per-frame actor uploads or campaign events.

Notifies and AnimEnd must dispatch synchronously at each boundary, followed by
a fresh mesh/state/speed read before advancing residual time. Function existence
alone is insufficient: disabled events, state probe/ignore masks, level startup
and actor deletion gates remain necessary before enabling automatic callbacks.

Two deliberate corrections are labelled in clock results: pinned blend ticking
shares/mutates elapsed time and can starve later slots, and past-end main frames
can create negative elapsed consumption. The portable clock uses independent
blend elapsed time and nonnegative consumption. The pinned positive-frame
TweenBlendAnim behavior is retained and labelled, not reinterpreted as a tween.
Frame-1 history is captured without reading vertices; an unusable captured
history is diagnosed when sampled. Out-of-range notify times fail explicitly.

Implementation references are the locally pinned SurrealEngine commit
`677ee14c5b83486e6634687953779aafb7973ad6`: VM/Bytecode.cpp,
VM/ExpressionEvaluator.cpp, VM/ExpressionValue.h, VM/Frame.cpp, Native/NObject.cpp,
Native/NActor.cpp and UActor_Animation.cpp. This is a fidelity contract against
that implementation, not verification of the closed-source original DLL.

## Running offline tests

```powershell
.\desktop\build\quest_portable_vm_test.exe
.\desktop\build\quest_actor_animation_clock_test.exe
.\desktop\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex'
.\desktop\build\script_bytecode_inspect.exe 'D:\Steam\steamapps\common\Deus Ex' DeusEx.ScriptedPawn.PlayWaiting
```

The first two tests need no commercial assets. The original integration test
requires read-only data from a user-owned installation; without that argument
it explicitly skips. It exercises isolated original functions, not a campaign
playthrough. Test-generated checkpoints are outside the original installation.

An isolated visual fixture can execute an original helper before capturing:

```powershell
.\desktop\build\deusex_desktop_visual.exe --game-root 'D:\Steam\steamapps\common\Deus Ex' --map 00_Training --cache-root artifacts\script-cache --actor-isolate 00_Training.Doctor1 --actor-script-function LoopAnimPivot --actor-script-name BreatheLight --actor-script-float 1 --actor-script-float -0.1 --width 512 --height 512 --output artifacts\jaime-helper.bmp
```

This invokes the compiled helper once and samples its resulting pose. The JSON
records the invoked function and instruction/write counts. It does not tick
time, invent startup, or prove NPC AI, headset rendering or performance. Manual
sequence/frame/fatness overrides cannot be mixed with helper execution.

## Remaining requirements and persistence boundary

State startup/continuations, GotoState, latent calls, iterators, switches,
dynamic arrays, class-default object identity, remaining structs/natives,
event masks, RNG, attachment rendering and dynamic GPU pose updates remain
unfinished. Virtual calls currently resolve class hierarchy only because no
script state can become active. Unknown required behavior fails explicitly.

Version-4 runtime saves preserve supported actor overlays and the complete
native clocks, including captured tween histories. Untouched runtimes still
write version 3. Read-only validation checks original map/class/property schemas
and leaves live state untouched; v4 application requires its authored map.
Loading a validated legacy v1-v3 checkpoint explicitly clears overlays/clocks.
See [script-state saves](SCRIPT_STATE_SAVE.md) for the format and limits.

Map replacement and unload still refuse while script state exists, even after
successful saving: a per-map archive is required to retain the abandoned map and
make rollback safe. Quest checks this before cancelling UI/audio/geometry work.
Runtime shutdown explicitly discards state. These scoped saves do not serialize
latent continuations, spawned actors, timers, active script states or arbitrary
campaign systems; automatic gameplay does not yet invoke these execution APIs.

Full campaign progression, live animation/AI, Quest stereo rendering, physical
controllers and device performance still require implementation and verification.
