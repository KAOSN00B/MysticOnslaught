@echo off
call B:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat >nul
if not exist tools\biome_content\build mkdir tools\biome_content\build
cl /nologo /std:c++17 /EHsc /MD /I C:\CLibraries\raylib-5.5_win64_msvc16\include /I TestGame /Fo:tools\biome_content\build\ /Fe:tools\biome_content\build\proof_runtime.exe TestGame\GeneratedVariantValidationTests.cpp TestGame\RoomLibrary.cpp TestGame\DungeonGen.cpp TestGame\TileRenderer.cpp TestGame\RoomBlueprint.cpp TestGame\RoomLayout.cpp TestGame\RoomCapacity.cpp TestGame\RoomCollision.cpp TestGame\TileDefs.cpp TestGame\RoomAssetCatalog.cpp /link /LIBPATH:C:\CLibraries\raylib-5.5_win64_msvc16\lib raylib.lib winmm.lib gdi32.lib opengl32.lib user32.lib shell32.lib
if errorlevel 1 exit /b %errorlevel%
tools\biome_content\build\proof_runtime.exe %* > docs\biome-content-audit\proof\runtime-validation.txt
if errorlevel 1 exit /b %errorlevel%
findstr /C:"PASS" /C:"WARNING" /C:"ERROR" docs\biome-content-audit\proof\runtime-validation.txt

