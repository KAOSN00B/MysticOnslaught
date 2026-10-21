# End-to-end flow self-test: drives the REAL game (dev build) through
#   new profile -> village -> Zeph/Poe purchases -> run -> boss -> relic ->
#   world map -> Zeph's camp -> Second Wind -> death -> Poe -> class select ->
#   village building transaction -> victory -> home
# then relaunches the game to verify everything that should persist did, and
# finally tours every domain (phase 3) through the real world-map entry path,
# checking each uses authored rooms and saving screenshots for review.
#
#   powershell -ExecutionPolicy Bypass -File tools\run_flow_selftest.ps1 [-Build] [-Configuration Release|Debug]
#
# Uses a brand-new sandbox profile (MO_PROFILE_DIR) every run; never touches a
# real profile. Exit code 0 = both phases passed.
param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [switch]$Build,
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if ($Build) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration | Out-Host }
$exe = Join-Path $projectRoot "x64\Build-$Configuration\TestGame.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "No $Configuration build at $exe (run tools\build.ps1 -Configuration $Configuration or pass -Build)." }

$runRoot = Join-Path $projectRoot ('x64\FlowTest\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$profileDir = Join-Path $runRoot 'profile'
New-Item -ItemType Directory -Path $profileDir -Force | Out-Null

function Invoke-Phase([int]$phase, [string]$profile = $profileDir) {
    $log = Join-Path $runRoot "phase$phase.log"
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.WorkingDirectory = Join-Path $projectRoot 'TestGame'   # dev builds resolve assets from here
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.EnvironmentVariables['MO_FLOW_SELFTEST'] = "$phase"
    $psi.EnvironmentVariables['MO_PROFILE_DIR'] = $profile
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEndAsync()
    $err = $p.StandardError.ReadToEndAsync()
    if (!$p.WaitForExit($TimeoutSeconds * 1000)) {
        try { $p.Kill() } catch {}
        throw "Phase $phase timed out after $TimeoutSeconds s (log: $log)"
    }
    ($out.Result + $err.Result) | Set-Content -LiteralPath $log -Encoding UTF8
    $report = Join-Path $profile "flowtest_report_phase$phase.txt"
    if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report | ForEach-Object { Write-Host $_ } }
    else { Write-Host "(no report written; see $log)" }
    return [int]$p.ExitCode
}

$code1 = Invoke-Phase 1
Write-Output "Phase 1 exit code: $code1"
$code2 = 1
if ($code1 -eq 0) {
    $code2 = Invoke-Phase 2
    Write-Output "Phase 2 exit code: $code2"
}
$code3 = 1
if ($code1 -eq 0 -and $code2 -eq 0) {
    $tourProfile = Join-Path $runRoot 'profile_tour'
    New-Item -ItemType Directory -Path $tourProfile -Force | Out-Null
    $code3 = Invoke-Phase 3 $tourProfile
    Write-Output "Phase 3 exit code: $code3"
}
$code4 = 1; $code5 = 1
if ($code3 -eq 0) {
    $resumeProfile = Join-Path $runRoot 'profile_resume'
    New-Item -ItemType Directory -Path $resumeProfile -Force | Out-Null
    $code4 = Invoke-Phase 4 $resumeProfile
    Write-Output "Phase 4 exit code: $code4"
    if ($code4 -eq 0) {
        $code5 = Invoke-Phase 5 $resumeProfile
        Write-Output "Phase 5 exit code: $code5"
    }
}
$code6 = 1
if ($code1 -eq 0) {
    $eventsProfile = Join-Path $runRoot 'profile_events'
    New-Item -ItemType Directory -Path $eventsProfile -Force | Out-Null
    $code6 = Invoke-Phase 6 $eventsProfile
    Write-Output "Phase 6 exit code: $code6"
}
# The Character Animator stands alone (no play-through needed).
$animatorProfile = Join-Path $runRoot 'profile_animator'
New-Item -ItemType Directory -Path $animatorProfile -Force | Out-Null
$code8 = Invoke-Phase 8 $animatorProfile
Write-Output "Phase 8 exit code: $code8"
Write-Output "Sandbox and logs: $runRoot"
if ($code1 -ne 0 -or $code2 -ne 0 -or $code3 -ne 0 -or $code4 -ne 0 -or $code5 -ne 0 -or $code6 -ne 0 -or $code8 -ne 0) { Write-Output 'FLOW SELF-TEST FAILED'; exit 1 }
Write-Output 'FLOW SELF-TEST PASSED'
