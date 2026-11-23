@echo off
setlocal
set "PROJECT_DIR=%~dp0"
set "MAP_FILE=%PROJECT_DIR%Content\Wizard\Levels\L_Arena.umap"
set "UE_EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "UE_CMD=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "UPROJECT=%PROJECT_DIR%wizardEPIC.uproject"
set "FORGE_DIR=%PROJECT_DIR%Scripts\forge"

if not exist "%MAP_FILE%" (
    echo [wizardEPIC] First launch - assembling DataAssets and L_Arena.umap ...
    "%UE_CMD%" "%UPROJECT%" -ExecutePythonScript="%FORGE_DIR%\ue_forge.py" -unattended -nosplash -nullrhi -stdout
)

echo [wizardEPIC] Starting game...
start "" "%UE_EDITOR%" "%UPROJECT%" -game -windowed -resx=1920 -resy=1080
