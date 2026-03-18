@echo off
setlocal
echo ============================================
echo   Mystic Onslaught -- Web Build
echo ============================================

:: ── Configuration ────────────────────────────────────────────────────────────
:: If you moved emsdk or raylib, update these two paths.
set EMSDK=C:\Users\rober\emsdk
set RAYLIB_SRC=C:\CLibraries\raylib-src\src
set SRC=TestGame
set OUT=web_build

:: ── Always run from the folder this bat lives in ─────────────────────────────
cd /d "%~dp0"

:: ── Activate Emscripten environment ──────────────────────────────────────────
echo Activating Emscripten...
call %EMSDK%\emsdk_env.bat
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Could not activate emsdk. Is it installed at %EMSDK%?
    pause & exit /b 1
)

:: ── Check raylib web library exists ──────────────────────────────────────────
if not exist "%RAYLIB_SRC%\libraylib.a" (
    echo ERROR: %RAYLIB_SRC%\libraylib.a not found.
    echo Run:  emmake mingw32-make PLATFORM=PLATFORM_WEB  inside %RAYLIB_SRC%
    pause & exit /b 1
)

:: ── Create output folder ──────────────────────────────────────────────────────
if not exist %OUT% mkdir %OUT%

:: ── Compile ───────────────────────────────────────────────────────────────────
echo Compiling... this will take a minute.
emcc -std=c++14 -Os -DPLATFORM_WEB ^
  %SRC%\main.cpp ^
  %SRC%\Engine.cpp ^
  %SRC%\BaseCharacter.cpp ^
  %SRC%\Character.cpp ^
  %SRC%\Enemy.cpp ^
  %SRC%\Cyclops.cpp ^
  %SRC%\Ogre.cpp ^
  %SRC%\Molarbeast.cpp ^
  %SRC%\Prop.cpp ^
  %SRC%\MainMenu.cpp ^
  %SRC%\PauseAndGameOver.cpp ^
  %SRC%\Leaderboard.cpp ^
  %SRC%\FireballProjectile.cpp ^
  %SRC%\SwordBeamProjectile.cpp ^
  %SRC%\FreezeProjectile.cpp ^
  %SRC%\CyclopsLaserProjectile.cpp ^
  %SRC%\LavaballProjectile.cpp ^
  %SRC%\FireBallPickup.cpp ^
  %SRC%\SwordBeamPickup.cpp ^
  %SRC%\FreezePickup.cpp ^
  %SRC%\HealPickup.cpp ^
  %SRC%\Game.cpp ^
  %SRC%\Player.cpp ^
  -I%SRC% -I%RAYLIB_SRC% -I%RAYLIB_SRC%\external -IC:\CLibraries\raylib-5.5_win64_msvc16\include ^
  %RAYLIB_SRC%\libraylib.a ^
  -s USE_GLFW=3 ^
  -s INITIAL_MEMORY=268435456 ^
  -s ALLOW_MEMORY_GROWTH=1 ^
  -s STACK_SIZE=33554432 ^
  --preload-file Hero ^
  --preload-file Enemy ^
  --preload-file Bosses ^
  --preload-file PowerUps ^
  --preload-file TileSet ^
  --preload-file UI ^
  --preload-file Sounds ^
  --preload-file Map.png ^
  --shell-file %RAYLIB_SRC%\shell.html ^
  -o %OUT%\index.html

:: ── Result ────────────────────────────────────────────────────────────────────
echo.
if %ERRORLEVEL%==0 (
    echo ============================================
    echo   BUILD SUCCESSFUL
    echo   Files are in: %OUT%\
    echo.
    echo   Zip the contents of %OUT%\ and upload
    echo   to itch.io as an HTML project.
    echo   Set embed size to 1920 x 1080.
    echo ============================================
) else (
    echo ============================================
    echo   BUILD FAILED -- check errors above
    echo ============================================
)

pause
