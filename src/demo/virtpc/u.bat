@echo off
if /i "%1"=="" goto :end
call attach c:\repo\oscore.dev\vhd\oscore.dev.vhd
rd /s /q %1%\efi
rd /s /q %1%\k2os
xcopy /Y c:\repo\oscore.dev\img\x32\debug\demo\virtpc\bootdisk\*.* %1%\ /e /s
call detach c:\repo\oscore.dev\vhd\oscore.dev.vhd
:end