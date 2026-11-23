@echo off
REM ═══════════════════════════════════════════════════════════════════════════
REM  Play_WizardEPIC.bat — Launch the game in standalone mode
REM  wizardEPIC «Арена стихий»
REM ═══════════════════════════════════════════════════════════════════════════

setlocal

set "PROJECT_DIR=%~dp0"
set "UPROJECT=%PROJECT_DIR%wizardEPIC.uproject"
set "UE_EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

if not exist "%UE_EDITOR%" (
    echo [ERROR] Unreal Editor not found at:
    echo         %UE_EDITOR%
    echo.
    echo Make sure UE 5.8 is installed at the default location.
    pause
    exit /b 1
)

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
