param(
    # Output folder for the generated web build.
    [string]$OutDir = "web",

    # Path to the raylib source/include folder compiled for Emscripten.
    # Example: C:\raylib\src
    [string]$RaylibInclude = $env:RAYLIB_WEB_INCLUDE,

    # Path to the folder containing the Emscripten-compatible libraylib.a.
    # Example: C:\raylib\src
    [string]$RaylibLib = $env:RAYLIB_WEB_LIB
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$gameRoot = Join-Path $projectRoot "TestGame"
$outPath = Join-Path $projectRoot $OutDir

# em++ should come from an activated Emscripten environment. This keeps the
# script simple and matches the normal Emscripten workflow.
$empp = Get-Command em++ -ErrorAction SilentlyContinue
if (-not $empp) {
    throw "em++ was not found. Run emsdk_env first, then rerun this script."
}

if ([string]::IsNullOrWhiteSpace($RaylibInclude) -or -not (Test-Path $RaylibInclude)) {
    throw "Set -RaylibInclude or RAYLIB_WEB_INCLUDE to your raylib Emscripten include/src folder."
}

if ([string]::IsNullOrWhiteSpace($RaylibLib) -or -not (Test-Path $RaylibLib)) {
    throw "Set -RaylibLib or RAYLIB_WEB_LIB to the folder that contains the Emscripten raylib library."
}

New-Item -ItemType Directory -Force -Path $outPath | Out-Null

# Keep the web source list explicit so it is easy to audit and stays aligned
# with the desktop project.
$sources = @(
    "BaseCharacter.cpp",
    "Character.cpp",
    "Cyclops.cpp",
    "CyclopsLaserProjectile.cpp",
    "Enemy.cpp",
    "Engine.cpp",
    "FireBallPickup.cpp",
    "FireballProjectile.cpp",
    "FreezePickup.cpp",
    "FreezeProjectile.cpp",
    "Game.cpp",
    "HealPickup.cpp",
    "LavaBallProjectile.cpp",
    "main.cpp",
    "MainMenu.cpp",
    "Molarbeast.cpp",
    "Ogre.cpp",
    "PauseAndGameOver.cpp",
    "Player.cpp",
    "Prop.cpp",
    "SwordBeamPickup.cpp",
    "SwordBeamProjectile.cpp"
) | ForEach-Object { Join-Path $gameRoot $_ }

# These mounts match the AssetPath() virtual paths used by the web build.
$preloadDirs = @("Hero", "Enemy", "PowerUps", "Bosses", "Sounds", "TileSet", "UI")

$args = @()
$args += $sources
$args += "-std=c++17"
$args += "-I", $gameRoot
$args += "-I", $RaylibInclude
$args += "-L", $RaylibLib
$args += "-lraylib"
$args += "-sUSE_GLFW=3"
$args += "-sALLOW_MEMORY_GROWTH=1"
$args += "-sASSERTIONS=1"
$args += "-sFORCE_FILESYSTEM=1"
$args += "--preload-file"
$args += (Join-Path $projectRoot "Hero") + "@/Hero"
$args += "--preload-file"
$args += (Join-Path $projectRoot "Enemy") + "@/Enemy"
$args += "--preload-file"
$args += (Join-Path $projectRoot "PowerUps") + "@/PowerUps"
$args += "--preload-file"
$args += (Join-Path $projectRoot "Bosses") + "@/Bosses"
$args += "--preload-file"
$args += (Join-Path $projectRoot "Sounds") + "@/Sounds"
$args += "--preload-file"
$args += (Join-Path $projectRoot "TileSet") + "@/TileSet"
$args += "--preload-file"
$args += (Join-Path $projectRoot "UI") + "@/UI"
$args += "-o"
$args += (Join-Path $outPath "index.html")

Push-Location $projectRoot
try {
    & $empp.Source @args
}
finally {
    Pop-Location
}
