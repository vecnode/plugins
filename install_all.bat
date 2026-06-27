@echo off
REM ============================================================================
REM  install_all.bat - install every built bundle into the user VST3/CLAP folder
REM
REM  Usage:  install_all.bat [Format]
REM            Format : vst3 (default) | clap
REM
REM  Installs <Plugin>\build\out\<Plugin>.<Format> into:
REM            %LOCALAPPDATA%\Programs\Common\VST3   (or \CLAP)
REM  If a DAW has the plugin loaded the bundle is staged to *.pending; close the
REM  DAW and re-run this script to finish.  Run build_all.bat first.
REM ============================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "FORMAT=%~1"
if "%FORMAT%"=="" set "FORMAT=vst3"

set "PS=powershell -NoProfile -ExecutionPolicy Bypass -File"
set "INSTALLED="
set "SKIPPED="
set "FAILED="

echo Installing all %FORMAT% plugins
echo.

for /d %%D in (*) do (
  if exist "%%D\config.h" (
    if exist "%%D\build\out\%%D.%FORMAT%" (
      echo === Installing %%D ===
      %PS% "scripts\install-plugin.ps1" -Plugin "%%D" -Format %FORMAT%
      if errorlevel 1 (
        set "FAILED=!FAILED! %%D"
      ) else (
        set "INSTALLED=!INSTALLED! %%D"
      )
    ) else (
      set "SKIPPED=!SKIPPED! %%D"
      echo === Skipping %%D ^(no built %FORMAT% bundle - run build_all.bat first^) ===
    )
    echo.
  )
)

echo ============================================
echo Installed:!INSTALLED!
if defined SKIPPED echo Skipped:!SKIPPED!
if defined FAILED (
  echo Locked/failed:!FAILED!
  echo Close your DAW ^(remove the plugin from any track^), then re-run install_all.bat.
  exit /b 1
)
echo Done.
exit /b 0
