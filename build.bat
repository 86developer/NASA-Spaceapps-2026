@echo off
setlocal
echo ==============================================================================
echo [VitalGrid] Compilando Backend C++ (Flight Surgeon AI + SOMA Watchdog)
echo ==============================================================================

where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if exist "%USERPROFILE%\scoop\apps\gcc\current\bin\g++.exe" (
        set "PATH=%USERPROFILE%\scoop\apps\gcc\current\bin;%PATH%"
    ) else (
        echo [ERROR] No se encontro g++. Por favor instala MinGW o GCC.
        exit /b 1
    )
)

echo Compilando con g++ (C++17, -O3, Winsock2)...
g++ -std=c++17 -O3 -Wall -Wextra src/main.cpp src/engine/flight_surgeon.cpp src/soma/soma_watchdog.cpp src/network/websocket_server.cpp -lws2_32 -o vitalgrid_daemon.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [EXITO] Compilacion completada con exito: vitalgrid_daemon.exe
    echo Ejecuta run.bat para iniciar el demonio.
) else (
    echo.
    echo [ERROR] Fallo la compilacion.
    exit /b 1
)
