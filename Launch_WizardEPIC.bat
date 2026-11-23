@echo off
setlocal
set "PROJECT_DIR=%~dp0"
set "MAP_FILE=%PROJECT_DIR%Content\Wizard\Levels\L_Arena.umap"
set "UPROJECT=%PROJECT_DIR%wizardEPIC.uproject"
set "FORGE_DIR=%PROJECT_DIR%Scripts\forge"

set "UE_ROOT="
for %%D in (
    "%USERPROFILE%\Epic Games\UE_5.8"
    "C:\Program Files\Epic Games\UE_5.8"
    "D:\Epic Games\UE_5.8"
) do (
    if exist "%%~D\Engine\Binaries\Win64\UnrealEditor.exe" (
        set "UE_ROOT=%%~D"
        goto :found_ue
    )
)
echo [ERROR] Unreal Engine 5.8 not found in %%USERPROFILE%%\Epic Games\UE_5.8 or C:\Program Files\Epic Games\UE_5.8.
pause
exit /b 1

:found_ue
set "UE_EDITOR=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
set "UE_CMD=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if not exist "%MAP_FILE%" (
    echo [wizardEPIC] First launch - assembling DataAssets and L_Arena.umap ...
    if not exist "%FORGE_DIR%\Generated\layout.json" (
        where python >nul 2>&1 && python "%FORGE_DIR%\offline_forge.py" --out "%FORGE_DIR%\Generated"
    )
    "%UE_CMD%" "%UPROJECT%" -ExecutePythonScript="%FORGE_DIR%\ue_forge.py" -unattended -nop4 -nosplash -NullRHI -stdout
    if errorlevel 1 (
        echo [ERROR] Forge assembly failed!
        pause
        exit /b 1
    )
)

echo [wizardEPIC] Starting game...
start "" "%UE_EDITOR%" "%UPROJECT%" -game -windowed -resx=1920 -resy=1080
