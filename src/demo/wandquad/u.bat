@echo off
if /i "%1"=="" goto :end
rd /s /q %1%\efi
rd /s /q %1%\k2os
xcopy /Y c:\repo\oscore.dev\img\a32\debug\demo\wandquad\bootdisk\*.* %1%\ /e /s
:end