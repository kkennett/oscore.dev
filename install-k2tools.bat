@echo off
echo.
echo Installing k2tools via nuget
echo.
type nul
nuget.exe install k2tools32 -Version 1.0.0 -Source https://www.myget.org/F/k2tools32/api/v2 > NUL
if "%errorlevel%"=="0" goto continue1
echo.
echo Error %errorlevel% calling nuget.  Is it installed and on path?
echo.
goto :EOF
:continue1
if exist k2tools32\. rd /s /q k2tools32
move k2tools32.1.0.0\k2tools32 . > NUL
rd /s /q k2tools32.1.0.0
echo.
echo Tools installed. You can run k2tools32\k2setup now
