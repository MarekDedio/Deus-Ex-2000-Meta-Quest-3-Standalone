# Quest diagnostics

The app's diagnostic mailbox contains one request at a time. Use the helper
instead of writing directly to `quest-map.request`; replacing a pending menu
or map command with a screenshot can make a test observe the wrong state.

```powershell
.\tools\Send-QuestDiagnostic.ps1 -Command MENU
.\tools\Capture-QuestScreenshot.ps1 -OutputPath artifacts\inventory.bmp
.\tools\Send-QuestDiagnostic.ps1 -Command PAGE
.\tools\Capture-QuestScreenshot.ps1 -OutputPath artifacts\health.bmp
```

The helper waits for an existing request to be consumed, uses shell noclobber
when queuing its own request, then waits for consumption. A timeout leaves the
request intact. An ADB error is a failure, not evidence of consumption. It
requires exactly one authorized device; `-AdbPath` supports a different SDK
location. MENU toggles the panel, PAGE advances through its four pages,
TURNLEFT/RIGHT are diagnostic snap turns, and PICKUP changes the live inventory
for an icon test. None of these commands writes a save.

```powershell
.\tools\Send-QuestDiagnostic.ps1 -MapName '06_HongKong_MJ12lab'
```

Map requests require that original map to be deployed. `Consumed = True`
means only that the app read the command, not that a level or GPU upload
finished. Before capturing a newly requested map, inspect `quest_main` logcat
for `staged visual runtime transition complete` with the requested name; a
preparation/transition failure must not be treated as a successful load.

Screenshots read the actual resolved left-eye framebuffer. They are diagnostic
captures, not comfort/performance measurements: synchronous readback can add a
large frame-time spike. The helper rejects nearly uniform lower-half readback,
but a nonuniform texture filling the view can still be an unhelpful viewpoint.
Always inspect the image. A valid tracked headset pose is required; an awake
app loop is not evidence of valid tracking, and safety/tracking settings should
not be bypassed to force a frame.

The mailbox/capture failure paths have a headset-free regression suite:

```powershell
.\tools\tests\Test-QuestDiagnostic.ps1
```

It substitutes a mock ADB process, uses generated BMP/state fixtures in a unique
temporary directory, and cleans up only that directory. It tests consumption,
pending-request preservation, noclobber collisions, unsafe tokens, and failures
at each ADB phase. It does not claim to validate Quest GPU rendering or tracking.
