@echo off
REM ============================================================================
REM  build_all.bat - build the audioagent library and every plugin in this repo
REM
REM  Usage:  build_all.bat [Format] [Config]
REM            Format : vst3 (default) | vst2 | clap | aax | app | all
REM            Config : Release (default) | Debug | Tracer
REM
REM  A "plugin" is any top-level folder containing config.h + CMakeLists.txt.
REM  audioagent is a header-only INTERFACE library, so it is compiled into each
REM  plugin rather than built on its own.
REM ============================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "FORMAT=%~1"
if "%FORMAT%"=="" set "FORMAT=vst3"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Release"

set "PS=powershell -NoProfile -ExecutionPolicy Bypass -File"
set "BUILT="
set "FAILED="
set "FOUND="

echo Building all plugins  [Format=%FORMAT%  Config=%CONFIG%]
echo.

for /d %%D in (*) do (
  if exist "%%D\config.h" if exist "%%D\CMakeLists.txt" (
    set "FOUND=1"
    echo === Building %%D ===
    %PS% "scripts\build.ps1" -Plugin "%%D" -Format %FORMAT% -Config %CONFIG%
    if errorlevel 1 (
      set "FAILED=!FAILED! %%D"
    ) else (
      set "BUILT=!BUILT! %%D"
    )
    echo.
  )
)

if not defined FOUND (
  echo No plugins found ^(no top-level folder contains config.h^).
  exit /b 1
)

echo ============================================
echo Built: !BUILT!
if defined FAILED (
  echo Failed:!FAILED!
  exit /b 1
)
echo All plugins built. Install with:  install_all.bat %FORMAT%
exit /b 0
