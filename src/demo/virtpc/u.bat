@echo off
if /I "%COMPUTERNAME%" == "ZEFPHYR" goto :IsZefphyr
:NotRaven
set _K2_VHD_DRIVE=H
goto :DoneCheck
:IsZefphyr
set _K2_VHD_DRIVE=D
:DoneCheck
call attach c:\repo\oscore.dev\vhd\x86disk.vhd
rd /s /q %_K2_VHD_DRIVE%:\efi
rd /s /q %_K2_VHD_DRIVE%:\k2os
xcopy /Y c:\repo\oscore.dev\img\x32\debug\demo\virtpc\bootdisk\*.* %_K2_VHD_DRIVE%:\ /e /s
call detach c:\repo\oscore.dev\vhd\x86disk.vhd
