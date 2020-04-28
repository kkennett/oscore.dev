@echo off
if /I "%COMPUTERNAME%" == "ZEFPHYR" goto :IsZefphyr
:NotRaven
set _K2_VHD_DRIVE=H
goto :DoneCheck
:IsZefphyr
set _K2_VHD_DRIVE=D
:DoneCheck
call attach c:\repo\oscore.dev\vhd\x86disk.vhd
copy /Y c:\repo\oscore.dev\src\uefi\Build\K2OsLoader\DEBUG_GCC48\IA32\K2Loader.efi %_K2_VHD_DRIVE%:\EFI\BOOT\BOOTIA32.EFI
copy /Y C:\repo\oscore.dev\bld\out\gcc\dlx\X32\Debug\kern\*.* %_K2_VHD_DRIVE%:\K2OS\X32\KERN
call detach c:\repo\oscore.dev\vhd\x86disk.vhd
