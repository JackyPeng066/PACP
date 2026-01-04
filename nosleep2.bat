@echo off
title Anti-Shutdown Script (Close window to stop)
echo Monitoring... Attempting to abort shutdown every 10 seconds.

:start
rem Try to abort shutdown
shutdown /a >nul 2>&1

rem Wait 10 seconds and repeat
timeout /t 10 /nobreak >nul
goto start