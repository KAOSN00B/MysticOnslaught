# Native release gate: builds every supported native configuration, runs every
# registered C++ suite, and optionally the end-to-end flow self-test.
#
#   powershell -ExecutionPolicy Bypass -File tools\verify_native.ps1          # builds + suites
#   powershell -ExecutionPolicy Bypass -File tools\verify_native.ps1 -Flow    # + flow self-test (opens the game window)
#
# Each step runs in its own PowerShell process (the build scripts import the
# vcvars environment). Exit code 0 only when every step passed. Web builds are
# discontinued and are not part of this gate.
param(
    [switch]$Flow,
    [switch]$IncludeContent,   # also run the slow full-room-library validation suite
    [string]$VsInstall,
    [string]$RaylibDir
)
$ErrorActionPreference = 'Stop'
$tools = $PSScriptRoot

$toolchainArgs = @()
if ($VsInstall) { $toolchainArgs += @('-VsInstall', $VsInstall) }
if ($RaylibDir) { $toolchainArgs += @('-RaylibDir', $RaylibDir) }

# Shipping = full game, PublicDemo = itch demo, Release = developer build the flow self-test drives.
$steps = @()
foreach ($configuration in 'Shipping', 'PublicDemo', 'Release') {
    $steps += ,@("Build $configuration", (Join-Path $tools 'build.ps1'), (@('-Configuration', $configuration) + $toolchainArgs))
}
$testArgs = $toolchainArgs
if ($IncludeContent) { $testArgs += '-IncludeContent' }
$steps += ,@('C++ suites', (Join-Path $tools 'run_tests.ps1'), $testArgs)
if ($Flow) { $steps += ,@('Flow self-test', (Join-Path $tools 'run_flow_selftest.ps1'), @('-Configuration', 'Release')) }

$results = @()
foreach ($step in $steps) {
    $name, $script, $stepArgs = $step
    Write-Output "=== $name"
    $started = Get-Date
    & powershell -NoProfile -ExecutionPolicy Bypass -File $script @stepArgs | Out-Host
    $code = $LASTEXITCODE
    $results += [pscustomobject]@{
        Step    = $name
        Result  = $(if ($code -eq 0) { 'PASS' } else { "FAIL ($code)" })
        Seconds = [int]((Get-Date) - $started).TotalSeconds
    }
}

$results | Format-Table -AutoSize | Out-String | Write-Output
$failed = @($results | Where-Object Result -ne 'PASS')
if ($failed.Count -gt 0) { Write-Output "NATIVE GATE FAILED: $($failed.Count) of $($results.Count) step(s)."; exit 1 }
Write-Output "NATIVE GATE PASSED ($($results.Count) steps)"
