@echo off
setlocal
cd /d "%~dp0"
set INTERNAL_STORAGE=%~dp0internal_storage

echo =======================================================
echo Snowsky Echo Mini - Native C Firmware Simulator
echo Photorealistic Borderless Chassis & Standalone eMMC
echo =======================================================
echo Controls:
echo   Mouse Drag     : Click and drag anywhere on casing to move window
echo   Edge Buttons   : Tactile buttons attached to rotating chassis edges
echo   Rotate CW      : Click [ROTATE] button or press 'R' key (rotates buttons too!)
echo   Open Storage   : Click [FILES] button or press 'F' key to open Windows Explorer
echo   Power Off / On : Press and hold [POWER] or 'P' key to power off (standby)
echo                    Press and hold [POWER] or 'P' again in standby to turn on!
echo   Close Window   : Click [X] in the top utility bar
echo.
echo Keyboard Shortcuts:
echo   Up / Down      : Navigate Menu / Volume
echo   Enter / Space  : Play / Pause / Select
echo   Escape / Back  : Back / Return
echo   M / Tab        : Menu / Options
echo   R              : Rotate 90 deg clockwise
echo   F              : Open internal storage in Explorer
echo   P              : Tap = Backlight sleep, Hold = Power Off / Power On
echo =======================================================
echo.

build\sim\platform\sim\echo_sim.exe --scale 2 --internal "%INTERNAL_STORAGE%" --system-dir "%INTERNAL_STORAGE%\.echo"
if errorlevel 1 pause