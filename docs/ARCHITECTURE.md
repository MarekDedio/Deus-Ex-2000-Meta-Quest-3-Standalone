# Architecture

## Runtime layers

1. **Quest platform**: Android lifecycle, ARM64 packaging, storage access, and
   OpenXR session management.
2. **VR presentation**: stereo views, Vulkan rendering, controller poses,
   haptics, spatial audio, locomotion, comfort, and 3D UI.
3. **UE1 compatibility**: package serialization, object model, reflection,
   UnrealScript VM, BSP, meshes, animation, collision, audio, and saves.
4. **Deus Ex compatibility**: native functions, conversations, AI, inventory,
   augmentations, missions, goals, and map transitions.
5. **Game data**: read-only files supplied by the owner after installation.

## Non-goals

- Emulating the x86 Windows executable on Quest.
- Shipping copyrighted game content with the port.
- Replacing the original installation or changing its files.
- Claiming campaign compatibility before automated and headset testing proves it.

## Initial technical direction

The initial executable uses Meta's native OpenXR Android samples as a known-good
Quest lifecycle baseline. UE1 compatibility work will be evaluated in an isolated
fork of SurrealEngine because it already implements package loading and much of
the UnrealScript VM. Changes remain in the fork, in accordance with its maintainer
guidance, and are not submitted upstream without explicit approval.

