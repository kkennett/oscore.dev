@echo off
rem   
rem   BSD 3-Clause License
rem   
rem   Copyright (c) 2020, Kurt Kennett
rem   All rights reserved.
rem   
rem   Redistribution and use in source and binary forms, with or without
rem   modification, are permitted provided that the following conditions are met:
rem   
rem   1. Redistributions of source code must retain the above copyright notice, this
rem      list of conditions and the following disclaimer.
rem   
rem   2. Redistributions in binary form must reproduce the above copyright notice,
rem      this list of conditions and the following disclaimer in the documentation
rem      and/or other materials provided with the distribution.
rem   
rem   3. Neither the name of the copyright holder nor the names of its
rem      contributors may be used to endorse or promote products derived from
rem      this software without specific prior written permission.
rem   
rem   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
rem   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
rem   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
rem   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
rem   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
rem   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
rem   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
rem   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
rem   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
rem   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
rem   
echo.
echo Installing k2tools via nuget
echo.
type nul
nuget.exe install k2tools32 -Version 2.0.0 -Source https://www.myget.org/F/k2tools32/api/v2 > NUL
if "%errorlevel%"=="0" goto continue1
echo.
echo Error %errorlevel% calling nuget.  Is it installed and on path?
echo.
goto :EOF
:continue1
if exist k2tools32\. rd /s /q k2tools32
move k2tools32.2.0.0\k2tools32 . > NUL
rd /s /q k2tools32.2.0.0
echo.
echo Tools installed. You can run k2tools32\k2setup now
