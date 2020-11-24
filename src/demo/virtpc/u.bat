@echo off
if /i "%1"=="" goto :end
call attach c:\repo\oscore.dev\vhd\x86disk.vhd
rd /s /q %1%\efi
rd /s /q %1%\k2os
xcopy /Y c:\repo\oscore.dev\img\x32\debug\demo\virtpc\bootdisk\*.* %1%\ /e /s
call detach c:\repo\oscore.dev\vhd\x86disk.vhd
:end