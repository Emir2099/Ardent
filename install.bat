@echo off
setlocal enabledelayedexpansion
set PREFIX=%ProgramFiles%\Ardent
if not "%1"=="" set PREFIX=%1
set BIN_DIR=%PREFIX%\bin
set SCROLL_DIR=%PREFIX%\lib\ardent\scrolls
set EX_DIR=%PREFIX%\share\ardent\examples

echo Installing Ardent to %PREFIX%
mkdir "%BIN_DIR%" 2>nul
mkdir "%SCROLL_DIR%" 2>nul
mkdir "%EX_DIR%" 2>nul

if not exist ardent_vm.exe (
  echo Error: ardent_vm.exe not found. Build first.
  exit /b 1
)
copy /Y ardent_vm.exe "%BIN_DIR%\ardent.exe" >nul
copy /Y ardent_vm.exe "%BIN_DIR%\ardentc.exe" >nul
copy /Y ardent_vm.exe "%BIN_DIR%\oracle.exe" >nul
copy /Y examples\*.ardent "%EX_DIR%" >nul 2>nul

:: Set ARDENT_HOME for current user
setx ARDENT_HOME "%PREFIX%\lib\ardent" >nul

echo "The Scholar's Ink now flows through your system."
echo Ardent installed at: %PREFIX%
echo Add to PATH manually if needed: setx PATH "%BIN_DIR%;%PATH%"
echo Standard scrolls root: %ARDENT_HOME%\scrolls
echo Try: ardent --demo   ^| ardent --scrolls
endlocal
