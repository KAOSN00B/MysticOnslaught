@echo off
setlocal
echo ============================================
echo   Mystic Onslaught -- Web Build
echo ============================================

set EMSDK=C:\Users\rober\emsdk
set RAYLIB_SRC=C:\CLibraries\raylib-src\src
set SRC=TestGame
set OUT=web_build

cd /d "%~dp0"

if not exist "%RAYLIB_SRC%\libraylib.a" (
    echo ERROR: %RAYLIB_SRC%\libraylib.a not found.
    echo Run build_raylib_web.bat first to compile raylib for the web.
    pause
    exit /b 1
)

echo Activating Emscripten...
call %EMSDK%\emsdk_env.bat
if %ERRORLEVEL% NEQ 0 ( echo ERROR: Failed to activate emsdk & pause & exit /b 1 )

if not exist %OUT% mkdir %OUT%

echo Compiling... this will take a minute.

call emcc ^
  %SRC%\main.cpp ^
  %SRC%\Engine.cpp ^
  %SRC%\BaseCharacter.cpp ^
  %SRC%\Character.cpp ^
  %SRC%\Enemy.cpp ^
  %SRC%\Cyclops.cpp ^
  %SRC%\Ogre.cpp ^
  %SRC%\Molarbeast.cpp ^
  %SRC%\Prop.cpp ^
  %SRC%\Game.cpp ^
  %SRC%\MainMenu.cpp ^
  %SRC%\PauseAndGameOver.cpp ^
  %SRC%\Leaderboard.cpp ^
  %SRC%\FireballProjectile.cpp ^
  %SRC%\SwordBeamProjectile.cpp ^
  %SRC%\FreezeProjectile.cpp ^
  %SRC%\CyclopsLaserProjectile.cpp ^
  %SRC%\LavaballProjectile.cpp ^
  %SRC%\SpreadProjectile.cpp ^
  %SRC%\HealPickup.cpp ^
  %SRC%\GoldPickup.cpp ^
  %SRC%\ManaGemPickup.cpp ^
  %SRC%\AudioManager.cpp ^
  %SRC%\CombatDirector.cpp ^
  %SRC%\DebugPanel.cpp ^
  %SRC%\HUDRenderer.cpp ^
  %SRC%\MapDirector.cpp ^
  %SRC%\NavigationGrid.cpp ^
  %SRC%\OverlayRenderer.cpp ^
  %SRC%\ProjectileSystem.cpp ^
  %SRC%\RoomDirector.cpp ^
  %SRC%\RunStateController.cpp ^
  %SRC%\ShopManager.cpp ^
  %SRC%\TouchControls.cpp ^
  %SRC%\VFXManager.cpp ^
  %SRC%\WorldConfig.cpp ^
  -o %OUT%\index.html ^
  -std=c++17 -Os -DPLATFORM_WEB ^
  -I%SRC% ^
  -I%RAYLIB_SRC% ^
  -I%RAYLIB_SRC%\external ^
  %RAYLIB_SRC%\libraylib.a ^
  -sUSE_GLFW=3 ^
  -sALLOW_MEMORY_GROWTH=1 ^
  -sFORCE_FILESYSTEM=1 ^
  --shell-file shell.html ^
  --preload-file Hero ^
  --preload-file Enemy ^
  --preload-file Bosses ^
  --preload-file ForestLevel ^
  --preload-file TileSet ^
  --preload-file UI ^
  --preload-file Sounds ^
  --preload-file Music ^
  --preload-file PowerUps ^
  --preload-file Map.png

if %ERRORLEVEL% NEQ 0 ( echo. & echo BUILD FAILED & pause & exit /b 1 )

echo.
echo SUCCESS - web_build\ is ready.
echo Zip the contents of web_build\ and upload to itch.io ^(1920x1080^).
pause
