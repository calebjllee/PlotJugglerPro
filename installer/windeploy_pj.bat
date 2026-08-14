@echo off
REM Deploy Qt dependencies for PlotJugglerPro.
REM This script runs windeployqt on the main executable. The installer payload
REM already contains PlotJugglerPro plugin DLLs copied from the Release output.
REM
REM Usage: windeploy_pj.bat [path_to_windeployqt.exe]
REM   If windeployqt.exe is not in PATH, provide the full path as argument
REM   Example: windeploy_pj.bat C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe

setlocal enabledelayedexpansion

set DATA_DIR=%~dp0io.plotjuggler.application\data

REM Use provided windeployqt path or default to PATH
if "%~1"=="" (
    set WINDEPLOYQT=windeployqt.exe
) else (
    set WINDEPLOYQT=%~1
)

echo Deploying Qt dependencies for PlotJugglerPro...
echo Using: %WINDEPLOYQT%

REM Deploy for main executable
echo Processing: PlotJugglerPro.exe
"%WINDEPLOYQT%" --release --webengine "%DATA_DIR%\PlotJugglerPro.exe"
if errorlevel 1 exit /b %errorlevel%

echo Done.
