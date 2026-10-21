# Combat stress measurement (developer build): flow phase 7 spawns mixed
# groups of 10, 25 and 50 enemies around an invulnerable Warrior, 12 seconds
# each, and prints the frame-timing summary per enemy-count bucket.
#
#   powershell -ExecutionPolicy Bypass -File tools\run_combat_stress.ps1 [-Build] [-Configuration Release|Debug]
#
# Use Release for numbers (Debug is unoptimized). Opens a game window; uses a
# brand-new sandbox profile (MO_PROFILE_DIR), never a real profile. Not part of
# tools\verify_native.ps1.
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

$runRoot = Join-Path $projectRoot ('x64\CombatStress\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$profileDir = Join-Path $runRoot 'profile'
New-Item -ItemType Directory -Path $profileDir -Force | Out-Null

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exe
$psi.WorkingDirectory = Join-Path $projectRoot 'TestGame'   # dev builds resolve assets from here
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.EnvironmentVariables['MO_FLOW_SELFTEST'] = '7'
$psi.EnvironmentVariables['MO_PROFILE_DIR'] = $profileDir
$p = [System.Diagnostics.Process]::Start($psi)
$out = $p.StandardOutput.ReadToEndAsync()
$err = $p.StandardError.ReadToEndAsync()
if (!$p.WaitForExit($TimeoutSeconds * 1000)) {
    try { $p.Kill() } catch {}
    throw "Combat stress timed out after $TimeoutSeconds s (log folder: $runRoot)"
}
$log = Join-Path $runRoot 'stress.log'
($out.Result + $err.Result) | Set-Content -LiteralPath $log -Encoding UTF8
Get-Content -LiteralPath $log | Select-String -Pattern 'STRESS group|FRAMETIME|FAIL' | ForEach-Object { ($_.Line -replace '^.*?(STRESS|FRAMETIME|FAIL)', '$1') }
Write-Output "Exit code $($p.ExitCode). Full log: $log"
exit $p.ExitCode
