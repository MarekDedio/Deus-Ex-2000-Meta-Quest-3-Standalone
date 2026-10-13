# Player/HUD integration checkpoint (2026-10-14)

This work follows the user's order: original Training music (accepted on Quest),
then the original HUD/starting keyring, before NPC animation and crate/held-item
gameplay. It is not a claim that the tutorial or campaign is finished.

## Compass

The production compass reads the first original live Level-slot
DeusExLevelInfo.TrueNorth, using the actual native ULevel identity. It is not
selected by export order, a class-name suffix or a fixed Training constant.
Missing original map info leaves the original initial zero; malformed metadata
does not silently turn into north. Map-worker preparation publishes this
snapshot with the corresponding map, including restoration.

Rendered heading derives from the same map/tracking-space transform as the
scene: UE X maps to local negative Z, and UE Y to local X. Physical head yaw,
snap turns, room-space rebases and Meta recentering therefore affect the
compass consistently. Looking vertically uses the horizontal head-right axis.
Invalid or wholly undefined tracking does not invent a direction. The 606
generated heading/placement controls cover these cases. The original 73x40 artwork is
cropped from its padded textures and rasterized only when the scrolling strip
changes by a logical pixel. COMPASS and screenshot metadata expose placement
and map-source state for subsequent physical verification.

The first installed build's compass worked but the user found its literal PC
screen-edge attachment almost outside the Quest FOV. The follow-up insets the
left health/compass column: compass center(-0.50,0.25,-1.65)m, retaining the
original 4mm/pixel scale, shared left edge and four-pixel vertical gap. Its
outer horizontal edge is within22 degrees of forward; the pending health
column stays within22 horizontal/26 vertical degrees. These are bounded
placement controls. A captured frame from the replacement APK verifies the
inward shift, and the user accepts the revised placement as comfortable after
confirming compass direction works. Broader map/recenter routes remain open.

## Original player prerequisites

The renderer's real native-client lease and registered stereo viewport now
have a session-owned UObject representation. Engine.Viewport inherits the
actual serialized Engine.Player schema; Engine.Client inherits Core.Object.
These native registrations, instances and GC roots are separate from map
exports, Actor births and class-default wrappers. They are transient engine
session state, not strings restored as native pointers from a save.

Preparation occurs after actual render-target registration and before map
loading. It does not create a PlayerPawn, start a world, call Login, invent
inventory or claim possession. The audited SetPlayer bridge pairs actual
Player.Actor and PlayerPawn.Player, then calls the most-derived original
Possess in one transaction. It requires a genuine live session and begun
native world; unsupported callbacks must roll back both pairing edges and
script/native changes. Client revocation is irreversible. Cold/checkpoint
restoration requires a later genuine rebind rather than reviving old pointers.

Retail static evidence: Engine.dll SetPlayer thunk 103037ec leads to body
10368c90; its event FName at 10583684 is Possess. SpawnPlayActor's separate
post-SetPlayer FName at 10583640 is TravelPreAccept, not a second Possess.
The original DLL/EXE was never executed.

## HUD state and verification boundary

Production full-HUD readers select only the actual paired viewport.Actor.
Unbound means unavailable, not healthy limbs or a synthetic keyring. The
snapshot uses six independent declared Pawn health Ints, declared player
Energy/EnergyMax, actual owned Inventory links, belt assignment/icon/description
and the real owned KeyRing. Cyclic, oversized, foreign-world and malformed
chains are refused. A separate explicitly detached actor reader exists for
original-script regression fixtures; it is not live possession evidence.

The original keyring control executes compiled CreateKeyRing, Spawn, GiveTo,
AddInventory, Idle2 and SetBase on a native-born JCDenton in an explicitly
dormant test world. That is stronger than drawing an icon, but still does not
prove Login, accepted world startup, native root-window creation, key collection,
belt activation, damage/healing or tutorial gameplay. The isolated Possess
control now passes original FMin244 and reaches Extension.PlayerPawnExt.Possess
PC6/native1052 InitRootWindow, rolling back its native pairing and complete
checkpoint. The unchanged Training startup probe
passes the former GameInfo.DefaultPlayerName ClassContext boundary and next
stops at Camera5's Engine.PlayerPawn.ConsoleCommand. Final coherent host tests
pass74 ordinary/86 registered with12 explicit owned-data skips. Separate
original-player/keyring/session controls pass169 checks using the same38 owned
packages as production, including actual keyring creation, idempotence,
birth-budget rollback, rooted warm/cold graph restoration, multiple viewports,
client revocation and permanent native UClass lifetimes. These remain isolated
controls, not live gameplay acceptance.

The shared compositor and original-data software previews cover belt, body
health and compass artwork. Their healthy/damaged/starting-ring values are
explicit presentation fixtures. Source files remain outside Git/APK and
unchanged. See ORIGINAL-HUD-ARTWORK.md and ORIGINAL-HUD-CONTRACT.md for exact
layout and remaining gameplay acceptance requirements.

Production prepares the original belt/health artwork, FontTiny and a bounded
icon catalog on the CPU worker, then uploads the textures on the render thread.
Only a real bound-player snapshot can make health/belt visible; unbound state
does not display a fixture ring or healthy body. Values are queried at most
10Hz and textures update only on snapshot changes. Retained actor-bank refreshes
do not hide the HUD during pickups; actual map workers/atlas replacement do.
Selection, ammo/counts, breath and damage/healing animation remain unavailable
until their original owners are implemented, rather than inferred from the
separate inventory-list selection or scalar player health.
