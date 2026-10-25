# Original MAIN-to-BLEND animation continuation

First-Training actor-update integration, 2026-10-25. This implements the ordinary
four-slot blend phase and a committing MAIN+BLEND channel entry. It is not full
Actor/Level Tick, a new NPC presentation policy or automatic Quest physics.

## Original evidence

Owned GOTY Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Static objdump reads only; no commercial EXE/DLL is loaded or executed.

The original AActor.Tick MAIN loop initializes one Remaining Float local at
103A1BF6 and one counter at103A1BF3. MAIN exits at103A1FA1. The blend phase
**does not reset either local**. It rechecks live MAIN AnimSequence, Frame and
Rate-or-TweenRate, then shared Remaining>0 and the shared counter increment
before compare<=4 (103A1FA1..1FCC,103A21BC..21EE). An admitted pass scans
slots0..3; its end jumps back to103A1FA1. Four is the total MAIN+BLEND body
iteration allowance, not four per channel. MAIN's failed fifth increment is
retained too.

These native associations are not portable memory offsets:

| Actual Actor field | Native association | Declared dimension/flags |
| --- | --- | --- |
| BlendAnimSequence | +68+4i | Name[4],20hex |
| BlendAnimFrame | +78+4i | Float[4],0 |
| BlendAnimRate | +88+4i | Float[4],0 |
| BlendTweenRate | +98+4i | Float[4],0 |
| BlendAnimLast | +23C+4i | Float[4],0 |
| BlendAnimMinRate | +24C+4i | Float[4],20hex |
| SimBlendAnim | +26C+16i | Core.Object.Plane[4],20hex |

Per slot (103A2201..23C9):

1. None sequence skips everything else. Frame>=Last skips rate/tween work.
   Capture old Frame, then choose tween versus ordinary playback from Frame<0.
2. Ordinary nonnegative Rate stores Float(Frame+Remaining*Rate), with PC64
   intermediate operations. Negative Rate loads Velocity Z,Y,X, forms
   (X²+Y²)+Z², spills a Double argument to original Core.appSqrt, and **spills
   its sqrt return to Double** at103A2299. This differs from MAIN's retained
   extended sqrt. It then spills Float(-storedDoubleSpeed*BlendRate) at
   103A22B2 **before** max(BlendAnimMinRate,that Float). It is not the older
   corrected driver's min(speedRate,Last).
3. The endpoint comparison reloads stored Float Frame. Below Last, shared
   Remaining stays unchanged; later outer passes can advance that residual
   again. No per-slot elapsed consumption is added.
4. Reaching Last computes a residual from stored Frame, Last and old Frame.
   It copies Last to Frame, writes Rate+0, reads RemoteRole, then divides and
   stores shared Remaining. These prefix stores precede a reached divide
   failure. Role<2 and actual non-Weapon ancestry update SimBlendAnim
   X=Frame*10000 and Y=Rate*5000, capped32767. Y's Float store does not pop the
   PC64 product before the clamp comparison. No blend callback, sequence clear,
   finished flag or loop behavior is synthesized.
5. Tween computes PC64(Frame+Remaining*TweenRate), FSTs Float but tests the
   **unrounded** value against zero. A crossing stores Frame+0 and computes a
   residual from the PC64 tentative value and old Frame. Ordinary playback
   resumes only on a later pass, not immediately in that slot.

There is **no Remaining>0 gate between slots**. A zero residual from an earlier
slot still flows through later active slots before the next outer gate.

Reproduction:

```powershell
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A1BF0 --stop-address=0x103A1C66 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A1FA1 --stop-address=0x103A1FD5 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A21BC --stop-address=0x103A23CE 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
```

## Runtime integration

quest_actor_blend_phase.h implements the source flow with actual MAIN outcome
carry, fresh fields and existing host-independent PC64 dyadic arithmetic. Every
provider borrows caller work; the fixed native counter bounds all scans. Missing
reached providers, active nonfinite/overflow/zero-divisor arithmetic and invalid
MAIN continuations explicitly refuse. Inactive None/rate/time branches do not
eagerly demand other providers or finite data.

portable_actor_blend_phase.inl authenticates actual Engine.Actor array[4]
fields and exact Core.Object.Plane declarations/channels. Native stores use the
same Execution write journal, update only the selected slot/Plane component,
and mirror existing saved/render animation fields without rebuilding unrelated
clocks, history or callback-mutated state.

AdvancePortableActorAnimationChannels runs genuine MAIN and feeds its outcome
into genuine BLEND in one **committing** root. It does not replace Tick prefix
or suffix. No fresh whole-delta-per-channel clock, fake elapsed debt, forced
startup, selected idle sequence or fabricated player is introduced. The old
corrected portable blend clock remains a distinct legacy/presentation helper,
not this native continuation authority.

## Verification and remaining work

The helper passes2658 checks/60 explicit refusals: MAIN gates and failed cap
carry, repeated scans, shared end/tween residuals, exact endpoint zero visiting
later slots, negative-speed max/min, Weapon/Role Sim fields, native prefix writes
and every admitted provider-work cutoff. Strict ARM64 syntax checks of the
helper and owned test translation units pass.

The owned Training test uses actual Doctor1 mesh, genuine PlayAnim,
PlayBlendAnim/TweenBlendAnim packets, and keeps the original world dormant. It
checks real four-slot state, saved/native/render clock coherence, committing
updates and all write-prefix/selected native-work failures. The final owned
run passes576 checks/22 refusal prefixes. The existing actual Training crate
regression passes415/5, and the dormant Engine collision/config fixture
passes15156/31. These are distinct fixtures, not completed world startup or
live Quest scheduling.

The final full desktop build, strict ARM64 syntax gate and Android native
link pass. The shared CTest suite has239 registered tests:200 execute,
39 explicitly skip without owned data, zero failures (79.35 seconds). The
owned tests above ran separately with the genuine installation. Normal
Training startup repeat2 retains220285 instructions/25439 writes; the aimed
two-crowbar repeat2 retains228837/26047, including the original lower-crate
HP10->4->-2/removal and surviving Lockpick birth. Both intentionally roll
back at the named bound-player ConsoleCommand FExec refusal; neither is a
successful or playable startup. All45 checked original script/config/map
files retain their length, exact modification time and SHA256 after those
tests. No commercial DLL/EXE is executed, and no APK is generated.

Next is genuine stasis/owner/parity and Level pending/live-slot scheduling
around these channels and the implemented post-animation phase. See
ORIGINAL-ACTOR-STASIS-PREREQUISITES.md for exact renderer-time, distance-prepass
and NetMode ownership gaps. Controller weapon Fire, original inventory/loot
collection, native deferred cleanup/save and renderer publication remain
unfinished. No APK or headset acceptance follows from channel tests.
