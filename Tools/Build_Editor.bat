@echo off
rem Builds the WizardEPIC editor target (Win64 Development).
rem MaxParallelActions is capped because the machine has 8 GB of RAM.
setlocal
set UE_ROOT=C:\Program Files\Epic Games\UE_5.8
set PROJECT=%~dp0..\wizardEPIC.uproject
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" WizardEPICEditor Win64 Development -Project="%PROJECT%" -WaitMutex -FromMsBuild -MaxParallelActions=4 -ForceUnity
exit /b %ERRORLEVEL%
