call attach c:\repo\oscore.dev\vhd\x86disk.vhd
copy /Y c:\repo\oscore.dev\src\uefi\Build\K2OsLoader\DEBUG_GCC48\IA32\K2Loader.efi h:\EFI\BOOT\BOOTIA32.EFI
copy /Y C:\repo\oscore.dev\bld\out\gcc\dlx\X32\Debug\kern\*.* H:\K2OS\X32\KERN
call detach c:\repo\oscore.dev\vhd\x86disk.vhd
