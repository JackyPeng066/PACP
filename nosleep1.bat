@echo off
title No Sleep Script (Close window to stop)
echo Script is running...
echo Pressing F15 key every 60 seconds to keep PC awake.
echo Do not close this window until you are done.

:loop
rem Create a temporary VBScript to press a key
echo Set wsc = CreateObject("WScript.Shell") > _temp.vbs
echo wsc.SendKeys "{F15}" >> _temp.vbs

rem Run the script
cscript //nologo _temp.vbs

rem Delete the temporary file
del _temp.vbs

rem Wait for 60 seconds
timeout /t 60 /nobreak >nul
goto loop