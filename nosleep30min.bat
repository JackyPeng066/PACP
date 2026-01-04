@echo off
title [NO-SLEEP KEEPER] Running... (Do not close)
mode con: cols=50 lines=12
color 0A

echo ==================================================
echo           NO-SLEEP KEEPER (F15 Ghost Key)
echo ==================================================
echo.
echo   [STATUS] Active
echo   [ACTION] Sending F15 key signal every 60s.
echo   [NOTE]   This will NOT interfere with typing
echo            or your running programs.
echo.
echo   Please MINIMIZE this window. DO NOT close it.
echo ==================================================

:loop
:: Use PowerShell to send the F15 key (Resets system idle timer)
powershell -NoProfile -Command "$w = New-Object -ComObject WScript.Shell; $w.SendKeys('{F15}')"

:: Wait 1800 seconds (Silent countdown)
timeout /t 1800 >nul

:: Repeat
goto loop