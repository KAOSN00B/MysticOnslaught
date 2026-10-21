# Builds the game with MSBuild using discovered toolchain paths.
#
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1                       # Shipping (full game)
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Configuration PublicDemo
#
# Output: x64\Build-<Configuration>\TestGame.exe (intermediates beside it), so a
# fresh build never mixes with the older hand-named candidate folders.
# source.sha256 beside the exe records which source state was compiled.
param(
    [ValidateSet('Shipping','PublicDemo','Release','Debug')][string]$Configuration = 'Shipping',
    [string]$VsInstall,
    [string]$RaylibDir,
    [switch]$Rebuild
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\MoToolchain.ps1"
$projectRoot = Split-Path $PSScriptRoot -Parent
$vs = Resolve-MoVisualStudio $VsInstall
$raylib = Resolve-MoRaylib $RaylibDir
Import-MoVcVars $vs
$msbuild = Find-MoMsBuild $vs

$outDir = Join-Path $projectRoot "x64\Build-$Configuration\"
$intDir = Join-Path $projectRoot "x64\Build-$Configuration-Obj\"
$target = if ($Rebuild) { 'Rebuild' } else { 'Build' }
# Fingerprint the source that this build compiles (taken before MSBuild starts).
$fingerprint = Get-MoSourceFingerprint $projectRoot
$project = Join-Path $projectRoot 'TestGame\TestGame.vcxproj'
& $msbuild $project "/t:$target" /m /nologo /verbosity:minimal `
    "/p:Configuration=$Configuration" /p:Platform=x64 `
    "/p:OutDir=$outDir" "/p:IntDir=$intDir" "/p:MoRaylibDir=$raylib"
if ($LASTEXITCODE -ne 0) { throw "MSBuild failed ($LASTEXITCODE)" }
$exe = Join-Path $outDir 'TestGame.exe'
if (!(Test-Path -LiteralPath $exe)) { throw "Build reported success but $exe is missing" }
Set-Content -LiteralPath (Join-Path $outDir 'source.sha256') -Value $fingerprint -Encoding ASCII
Write-Output $exe
