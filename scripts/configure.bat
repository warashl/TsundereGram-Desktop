@echo off
rem Configures the TsundereGram Desktop build tree once and reuses it later:
rem re-running cmake rewrites the projects and forces a full rebuild.
cd /d "%TBUILD%\%REPO_NAME%\Telegram" || exit /b 1
set "CFG_FLAGS=tsg-v1 libs=%CACHE_KEY%"
set "CFG_MARKER=..\out\.ci_configure_flags"
set "REUSE_TREE=0"
if exist ..\out\CMakeCache.txt if exist "%CFG_MARKER%" (
  for /f "usebackq delims=" %%a in ("%CFG_MARKER%") do (
    if "%%a"=="%CFG_FLAGS%" set "REUSE_TREE=1"
  )
)
if "%REUSE_TREE%"=="1" (
  echo Reusing the configured build tree.
  exit /b 0
)
echo Configuring the build tree.
call configure.bat x64 ^
  -D TDESKTOP_API_ID=2040 ^
  -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627 ^
  -D CMAKE_CONFIGURATION_TYPES=Release ^
  -D DESKTOP_APP_DISABLE_AUTOUPDATE=ON ^
  -D DESKTOP_APP_DISABLE_CRASH_REPORTS=ON ^
  -D CMAKE_MSVC_DEBUG_INFORMATION_FORMAT= || exit /b 1
> "%CFG_MARKER%" echo %CFG_FLAGS%
exit /b 0
