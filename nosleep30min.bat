@echo off
title Anti-Shutdown Script (Close window to stop)
color 0A

echo ============================================
echo   Anti-Shutdown Monitor
echo   Interval: 30 Minutes (1800 seconds)
echo   Status: Active
echo ============================================

:start
:: Try to abort shutdown
shutdown /a >nul 2>&1

:: Display current time and status
echo [%time:~0,8%] Abort command sent. Waiting 30 minutes...

:: Wait 30 minutes (1800 seconds)
timeout /t 1800 /nobreak >nul
goto start