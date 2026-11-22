@echo off
rem Copies autoladd.vst3 (sitting next to this script) into the system VST3 folder FL Studio scans.
rem Right-click -> "Run as administrator" if the copy is denied.
set "DEST=%CommonProgramFiles%\VST3"
if not exist "%DEST%" mkdir "%DEST%"
xcopy /E /I /Y "%~dp0autoladd.vst3" "%DEST%\autoladd.vst3"
if errorlevel 1 (
  echo.
  echo Copy failed - run this file as administrator.
) else (
  echo.
  echo Installed to %DEST%\autoladd.vst3
  echo In FL Studio: Options ^> Manage plugins ^> Find plugins, then add "autoladd" from the plugin picker.
)
pause
