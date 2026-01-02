@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul

:: ============================================================================
:: PACP C++ 開發環境一鍵懶人包 (MSYS2 + Make + GCC + PATH)
:: ============================================================================

TITLE PACP 開發環境自動安裝中...

:: 1. 權限檢查 (因為要寫入系統 PATH，必須是管理員)
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] 權限不足！
    echo         請對此檔案按右鍵，選擇「以系統管理員身分執行」。
    echo.
    pause
    exit /b
)

set "INSTALL_DIR=C:\msys64"
set "MSYS2_URL=https://repo.msys2.org/distrib/x86_64/msys2-x86_64-latest.exe"
set "INSTALLER_NAME=msys2-installer.exe"

:: ============================================================================
:: 步驟 1：下載與安裝 MSYS2
:: ============================================================================
if exist "%INSTALL_DIR%" (
    echo [INFO] 偵測到 %INSTALL_DIR% 已存在，跳過安裝，直接進行更新與設定。
    goto :UPDATE_PHASE
)

echo [STEP 1/4] 下載 MSYS2 安裝檔...
curl -L -o %INSTALLER_NAME% "%MSYS2_URL%"

if not exist "%INSTALLER_NAME%" (
    echo [ERROR] 下載失敗，請檢查網路。
    pause
    exit /b
)

echo.
echo [STEP 2/4] 靜默安裝 MSYS2 (路徑: %INSTALL_DIR%)...
%INSTALLER_NAME% in --confirm-command --accept-messages --root %INSTALL_DIR%
del %INSTALLER_NAME%
echo [INFO] 安裝完成。

:UPDATE_PHASE
:: ============================================================================
:: 步驟 2：更新與安裝工具 (只裝 PACP 專案需要的)
:: ============================================================================
echo.
echo [STEP 3/4] 更新系統並安裝 g++, make...

:: 第一次呼叫：更新核心 (pacman -Syu)
"%INSTALL_DIR%\usr\bin\bash.exe" -l -c "pacman -Syu --noconfirm"

:: 第二次呼叫：安裝工具鏈
:: base-devel: 提供 rm, cp, bash 等 shell 指令 (run.sh 需要)
:: toolchain: 提供 g++, gcc
:: make: 編譯需要
"%INSTALL_DIR%\usr\bin\bash.exe" -l -c "pacman -S --needed --noconfirm base-devel mingw-w64-ucrt-x86_64-toolchain make"

:: ============================================================================
:: 步驟 3：自動將路徑加入 Windows 系統環境變數 (PATH)
:: ============================================================================
echo.
echo [STEP 4/4] 設定 Windows 環境變數 (PATH)...

:: 使用 PowerShell 腳本來安全地添加 PATH，避免覆蓋現有設定
set "BIN_UCRT=%INSTALL_DIR%\ucrt64\bin"
set "BIN_USR=%INSTALL_DIR%\usr\bin"

powershell -Command ^
    "$path = [Environment]::GetEnvironmentVariable('Path', 'Machine');" ^
    "if (-not $path.Contains('%BIN_UCRT%')) {" ^
    "    [Environment]::SetEnvironmentVariable('Path', $path + ';%BIN_UCRT%', 'Machine');" ^
    "    Write-Host '[ADDED] %BIN_UCRT%';" ^
    "} else { Write-Host '[SKIP] UCRT64 path already exists.'; };" ^
    "if (-not $path.Contains('%BIN_USR%')) {" ^
    "    [Environment]::SetEnvironmentVariable('Path', $path + ';%BIN_USR%', 'Machine');" ^
    "    Write-Host '[ADDED] %BIN_USR%';" ^
    "} else { Write-Host '[SKIP] USR path already exists.'; }"

:: ============================================================================
:: 驗證與結束
:: ============================================================================
echo.
echo ===================================================================
echo  安裝與設定全部完成！
echo ===================================================================
echo.
echo  驗證安裝版本 (若顯示版本號即成功)：
echo  -----------------------------------
call refreshenv >nul 2>&1
g++ --version | findstr "g++" 
if %errorLevel% neq 0 echo [WARN] 目前視窗可能抓不到新 PATH，重開 CMD 後即可生效。
make --version | findstr "Make"

echo.
echo  [重要] 請 **關閉此視窗**，並重新開啟 VSCode 或 CMD。
echo         接著你就可以直接輸入 'make' 或 './run.sh' 了！
echo.
pause