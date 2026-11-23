@echo off
REM ═══════════════════════════════════════════════════════════════════════════
REM  Play_WizardEPIC.bat — Launch the game in standalone mode
REM  wizardEPIC «Арена стихий»
REM ═══════════════════════════════════════════════════════════════════════════

setlocal

set "PROJECT_DIR=%~dp0"
set "UPROJECT=%PROJECT_DIR%wizardEPIC.uproject"
set "UE_EDITOR="
for %%D in (
    "%USERPROFILE%\Epic Games\UE_5.8"
    "C:\Program Files\Epic Games\UE_5.8"
    "D:\Epic Games\UE_5.8"
) do (
    if exist "%%~D\Engine\Binaries\Win64\UnrealEditor.exe" (
        set "UE_EDITOR=%%~D\Engine\Binaries\Win64\UnrealEditor.exe"
        goto :found_ue
    )
)

echo [ERROR] Unreal Editor 5.8 not found.
echo         Checked %%USERPROFILE%%\Epic Games\UE_5.8 and C:\Program Files\Epic Games\UE_5.8.
pause
exit /b 1

:found_ue

if not exist "%UPROJECT%" (
    echo [ERROR] Project file not found: %UPROJECT%
    pause
    exit /b 1
)

echo.
echo ══════════════════════════════════════════════════
echo  Launching wizardEPIC ...
echo ══════════════════════════════════════════════════
echo  Project: %UPROJECT%
echo.
echo  Controls:
echo    1-4         Select spell (Fire / Earth / Wind / Lightning)
echo    Mouse wheel Cycle spells
echo    LMB         Cast (tap) / hold to charge
echo    Shift       Sprint
echo    RMB / Q     Dash
echo    Space       Jump
echo    R           Reset arena
echo    H           Toggle hints
echo    Tab         Toggle mouse capture
echo.

REM — Launch in standalone game mode ——————————————————————————————————
"%UE_EDITOR%" "%UPROJECT%" -game -windowed -resx=1920 -resy=1080 -log
