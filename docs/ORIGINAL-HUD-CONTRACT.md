# Original player HUD and Training bootstrap

The 2026-10-13 PC references identify missing gameplay systems, not merely
missing textures. These facts were read from the user's original GOTY packages
and installed configuration without running the original Windows executable.
This document records the implementation contract; it is not a completion claim.

## Starting NanoKeyRing

Training has no authored NanoKeyRing actor. DeusExPlayer.PostBeginPlay calls
InitializeSubSystems (PC78), whose standalone branch calls CreateKeyRing (PC244).
CreateKeyRing spawns a real NanoKeyRing with Owner=Self, assigns Player.KeyRing,
sets InitialState=Idle2, calls GiveTo(Self), and sets its base to the player.
GiveInitialInventory is a different path and is not the source of this device.

The ring is deliberately hidden and bDisplayableInv=false. It belongs in the
special object-belt slot, not the main inventory grid. Its original icons are
DeusExUI.Icons.BeltIconNanoKeyRing and LargeIconNanoKeyRing (logical47x44).
HasKey traverses the actual player's KeyList of NanoKeyInfo.NextKey nodes.
CreateNanoKeyInfo uses original New with player Outer; NanoKeyInfo is a
Core.Object subclass, not an Actor. Ownership, GC edges and save/load of that
graph are required. A UClass path or a drawn icon is not an inventory instance.

## Ten quick-use slots

HUDObjectBelt creates slots0..9. NanoKeyRing is forced to belt position0;
ordinary standalone allocation starts at1. PopulateBelt traverses the actual
Player.Inventory chain and reads each item's bInObjectBelt and beltPos.
Activation follows ActivateBelt -> RootWindow.ActivateObjectInBelt ->
belt.GetObjectFromBelt, then either changes a weapon's ammo or invokes
PutInHand/SetInHandPending on the real item. The main inventory's selected
list index is not this contract.

The authored belt is541x69; ordinary slots51x54, special slot width44, icon
area42x37. Captions use FontTiny and actual objectNum (the reference shows
1..9,0). Original DeusExUI.UserInterface assets include
HUDObjectBeltBackground_{Left,Right,Cell,Divider}, HUDObjectBeltBorder_1..3
and the PersonaItemHighlight nine-slice pieces.

## Body health and compass

DeusExHUD.ConfigurationChanged places HUDHitDisplay at(0,0) and the compass
at(0,hitHeight+4), ordinarily(0,110). The body pane is84x106. Its six independent
values are Engine.Pawn.HealthHead, HealthTorso, HealthArmLeft, HealthArmRight,
HealthLegLeft and HealthLegRight (authored defaults100), with real Energy,
EnergyMax and swimming state. The port's separate persistentPlayerHealth
scalar does not establish these limb values or their damage/healing behavior.

Original body art is HUDHitDisplayBackground_1, HUDHitDisplayBorder_1,
HUDHitDisplay_Body and the six corresponding part overlays. Window.GetColorScaled
and SetHitColor control independent part colors and damage/healing flashes.
Palette RGB with authored alpha zero is not equivalent to transparent RGBA.

The compass is73x40 with a55x22 clip at(13,7). It wraps a240x15 strip using
65536/240 rotation units per pixel and ((Player.Rotation.Yaw-mapNorth)&65535).
mapNorth comes from the real DeusExLevelInfo.TrueNorth; Training's value is0.
HUDCompassTicks is256x32 including padding, so stretching that whole texture
would be wrong. Other assets are HUDCompassBackground_1, HUDCompassBorder_1,
and HUDCompassTickBox.

## Required runtime integration

Original config selects DeusEx.JCDentonMale and DeusEx.DeusExRootWindow and
enables all three HUD elements. Extension.PlayerPawnExt.Possess invokes native
1052 InitRootWindow; the root binds parentPawn and constructs the HUD/window
graph. Non-Actor window creation and lifetime, possession, original player
startup, inventory ownership, item-in-hand state and key-list persistence
remain open. The current original startup probe still refuses ClassContext.

Implementation acceptance requires a fresh Training start with an actual
owned ring, functional key collection/door use, ten correctly mapped slots,
activation/save restoration, independent limb damage/healing, and map-relative
compass behavior during physical turning, snap-turn and Meta recentering.
Original artwork must accompany those states rather than replace them.
