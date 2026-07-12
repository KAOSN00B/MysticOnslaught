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
  %SRC%\SkeletonArcher.cpp ^
  %SRC%\FlameWisp.cpp ^
  %SRC%\SlimeEnemy.cpp ^
  %SRC%\AbyssSlime.cpp ^
  %SRC%\PumpkinJack.cpp ^
  %SRC%\Minotaur.cpp ^
  %SRC%\Sporeling.cpp ^
  %SRC%\Shieldbearer.cpp ^
  %SRC%\Phantom.cpp ^
  %SRC%\BomberImp.cpp ^
  %SRC%\Warchief.cpp ^
  %SRC%\LivingBlade.cpp ^
  %SRC%\Werewolf.cpp ^
  %SRC%\ChompBug.cpp ^
  %SRC%\Osiris.cpp ^
  %SRC%\TitanGuard.cpp ^
  %SRC%\ToxicVermin.cpp ^
  %SRC%\AncientBear.cpp ^
  %SRC%\PlayerPreview.cpp ^
  %SRC%\RoomHazardDirector.cpp ^
  %SRC%\EnemyProjectile.cpp ^
  %SRC%\Prop.cpp ^
  %SRC%\Game.cpp ^
  %SRC%\MainMenu.cpp ^
  %SRC%\PauseAndGameOver.cpp ^
  %SRC%\Leaderboard.cpp ^
  %SRC%\CyclopsLaserProjectile.cpp ^
  %SRC%\LavaballProjectile.cpp ^
  %SRC%\SpreadProjectile.cpp ^
  %SRC%\HealPickup.cpp ^
  %SRC%\GoldPickup.cpp ^
  %SRC%\CellPickup.cpp ^
  %SRC%\MetaProgression.cpp ^
  %SRC%\Relic.cpp ^
  %SRC%\PlayerClass.cpp ^
  %SRC%\CharacterTuning.cpp ^
  %SRC%\CharacterAnimator.cpp ^
  %SRC%\AudioManager.cpp ^
  %SRC%\CombatDirector.cpp ^
  %SRC%\DebugPanel.cpp ^
  %SRC%\HUDRenderer.cpp ^
  %SRC%\MapDirector.cpp ^
  %SRC%\WorldMapManager.cpp ^
  %SRC%\SettingsManager.cpp ^
  %SRC%\NavigationGrid.cpp ^
  %SRC%\OverlayRenderer.cpp ^
  %SRC%\ProjectileSystem.cpp ^
  %SRC%\RoomDirector.cpp ^
  %SRC%\RunStateController.cpp ^
  %SRC%\ShopManager.cpp ^
  %SRC%\TouchControls.cpp ^
  %SRC%\VFXManager.cpp ^
  %SRC%\WorldConfig.cpp ^
  %SRC%\DungeonGen.cpp ^
  %SRC%\RoomLayout.cpp ^
  %SRC%\TileDefs.cpp ^
  %SRC%\TileRenderer.cpp ^
  %SRC%\TileMapper.cpp ^
  %SRC%\NineSliceEditor.cpp ^
  %SRC%\CutsceneManager.cpp ^
  %SRC%\DialogueBox.cpp ^
  %SRC%\GamepadInput.cpp ^
  -o %OUT%\index.html ^
  -std=c++17 -Os -DPLATFORM_WEB ^
  -I%SRC% ^
  -I%RAYLIB_SRC% ^
  -I%RAYLIB_SRC%\external ^
  %RAYLIB_SRC%\libraylib.a ^
  -sUSE_GLFW=3 ^
  -sINITIAL_MEMORY=268435456 ^
  -sFORCE_FILESYSTEM=1 ^
  --shell-file shell.html ^
  --post-js audio_resume.js ^
  --preload-file Hero ^
  --preload-file Enemy ^
  --preload-file Bosses ^
  --preload-file TileSet ^
  --preload-file MapTilesets ^
  --preload-file UI ^
  --preload-file Sounds ^
  --preload-file Music ^
  --preload-file PowerUps ^
  --preload-file Map.png ^
  "--preload-file" "TestGame/tilemapper_Ancient Castle.txt@tilemapper_Ancient Castle.txt" ^
  "--preload-file" "TestGame/tilemapper_Caverns.txt@tilemapper_Caverns.txt" ^
  "--preload-file" "TestGame/tilemapper_Demons Insides.txt@tilemapper_Demons Insides.txt" ^
  "--preload-file" "TestGame/tilemapper_Dream Realm.txt@tilemapper_Dream Realm.txt" ^
  "--preload-file" "TestGame/tilemapper_Forest.txt@tilemapper_Forest.txt" ^
  "--preload-file" "TestGame/tilemapper_Graveyard.txt@tilemapper_Graveyard.txt" ^
  "--preload-file" "TestGame/tilemapper_Jungle.txt@tilemapper_Jungle.txt" ^
  "--preload-file" "TestGame/tilemapper_Lost City.txt@tilemapper_Lost City.txt" ^
  "--preload-file" "TestGame/tilemapper_The Sanctuary.txt@tilemapper_The Sanctuary.txt" ^
  "--preload-file" "TestGame/tilemapper_Wastelands.txt@tilemapper_Wastelands.txt"

if %ERRORLEVEL% NEQ 0 ( echo. & echo BUILD FAILED & pause & exit /b 1 )

echo.
echo SUCCESS - web_build\ is ready.
echo Zip the contents of web_build\ and upload to itch.io ^(1920x1080^).
pause
