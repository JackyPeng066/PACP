@echo off
setlocal
title Anti-Shutdown Timer (30 min interval)
color 0A

echo ============================================
echo   Anti-Shutdown Script Activated
echo   Interval: 30 Minutes (1800 seconds)
echo   Status: Monitoring...
echo ============================================

:loop
:: 執行取消關機指令
shutdown /a >nul 2>&1

:: 取得當前時間並顯示
set current_time=%time:~0,8%
echo [%current_time%] Sent 'shutdown /a' command.

:: 倒數 60 分鐘 (3600 秒)
:: /nobreak 確保不會因為誤觸鍵盤而中斷倒數
timeout /t 3600 /nobreak

goto loop