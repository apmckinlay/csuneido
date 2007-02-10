call "C:\Program Files\Microsoft Visual Studio 8\VC\vcvarsall.bat" x86
@echo on
call "c:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2\setenv" /XP32 /RETAIL
@echo on
doskey make=make -f makefile.vc8 $*
@title vc8 release
