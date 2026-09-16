@echo off
setlocal
title PowerScreen - Tester Ubuntu

rem Lanzador del emulador PowerScreen mediante Ubuntu/WSL.
rem Usa la compilacion aislada y no toca build/bin.
set "PROJECT_DIR=/mnt/d/DEV-Projects/K1C PowerUpgrades/PowerScreen"
set "BUILD_PATH=./.tmp-build-final/compilacion-wsl/bin/powerscreen"

echo Iniciando PowerScreen en Ubuntu/WSL...
echo Cierra la ventana SDL2 manualmente cuando termines.
wsl.exe -d Ubuntu -- bash -lc "cd '%PROJECT_DIR%' && SDL_VIDEODRIVER=x11 exec %BUILD_PATH%"
set "EXIT_CODE=%ERRORLEVEL%"

if not "%EXIT_CODE%"=="0" (
    echo.
    echo El emulador se cerro con el codigo %EXIT_CODE%.
    pause
)

exit /b %EXIT_CODE%
