# Process substitute for launch/install tests. It cannot contact a device or
# execute arbitrary shell commands; every accepted operation is listed below.
$ErrorActionPreference = 'Stop'
if (-not $env:QUEST_LIFECYCLE_MOCK_STATE) {
    throw 'QUEST_LIFECYCLE_MOCK_STATE must identify a generated test fixture.'
}
$fixturePath = $env:QUEST_LIFECYCLE_MOCK_STATE
$fixture = Get-Content -LiteralPath $fixturePath -Raw | ConvertFrom-Json
$commandText = $args -join ' '
$fixture.Commands = @($fixture.Commands) + $commandText

function Complete-MockAdb([int]$ExitCode, [string]$Output = '') {
    $fixture | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $fixturePath -Encoding utf8
    if ($Output) { Write-Output ($Output -split '\r?\n') }
    exit $ExitCode
}

if ($commandText -eq 'devices') {
    if ($fixture.FailOperation -eq 'Devices') {
        Complete-MockAdb 1 $fixture.Devices
    }
    Complete-MockAdb 0 $fixture.Devices
}
if ($args.Count -lt 3 -or $args[0] -cne '-s' -or $args[1] -cne $fixture.ExpectedSerial) {
    throw "Mock ADB operation did not target the expected serial: $commandText"
}
$operation = ($args | Select-Object -Skip 2) -join ' '
if ($operation -eq 'shell am start -W -n dev.deusex.questvr.smoketest/dev.deusex.questvr.MainActivity') {
    $fixture.Launches++
    if ($fixture.FailOperation -eq 'Launch') { Complete-MockAdb 1 'Status: ok' }
    if ($fixture.FailOperation -eq 'LaunchStatus') { Complete-MockAdb 0 'Error: Activity not started' }
    Complete-MockAdb 0 'Status: ok'
}
if ($operation -eq 'shell pidof dev.deusex.questvr.smoketest') {
    $fixture.ProcessPolls++
    if ($fixture.FailOperation -eq 'Process') { Complete-MockAdb 1 '1234' }
    if ($fixture.FailOperation -eq 'ProcessText') { Complete-MockAdb 0 'not a process id' }
    if ($fixture.ProcessPolls -le $fixture.EmptyProcessPolls) { Complete-MockAdb 1 }
    Complete-MockAdb 0 '1234 1235'
}
if ($args.Count -eq 5 -and $args[2] -ceq 'install' -and $args[3] -ceq '-r' -and
    $args[4] -ceq $fixture.ExpectedApk) {
    $fixture.Installs++
    if ($fixture.FailOperation -eq 'Install') { Complete-MockAdb 1 'Success' }
    Complete-MockAdb 0 'Success'
}
if ($operation -eq 'shell am force-stop dev.deusex.questvr.smoketest') {
    $fixture.Stops++
    if ($fixture.FailOperation -eq 'Stop') { Complete-MockAdb 1 }
    Complete-MockAdb 0
}
throw "Mock ADB received an unsupported operation: $commandText"
