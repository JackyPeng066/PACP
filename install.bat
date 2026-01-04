@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

TITLE PACP Environment Installer

:: --- Check Admin Rights ---
echo Checking Administrator rights...
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo.
    echo [ERROR] Access Denied!
    echo Please right-click this file and select "Run as Administrator".
    echo.
    pause
    exit /b
)

set "INSTALL_DIR=C:\msys64"
set "MSYS2_URL=https://repo.msys2.org/distrib/x86_64/msys2-x86_64-latest.exe"
set "INSTALLER_NAME=msys2-installer.exe"

:: --- Step 1: Install MSYS2 ---
if exist "%INSTALL_DIR%" (
    echo [INFO] MSYS2 found. Skipping installation, moving to updates.
    goto :UPDATE_PHASE
)

echo.
echo [STEP 1/4] Downloading MSYS2...
curl -L -o %INSTALLER_NAME% "%MSYS2_URL%"

if not exist "%INSTALLER_NAME%" (
    echo [ERROR] Download failed. Check your internet connection.
    pause
    exit /b
)

echo.
echo [STEP 2/4] Installing MSYS2 (This may take a while)...
%INSTALLER_NAME% in --confirm-command --accept-messages --root %INSTALL_DIR%
if %errorLevel% neq 0 (
    echo [ERROR] Installation failed.
    pause
    exit /b
)
del %INSTALLER_NAME%

:UPDATE_PHASE
:: --- Step 2: Update and Install Tools ---
echo.
echo [STEP 3/4] Installing G++ and Make...

if not exist "%INSTALL_DIR%\usr\bin\bash.exe" (
    echo [ERROR] MSYS2 core files not found.
    pause
    exit /b
)

"%INSTALL_DIR%\usr\bin\bash.exe" -l -c "pacman -Syu --noconfirm"
"%INSTALL_DIR%\usr\bin\bash.exe" -l -c "pacman -S --needed --noconfirm base-devel mingw-w64-ucrt-x86_64-toolchain make"

:: --- Step 3: Set PATH ---
echo.
echo [STEP 4/4] Setting Environment Variables (PATH)...

set "BIN_UCRT=%INSTALL_DIR%\ucrt64\bin"
set "BIN_USR=%INSTALL_DIR%\usr\bin"

:: Simple PowerShell command to add PATH safely
powershell -Command "$p=[Environment]::GetEnvironmentVariable('Path','Machine'); if(-not $p.Contains('%BIN_UCRT%')){[Environment]::SetEnvironmentVariable('Path',$p+';%BIN_UCRT%','Machine');Write-Host '[ADDED] UCRT64'} else {Write-Host '[SKIP] UCRT64 exists'}; if(-not $p.Contains('%BIN_USR%')){[Environment]::SetEnvironmentVariable('Path',$p+';%BIN_USR%','Machine');Write-Host '[ADDED] USR/BIN'}"

:: --- Verify ---
echo.
echo ==================================================
echo      INSTALLATION COMPLETE!
echo ==================================================
echo.
echo Verifying versions...

set "PATH=%BIN_UCRT%;%BIN_USR%;%PATH%"

echo Checking G++...
g++ --version | findstr "g++"

echo Checking Make...
make --version | findstr "Make"

echo.
echo [IMPORTANT] Please CLOSE this window and restart VSCode/CMD.
echo Then you can run 'make' or './run.sh'.
echo.
pause