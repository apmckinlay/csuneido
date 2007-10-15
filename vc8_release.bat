@if not exist vc8_release mkdir vc8_release
@if not exist gc6.5\vc8_release mkdir gc6.5\vc8_release
call "C:\Program Files\Microsoft Visual Studio 8\VC\vcvarsall.bat" x86
@echo on
call "c:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2\setenv" /XP32 /RETAIL
@echo on
doskey make=\mingw\bin\mingw32-make -f makefile.vc8 $*
@title vc8 release
