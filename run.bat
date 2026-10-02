@echo off
setlocal
echo ==============================================================================
echo [VitalGrid] Iniciando Demonio Aeroespacial VitalGrid
echo ==============================================================================

if not exist vitalgrid_daemon.exe (
    echo vitalgrid_daemon.exe no encontrado. Ejecutando build.bat primero...
    call build.bat
    if %ERRORLEVEL% NEQ 0 exit /b 1
)

echo Abriendo Dashboard en el navegador predeterminado...
start "" "web\index.html"

echo Iniciando backend en ws://127.0.0.1:9002...
echo (Presiona Ctrl+C para detener el demonio)
vitalgrid_daemon.exe 9002
