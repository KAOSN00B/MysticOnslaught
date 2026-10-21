# Packages the Shipping build into a new, versioned, self-contained folder.
#
#   powershell -ExecutionPolicy Bypass -File tools\package_shipping.ps1 -Build   # build, then package (recommended)
#   powershell -ExecutionPolicy Bypass -File tools\package_shipping.ps1          # package the existing build
#
# Safety rules:
#   * Packages x64\Build-Shipping (the output of tools\build.ps1), never an older
#     hand-named candidate folder.
#   * Refuses a stale build: the source fingerprint stamped by tools\build.ps1
#     must match the current source (older builds without a stamp fall back to
#     the timestamp check).
#   * Never copies player data (profiles, run checkpoint and history, settings,
#     bindings, village saves, flow self-test output). Every file name the game
#     passes to UserDataPath(...) must be covered, or packaging stops.
#   * Each run creates a new folder; existing packages and builds are untouched.
param(
    [string]$BuildDirectory = 'x64\Build-Shipping',
    [switch]$Build,
    [string]$VsInstall,
    [string]$RaylibDir
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\MoToolchain.ps1"
$projectRoot = Split-Path $PSScriptRoot -Parent

if ($Build) {
    $buildArgs = @{ Configuration = 'Shipping' }
    if ($VsInstall) { $buildArgs.VsInstall = $VsInstall }
    if ($RaylibDir) { $buildArgs.RaylibDir = $RaylibDir }
    & (Join-Path $PSScriptRoot 'build.ps1') @buildArgs | Out-Host
}

$buildRoot = Join-Path $projectRoot $BuildDirectory
$executable = Join-Path $buildRoot 'TestGame.exe'
if (!(Test-Path -LiteralPath $executable)) { throw "No Shipping build at $executable. Run tools\build.ps1 (or pass -Build)." }

# Stale-build guard: the source this exe was built from must be today's source.
$exeTime = (Get-Item -LiteralPath $executable).LastWriteTimeUtc
$sourceFingerprint = Get-MoSourceFingerprint $projectRoot
$stampPath = Join-Path $buildRoot 'source.sha256'
if (Test-Path -LiteralPath $stampPath) {
    $builtFingerprint = (Get-Content -LiteralPath $stampPath -Raw).Trim()
    if ($builtFingerprint -ne $sourceFingerprint) {
        throw "Stale build: game source changed after $executable was built. Rebuild with tools\build.ps1 (or pass -Build)."
    }
} else {
    $newestSource = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'TestGame') -File |
        Where-Object { $_.Extension -in '.cpp', '.h', '.vcxproj' } |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($newestSource -and $newestSource.LastWriteTimeUtc -gt $exeTime) {
        throw "Stale build: $($newestSource.Name) changed after $executable was built. Rebuild with tools\build.ps1 (or pass -Build)."
    }
}

# Player data the game writes (UserDataPath), plus .tmp/.bak siblings from its
# atomic saves. Any new UserDataPath("<name>") must match, or packaging stops,
# so a future profile file cannot slip into a release unnoticed.
$personalPattern = '^Saves/|^(metaprogress|settings|keybindings|leaderboard|run_save|run_history)\.|^VillageLayoutSave|^flowtest_'
$personalDirs = @('shots', 'Saves')   # flow self-test screenshots; the save folder (SaveFiles.h)
$userDataNames = Get-ChildItem -Path (Join-Path $projectRoot 'TestGame\*') -Include '*.cpp','*.h' -File |
    ForEach-Object { [regex]::Matches((Get-Content -LiteralPath $_.FullName -Raw), 'UserDataPath\("([^"]+)"\)') } |
    ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
$uncovered = @($userDataNames | Where-Object { $_ -notmatch $personalPattern -and $personalDirs -notcontains $_ })
if ($uncovered) { throw "Package exclusions do not cover user data file(s): $($uncovered -join ', '). Add them to `$personalPattern." }

# Imported audio libraries ship only the clips the game names in quoted source
# paths (neither audio manifest is read at runtime). A path built at runtime
# (format string or folder prefix) cannot be resolved here, so packaging stops
# rather than silently pruning its clips; so does a path with no file on disk.
$referenced = @{}
Get-ChildItem -Path (Join-Path $projectRoot 'TestGame\*') -Include '*.cpp','*.h' -File | ForEach-Object {
    $text = Get-Content -LiteralPath $_.FullName -Raw
    foreach ($m in [regex]::Matches($text, '"((Sounds|Music)/MysticExpansion/[^"\r\n]*)"')) {
        $clip = $m.Groups[1].Value
        if ($clip -match '%' -or -not [IO.Path]::HasExtension($clip)) {
            throw "Built audio path in $($_.Name): `"$clip`". Name each clip in full so packaging keeps it."
        }
        if (!(Test-Path -LiteralPath (Join-Path $projectRoot $clip))) { throw "Audio named in $($_.Name) is missing on disk: $clip" }
        $referenced[$clip.Replace('/', '\')] = $true
    }
}

$vs = Resolve-MoVisualStudio $VsInstall
$crt = Find-MoCrtRedist $vs
if (!$crt) { throw 'MSVC runtime (VC\Redist\MSVC\<ver>\x64\Microsoft.VC*.CRT) not found under the Visual Studio install.' }

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$packageRoot = Join-Path $projectRoot ("x64\Packages\MysticOnslaught-$stamp")
New-Item -ItemType Directory -Path $packageRoot | Out-Null
Copy-Item -LiteralPath $executable -Destination (Join-Path $packageRoot 'MysticOnslaught.exe')

foreach ($folder in @('Hero','Enemy','Bosses','TileSet','MapTilesets','UI','Sounds','Music','PowerUps','VillageAssets')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $folder) -Destination $packageRoot -Recurse
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'TestGame\Rooms') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'Map.png') -Destination $packageRoot
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $projectRoot 'TestGame') -File -Filter '*.txt') {
    if ($file.Name -match '^(attacktuning_|charactertuning_|tilemapper_|biomevariants_|nineslice_)') {
        Copy-Item -LiteralPath $file.FullName -Destination $packageRoot
    }
}
Get-ChildItem -LiteralPath $crt -Filter '*.dll' -File | Copy-Item -Destination $packageRoot

# Unreferenced imported clips are raw pack files kept in the repo for future
# use, not part of the product (keeps the package lean; see the scan above).
foreach ($library in @('Sounds\MysticExpansion', 'Music\MysticExpansion')) {
    $libraryRoot = Join-Path $packageRoot $library
    if (Test-Path -LiteralPath $libraryRoot) {
        Get-ChildItem -LiteralPath $libraryRoot -File -Recurse |
            Where-Object { -not $referenced.ContainsKey($_.FullName.Substring($packageRoot.Length + 1)) } |
            ForEach-Object { Remove-Item -LiteralPath $_.FullName }
    }
}

# Player/developer data that may sit inside copied asset folders never ships.
$personal = Get-ChildItem -LiteralPath $packageRoot -File -Recurse |
    Where-Object { $_.Name -match $personalPattern -or $_.Extension -in '.tmp', '.bak' }
foreach ($f in $personal) { Remove-Item -LiteralPath $f.FullName }
Get-ChildItem -LiteralPath $packageRoot -Directory -Recurse |
    Where-Object { $personalDirs -contains $_.Name } |
    ForEach-Object { Remove-Item -LiteralPath $_.FullName -Recurse }

# Version stamp so a package can always be traced to its source state.
$commit = (& git -C $projectRoot rev-parse --short HEAD 2>$null)
$dirty  = (& git -C $projectRoot status --porcelain 2>$null | Measure-Object).Count -gt 0
$version = [ordered]@{
    product            = 'Mystic Onslaught'
    configuration      = 'Shipping (full game)'
    packaged           = (Get-Date).ToString('s')
    exeBuiltUtc        = $exeTime.ToString('s')
    gitCommit          = $commit
    uncommittedChanges = $dirty
    sourceFingerprint  = $sourceFingerprint   # tools\MoToolchain.ps1 Get-MoSourceFingerprint
    msvcRuntime        = (Split-Path $crt -Leaf)
}
$version | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $packageRoot 'version.json') -Encoding UTF8

@'
Mystic Onslaught - release candidate build
Run MysticOnslaught.exe. Keep the asset folders beside it.
Progress, the village, settings and key bindings are stored in
%LOCALAPPDATA%\MysticOnslaught (never in this folder).
See version.json for the exact build this package came from.
'@ | Set-Content -LiteralPath (Join-Path $packageRoot 'README.txt') -Encoding UTF8

$files = Get-ChildItem -LiteralPath $packageRoot -File -Recurse
$manifest = foreach ($file in $files) {
    [pscustomobject]@{ Path=$file.FullName.Substring($packageRoot.Length+1); Bytes=$file.Length; SHA256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
}
$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $packageRoot 'manifest.json') -Encoding UTF8
Write-Output $packageRoot
