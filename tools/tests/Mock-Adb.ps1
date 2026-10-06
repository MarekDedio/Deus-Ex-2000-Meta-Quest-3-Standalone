# A deliberately small ADB process substitute. Only the diagnostic-mailbox
# protocol is implemented; it cannot address a real device or execute a shell.
$ErrorActionPreference = 'Stop'
if (-not $env:QUEST_DIAGNOSTIC_MOCK_STATE) {
    throw 'QUEST_DIAGNOSTIC_MOCK_STATE must identify a generated test fixture.'
}
$fixturePath = $env:QUEST_DIAGNOSTIC_MOCK_STATE
$fixture = Get-Content -LiteralPath $fixturePath -Raw | ConvertFrom-Json
$commandText = $args -join ' '
$fixture.Commands = @($fixture.Commands) + $commandText

function Complete-MockAdb([int]$ExitCode, [string]$Output = '') {
    $fixture | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $fixturePath -Encoding utf8
    if ($Output) { Write-Output ($Output -split '\r?\n') }
    exit $ExitCode
}

if ($commandText -eq 'devices') {
    if ($fixture.FailOperation -eq 'Devices') { Complete-MockAdb 1 }
    Complete-MockAdb 0 "List of devices attached`nMOCK_QUEST_SERIAL`tdevice"
}
if ($commandText -eq 'shell rm -f /sdcard/Android/data/dev.deusex.questvr.smoketest/files/quest-screenshot.bmp') {
    $fixture.ScreenshotDeletes++
    if ($fixture.FailOperation -eq 'ScreenshotDelete') { Complete-MockAdb 1 }
    Complete-MockAdb 0
}
if ($commandText -eq "shell if [ -s '/sdcard/Android/data/dev.deusex.questvr.smoketest/files/quest-screenshot.bmp' ]; then echo ready; fi") {
    $fixture.ReadinessPolls++
    if ($fixture.FailOperation -eq 'Readiness') { Complete-MockAdb 1 'ready' }
    Complete-MockAdb 0 'ready'
}
if ($args[0] -eq 'pull' -and
    $args[1] -eq '/sdcard/Android/data/dev.deusex.questvr.smoketest/files/quest-screenshot.bmp') {
    $fixture.Pulls++
    if ($fixture.FailOperation -eq 'Pull') { Complete-MockAdb 1 }
    # Generated 8x8, 32-bit BMP with eight distinct colors in its lower half.
    # This tests the real capture validator, not a pre-recorded Quest frame.
    $bytes = [byte[]]::new(54 + 8 * 8 * 4)
    $bytes[0] = [byte][char]'B'; $bytes[1] = [byte][char]'M'
    [BitConverter]::GetBytes([uint32]$bytes.Length).CopyTo($bytes, 2)
    [BitConverter]::GetBytes([uint32]54).CopyTo($bytes, 10)
    [BitConverter]::GetBytes([uint32]40).CopyTo($bytes, 14)
    [BitConverter]::GetBytes([int32]8).CopyTo($bytes, 18)
    [BitConverter]::GetBytes([int32]8).CopyTo($bytes, 22)
    [BitConverter]::GetBytes([uint16]1).CopyTo($bytes, 26)
    [BitConverter]::GetBytes([uint16]32).CopyTo($bytes, 28)
    for ($pixel = 0; $pixel -lt 64; $pixel++) {
        $bytes[54 + $pixel * 4] = [byte]($pixel * 3)
        $bytes[54 + $pixel * 4 + 1] = [byte]$pixel
        $bytes[54 + $pixel * 4 + 3] = 255
    }
    [IO.File]::WriteAllBytes($args[2], $bytes)
    Complete-MockAdb 0
}
if ($commandText -match 'run-as .* test -f files/DeusEx/Maps/[A-Za-z0-9_-]+\.dx$') {
    $fixture.MapChecks++
    if ($fixture.FailOperation -eq 'MapCheck') { Complete-MockAdb 1 }
    Complete-MockAdb 0
}
if ($commandText.Contains('echo pending; else echo clear; fi')) {
    $fixture.Inspections++
    if ($fixture.FailOperation -eq 'Inspect' -or
        ($fixture.FailOperation -eq 'InspectAfterQueue' -and $fixture.SuccessfulWrites -gt 0)) {
        Complete-MockAdb 1 'clear'
    }
    if ($fixture.FailOperation -eq 'UnexpectedState') { Complete-MockAdb 0 'ambiguous' }
    if ($fixture.Pending -and $fixture.Consume -and $fixture.SuccessfulWrites -gt 0) {
        if ($fixture.PollsBeforeConsume -le 0) {
            $fixture.ConsumedRequests = @($fixture.ConsumedRequests) + $fixture.Pending
            $fixture.Pending = $null
        } else {
            $fixture.PollsBeforeConsume--
        }
    }
    if ($fixture.Pending) { Complete-MockAdb 0 'pending' }
    Complete-MockAdb 0 'clear'
}
if ($commandText -match "set -C; echo ([A-Za-z0-9_-]+) > files/DeusEx/quest-map\.request'") {
    $request = $Matches[1]
    $fixture.WriteAttempts++
    if ($fixture.CollisionRequest) { $fixture.Pending = $fixture.CollisionRequest }
    # Match sh noclobber: an intervening writer is preserved, not overwritten.
    if ($fixture.Pending -or $fixture.FailOperation -eq 'Queue') { Complete-MockAdb 1 }
    $fixture.Pending = $request
    $fixture.LastRequest = $request
    $fixture.SuccessfulWrites++
    Complete-MockAdb 0
}
throw "Mock ADB received an unsupported operation: $commandText"
