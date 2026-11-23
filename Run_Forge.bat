@echo off
REM ═══════════════════════════════════════════════════════════════════════════
REM  Run_Forge.bat — Generate assets + import into UE editor
REM  wizardEPIC Content Forge Pipeline
REM ═══════════════════════════════════════════════════════════════════════════

setlocal

set "PROJECT_DIR=%~dp0"
set "UPROJECT=%PROJECT_DIR%wizardEPIC.uproject"
set "FORGE_DIR=%PROJECT_DIR%Scripts\forge"
set "UE_EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

REM — Find Python (uv-managed or system) ——————————————————————————————
set "PYTHON="
for %%P in (
    "%USERPROFILE%\AppData\Roaming\uv\python\cpython-3.11.16-windows-x86_64-none\python.exe"
    "%USERPROFILE%\AppData\Roaming\uv\python\cpython-3.14.7-windows-x86_64-none\python.exe"
) do (
    if exist %%P (
        set "PYTHON=%%~P"
        goto :found_python
    )
)
where python >nul 2>&1 && set "PYTHON=python" && goto :found_python
echo [ERROR] Python not found. Install via: uv python install 3.11
exit /b 1

:found_python
echo.
echo ══════════════════════════════════════════════════
echo  wizardEPIC Forge Pipeline
echo ══════════════════════════════════════════════════
echo  Python:  %PYTHON%
echo  Project: %UPROJECT%
echo.

REM ── Step 1: Offline asset generation ────────────────────────────────────
echo [1/2] Running offline_forge.py ...
"%PYTHON%" "%FORGE_DIR%\offline_forge.py" --out "%FORGE_DIR%\Generated"
if errorlevel 1 (
    echo [ERROR] offline_forge.py failed!
    pause
    exit /b 1
)
echo.

REM ── Step 2: UE editor import ────────────────────────────────────────────
REM Check if UnrealBuildTool is running (C++ build in progress)
tasklist /FI "IMAGENAME eq UnrealBuildTool.exe" 2>nul | find /i "UnrealBuildTool.exe" >nul
if not errorlevel 1 (
    echo [WARNING] UnrealBuildTool is running! Waiting for C++ build to finish...
    echo          Press Ctrl+C to skip the UE import step.
    :wait_loop
    timeout /t 5 /nobreak >nul
    tasklist /FI "IMAGENAME eq UnrealBuildTool.exe" 2>nul | find /i "UnrealBuildTool.exe" >nul
    if not errorlevel 1 goto :wait_loop
    echo [OK] Build finished, proceeding...
)

if not exist "%UE_EDITOR%" (
    echo [WARNING] UE editor not found at: %UE_EDITOR%
    echo          Skipping UE import. Run manually from the editor.
    goto :done
)

echo [2/2] Running ue_forge.py in UnrealEditor-Cmd ...
"%UE_EDITOR%" "%UPROJECT%" -ExecutePythonScript="%FORGE_DIR%\ue_forge.py" -unattended -nosplash -nullrhi
if errorlevel 1 (
    echo [WARNING] ue_forge.py reported errors. Check the log above.
)

:done
echo.
echo ══════════════════════════════════════════════════
echo  Forge complete!
echo ══════════════════════════════════════════════════
pause
