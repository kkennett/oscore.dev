call attach c:\repo\oscore.dev\vhd\x86disk.vhd
copy /Y \repo\oscore.dev\bld\out\gcc\dlx\X32\Debug\kern\*.* f:\K2OS\X32\KERN
call detach c:\repo\oscore.dev\vhd\x86disk.vhd
