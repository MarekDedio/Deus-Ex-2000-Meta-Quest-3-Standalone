# Original speech UI on Quest

The remote Training transmissions now use the original `HUDInfoLinkDisplay`
artwork, bitmap fonts and speaker photograph, decoded from the user's own game
installation. No commercial assets are copied into Git or the APK.

## Original-data evidence

Static inspection of `DeusEx.u` establishes:

- `HUDInfoLinkDisplay.InitWindow`: a logical 431×114 window.
- `CreateControls`: a 64×64 portrait at (20,25), a 293×15 speaker-name window at
  (94,15), a speech window at (94,36) with 42 columns and six rows of 7×10 cells,
  and a 293×1 separator at (94,32).
- `DrawBackground` and `DrawBorder`: first 256×114 pixels of texture one and
  first 175×114 pixels of texture two. Source textures are 256×128; their padding
  must be clipped, not stretched.
- Original UI textures: `DeusExUI.UserInterface.HUDInfolinkBackground_1/2` and
  `HUDInfolinkBorder_1/2`.
- Original fonts: `DeusExUI.FontMenuHeaders_DS` and `FontFixedWidthSmall_DS`.
- `SetSpeaker`: `InfoPortraits.` plus the first 16 characters of the speech
  speaker's bind name. Paul uses the one-based player-skin suffix; Bob Page uses
  `BobPageAug` in mission 15. No arbitrary NPC portrait is substituted when the
  exact texture is missing.
- `DataLinkPlay.infoLinkNames` and `GetDisplayName`: the authored 17-name table,
  exact string comparison and blank result for an unknown speaker.
- `ColorThemeHUD_Default`: background tint 127, borders and header text 255,
  normal text 200. These tints apply to the original art/font pixels, not to the
  full-color photograph.

`InfoPortraits.utx` contains the actual 64×64 `JaimeReyes` photograph used by the
opening transmission. It is not a newly drawn avatar or a mesh screenshot.

## Implementation and scope

`native/quest_dialogue_ui.h` is the shared CPU compositor. Quest decodes the art,
fonts and available portraits once on the startup worker. During a transmission,
it updates the small texture only when the visible character count or cursor
blink changes. Asset decoding is not performed while a door animates.
Typing and expiry advance together before any tracking-related early return;
a lost head pose or long frame cannot leave typing behind the audio/deadline.
Speech expiry and progressive typing advance together before tracking-related
early returns, so a temporarily lost pose or a long frame does not leave the
typewriter behind the still-playing audio. Rendering still requires valid
tracking.

The panel preserves the original pixel layout and aspect ratio. Its binocular,
head-relative attachment above the center ray is a VR adaptation, not the PC
viewport's screen coordinate. Long text scrolls through six fixed-cell rows at
the original 0.03-second character interval; the bounded compositor is not
execution of `Extension.ComputerWindow`'s native command queue. The static/noise
transition effects and original start/end-transmission timing are not complete.
Paul's current port presentation uses player skin zero; custom player-skin
selection still needs connection to the live player metadata.

This applies only to remote datalinks, where the PC game actually shows a
portrait. Face-to-face conversations use the separate original `ConWindowActive`
cinematic UI and camera behavior; the port's existing subtitle/choice presentation
is still a fallback there. Do not claim this patch finishes that UI or the full
Training conversation interpreter.

## Verification

`desktop/dialogue_ui_test.cpp` uses synthetic images to check clipping, tint,
portrait placement, exact speaker selection, progressive typing, wrapping,
scrolling and bounded refusals. It does not prove original asset appearance.

`desktop/dialogue_ui_original_test.cpp GAME_ROOT [OUTPUT.bmp]` independently
checks actual class-default art/font references and font-cell dimensions, decodes
the real portraits/fonts and renders the actual opening `ConSpeech79` line.
Without an owned game installation the registered test skips with code 77.
The optional bitmap must be written outside the commercial installation and kept
in an ignored artifact directory. It is an offline compositor preview, not an
on-device capture or proof of original native UI execution.

`desktop/dialogue_ui_audit.cpp PACKAGE FILTER` is a read-only package export and
class-default audit tool. `script_bytecode_inspect` reads the functions named
above without running the original executable or DLLs.
