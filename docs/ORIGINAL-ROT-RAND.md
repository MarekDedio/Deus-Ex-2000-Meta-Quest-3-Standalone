# Original Core.Object.RotRand320

Read-only owned GOTY metadata/static disassembly,2026-10-23. This native is
required by real first-Training crate fragments: Fragment.Flying.BeginState
calls Projectile.RandSpin, which starts with RotRand(). No owned commercial
executable/DLL was loaded or executed, and none is redistributed.

Core.dll SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Export thunk10102EBE reaches body101391B0..10139239.

Original Core.u declaration: native320, flags0x2401, optional Bool bRoll with
flags0x90/dimension1, canonical Core.Object.Rotator return flags0x580/dimension1.
There are no other native inputs. At101391B8 the native initializes its INT
bRoll temporary to0 BEFORE argument evaluation; EndFunctionParms is consumed
at101391DA. Omitted or no-result input therefore means false, not stale input.

Original body calls appRand for Yaw FIRST, Pitch SECOND, and Roll THIRD only
when the full BOOL is nonzero. Each does signed `(appRand()<<1)%65535` through
SHL/CDQ/IDIV. The existing MSVCRT Draw15 range0..32767 makes that exactly
`draw*2`: even channels0..65534. SurrealEngine's scaled RAND_MAX formula is a
different distribution and is not substituted. Result stores are Pitch at0,
Yaw at4, Roll at8; all channels are fully written even with bRoll false.

The same rand thunk101032EC->101232D0->IAT102AB4FC is used by original Rand and
FRand. Reuse the already journaled portable game-thread seed, never host libc,
a new stream or reseeding on each call. Seed1 false/omitted yields
Pitch36934/Yaw82/Roll0, final seed3357800067; true adds Roll12668 and final
seed415139642. RandSpin then consumes three FRand draws for RotationRate in
Yaw/Pitch/Roll order, so its omitted-RotRand path consumes five draws total.

Required controls: original declaration/provenance/canonical return schema,
zero/one input, Bool/no-result ingress, false before evaluation, no malformed
call draws, exact mixed-stream continuation, save continuation and whole-root
late-failure rollback. This contract does not implement fragment physics,
usable loot, equipped VR input or completed Training startup.

The production adapter and asset-free generated-package controls now pass
86188 total random checks/67 rejection controls. Actual first Training fatal
damage passes the former RandSpin320 boundary and creates genuine fragments
and surviving contents; see TRAINING-STARTUP-CRATE-DAMAGE.md for the exact
pre-pairing/rollback scope. Android native link and strict ARM64 checks pass.
