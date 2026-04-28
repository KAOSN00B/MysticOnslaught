@echo off
setlocal

set EMSDK=C:\Users\rober\emsdk
set RAYLIB_SRC=C:\CLibraries\raylib-src\src

echo Activating Emscripten...
call %EMSDK%\emsdk_env.bat
if %ERRORLEVEL% NEQ 0 ( echo ERROR: Failed to activate emsdk & pause & exit /b 1 )

cd /d "%RAYLIB_SRC%"
if %ERRORLEVEL% NEQ 0 ( echo ERROR: Could not cd to %RAYLIB_SRC% & pause & exit /b 1 )

del /q rcore.o rshapes.o rtextures.o rtext.o rmodels.o utils.o raudio.o libraylib.a 2>nul

echo Step 1: rcore.c
call emcc -c rcore.c -O1 -w -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -I. -Iexternal/glfw/include
if %ERRORLEVEL% NEQ 0 ( echo FAILED on rcore.c & pause & exit /b 1 )

echo Step 2: rshapes.c
call emcc -c rshapes.c -O1 -w -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -I. -Iexternal/glfw/include
if %ERRORLEVEL% NEQ 0 ( echo FAILED on rshapes.c & pause & exit /b 1 )

echo Step 3: rtextures.c
call emcc -c rtextures.c -O1 -w -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -I. -Iexternal/glfw/include
if %ERRORLEVEL% NEQ 0 ( echo FAILED on rtextures.c & pause & exit /b 1 )

echo Step 4: rtext.c
call emcc -c rtext.c -O1 -w -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -I. -Iexternal/glfw/include
if %ERRORLEVEL% NEQ 0 ( echo FAILED on rtext.c & pause & exit /b 1 )

echo Step 5: rmodels.c
call emcc -c rmodels.c -O1 -w -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -I. -Iexternal/glfw/include
if %ERRORLEVEL% NEQ 0 ( echo FAILED on rmodels.c & pause & exit /b 1 )

echo Step 6: utils.c
call emcc -c utils.c -O1 -w -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -I. -Iexternal/glfw/include
if %ERRORLEVEL% NEQ 0 ( echo FAILED on utils.c & pause & exit /b 1 )

echo Step 7: raudio.c
call emcc -c raudio.c -O1 -w -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -I. -Iexternal/glfw/include
if %ERRORLEVEL% NEQ 0 ( echo FAILED on raudio.c & pause & exit /b 1 )

echo Step 8: archiving
call emar rcs libraylib.a rcore.o rshapes.o rtextures.o rtext.o rmodels.o utils.o raudio.o
if %ERRORLEVEL% NEQ 0 ( echo FAILED archiving & pause & exit /b 1 )

echo.
echo DONE - libraylib.a is ready. Now run build_web.bat
pause
