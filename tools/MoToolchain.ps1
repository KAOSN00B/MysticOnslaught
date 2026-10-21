# Shared toolchain discovery for the Mystic Onslaught build/test scripts.
# Dot-source this file: . "$PSScriptRoot\MoToolchain.ps1"
#
# Resolution order (first hit wins):
#   Visual Studio : -VsInstall argument, %MO_VS_INSTALL%, vswhere (latest with
#                   the x64 C++ tools), then the original B:\VisualStudio layout.
#   raylib (MSVC) : -RaylibDir argument, %MO_RAYLIB_DIR%, C:\CLibraries\raylib-5.5_win64_msvc16.
# Nothing here is specific to one developer's drive layout; the old paths are
# only last-resort fallbacks so existing machines keep working unchanged.

function Resolve-MoVisualStudio([string]$Requested) {
    $candidates = @()
    if ($Requested) { $candidates += $Requested }
    if ($env:MO_VS_INSTALL) { $candidates += $env:MO_VS_INSTALL }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($found) { $candidates += ($found | Select-Object -First 1) }
    }
    $candidates += 'B:\VisualStudio'
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'VC\Auxiliary\Build\vcvars64.bat'))) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'Visual Studio with the x64 C++ tools was not found. Install it or set MO_VS_INSTALL to its installation folder.'
}

function Resolve-MoRaylib([string]$Requested) {
    $candidates = @()
    if ($Requested) { $candidates += $Requested }
    if ($env:MO_RAYLIB_DIR) { $candidates += $env:MO_RAYLIB_DIR }
    $candidates += 'C:\CLibraries\raylib-5.5_win64_msvc16'
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'include\raylib.h')) -and
            (Test-Path -LiteralPath (Join-Path $candidate 'lib\raylib.lib'))) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'raylib 5.5 (MSVC build) was not found. Set MO_RAYLIB_DIR to the folder containing include\raylib.h and lib\raylib.lib.'
}

# Imports the vcvars64 environment into this PowerShell process. Windows
# environment names are case-insensitive, so importing through $env: collapses
# any duplicated Path/PATH entries that broke child compilers launched from
# MSYS/Git Bash shells.
function Import-MoVcVars([string]$VsInstall) {
    $vcvars = Join-Path $VsInstall 'VC\Auxiliary\Build\vcvars64.bat'
    $lines = & cmd.exe /d /c "`"$vcvars`" >nul 2>nul && set"
    if ($LASTEXITCODE -ne 0) { throw "vcvars64.bat failed: $vcvars" }
    foreach ($line in $lines) {
        $eq = $line.IndexOf('=')
        if ($eq -gt 0) { Set-Item -Path ("Env:" + $line.Substring(0, $eq)) -Value $line.Substring($eq + 1) }
    }
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'cl.exe is not available after importing vcvars64.' }
}

function Find-MoMsBuild([string]$VsInstall) {
    $msbuild = Join-Path $VsInstall 'MSBuild\Current\Bin\MSBuild.exe'
    if (!(Test-Path -LiteralPath $msbuild)) { throw "MSBuild.exe not found under $VsInstall" }
    return $msbuild
}

# Content hash of every compiled game input (TestGame *.cpp/*.h/*.vcxproj).
# build.ps1 stamps it beside the exe; package_shipping.ps1 refuses a build
# whose stamp no longer matches the source (timestamps alone can lie).
function Get-MoSourceFingerprint([string]$ProjectRoot) {
    $gameDir = Join-Path $ProjectRoot 'TestGame'
    $lines = Get-ChildItem -LiteralPath $gameDir -File |
        Where-Object { $_.Extension -in '.cpp', '.h', '.vcxproj' } |
        Sort-Object Name |
        ForEach-Object { "$($_.Name):$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)" }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([System.BitConverter]::ToString($sha.ComputeHash($bytes)) -replace '-', '') }
    finally { $sha.Dispose() }
}

function Find-MoCrtRedist([string]$VsInstall) {
    $redistRoot = Join-Path $VsInstall 'VC\Redist\MSVC'
    if (!(Test-Path -LiteralPath $redistRoot)) { return $null }
    $runtime = Get-ChildItem -LiteralPath $redistRoot -Directory |
        Where-Object Name -Match '^\d+\.' | Sort-Object { [version]$_.Name } -Descending |
        Select-Object -First 1
    if (!$runtime) { return $null }
    $crt = Get-ChildItem -LiteralPath (Join-Path $runtime.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' |
        Select-Object -First 1
    if ($crt) { return $crt.FullName }
    return $null
}
