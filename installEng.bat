@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

:: Define Colors for Output
set "RESET= [0m"
set "BOLD= [1m"
set "RED= [31m"
set "GREEN= [32m"
set "YELLOW= [33m"
set "CYAN= [36m"

TITLE PACP Environment Installer (English Version)

:: ============================================================================
:: 1. Check Administrator Privileges
:: ============================================================================
echo Checking Administrator rights...
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo.
    echo %RED%[ERROR] Access Denied!%RESET%
    echo Please right-click this file and select "Run as Administrator".
    echo.
    pause
    exit /b
)

set "INSTALL_DIR=C:\msys64"
set "MSYS2_URL=https://repo.msys2.org/distrib/x86_64/msys2-x86_64-latest.exe"
set "INSTALLER_NAME=msys2-installer.exe"

:: ============================================================================
:: 2. Install MSYS2
:: ============================================================================
if exist "%INSTALL_DIR%" (
    echo %YELLOW%[INFO] MSYS2 detected at %INSTALL_DIR%. Skipping installation.%RESET%
    goto :UPDATE_PHASE
)

echo.
echo %CYAN%[STEP 1/4] Downloading MSYS2...%RESET%
curl -L -o %INSTALLER_NAME% "%MSYS2_URL%"

if not exist "%INSTALLER_NAME%" (
    echo %RED%[ERROR] Download failed. Please check your internet connection.%RESET%
    pause
    exit /b
)

echo.
echo %CYAN%[STEP 2/4] Installing MSYS2...%RESET%
echo %YELLOW%Please wait, this may take a few minutes. Do not close the window.%RESET%

:: Use start /wait to ensure the script pauses until installation is complete
start /wait "" %INSTALLER_NAME% --default-answer --accept-licenses --root %INSTALL_DIR% install

if not exist "%INSTALL_DIR%\usr\bin\bash.exe" (
    echo %RED%[ERROR] Installation failed. bash.exe not found.%RESET%
    pause
    exit /b
)

:: Clean up installer
del %INSTALLER_NAME%
echo %GREEN%[SUCCESS] MSYS2 Installed.%RESET%

:UPDATE_PHASE
:: ============================================================================
:: 3. Update and Install Tools (GCC, Make)
:: ============================================================================
echo.
echo %CYAN%[STEP 3/4] Updating system and installing G++ / Make...%RESET%

if not exist "%INSTALL_DIR%\usr\bin\bash.exe" (
    echo %RED%[ERROR] MSYS2 core not found at %INSTALL_DIR%.%RESET%
    pause
    exit /b
)

:: Update Core System
echo %YELLOW%-> Updating Package Database...%RESET%
"%INSTALL_DIR%\usr\bin\bash.exe" -l -c "pacman -Syu --noconfirm"

:: Install Toolchain
echo %YELLOW%-> Installing Toolchain (gcc, make, etc.)...%RESET%
"%INSTALL_DIR%\usr\bin\bash.exe" -l -c "pacman -S --needed --noconfirm base-devel mingw-w64-ucrt-x86_64-toolchain make"

:: ============================================================================
:: 4. Set Environment Variables
:: ============================================================================
echo.
echo %CYAN%[STEP 4/4] Setting Environment Variables (PATH)...%RESET%

set "BIN_UCRT=%INSTALL_DIR%\ucrt64\bin"
set "BIN_USR=%INSTALL_DIR%\usr\bin"

:: Use PowerShell to persistently add to System PATH
powershell -Command "$p=[Environment]::GetEnvironmentVariable('Path','Machine'); if(-not $p.Contains('%BIN_UCRT%')){[Environment]::SetEnvironmentVariable('Path',$p+';%BIN_UCRT%','Machine');Write-Host '[ADDED] UCRT64'} else {Write-Host '[SKIP] UCRT64 exists'}; if(-not $p.Contains('%BIN_USR%')){[Environment]::SetEnvironmentVariable('Path',$p+';%BIN_USR%','Machine');Write-Host '[ADDED] USR/BIN'}"

:: ============================================================================
:: Verification
:: ============================================================================
echo.
echo %GREEN%==================================================%RESET%
echo %GREEN%      INSTALLATION COMPLETE!                      %RESET%
echo %GREEN%==================================================%RESET%
echo.
echo Verifying installation...

:: Temporarily set PATH for this session to verify immediately
set "PATH=%BIN_UCRT%;%BIN_USR%;%PATH%"

echo -----------------------------------
echo %BOLD%Checking G++:%RESET%
g++ --version | findstr "g++"
if %errorLevel% neq 0 echo %RED%[WARN] G++ not found immediately.%RESET%

echo.
echo %BOLD%Checking Make:%RESET%
make --version | findstr "Make"
if %errorLevel% neq 0 echo %RED%[WARN] Make not found immediately.%RESET%
echo -----------------------------------

echo.
echo %YELLOW%[IMPORTANT] Please CLOSE this window and restart VSCode/CMD.%RESET%
echo %YELLOW%           Then you can run 'make' or './run3.sh'.%RESET%
echo.
pause