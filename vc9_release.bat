@if not exist vc9_release mkdir vc9_release
@if not exist gc6.5\vc9_release mkdir gc6.5\vc9_release
call "C:\Program Files\Microsoft Visual Studio 9.0\VC\vcvarsall.bat" x86
@echo on
doskey make=c:\mingw\bin\mingw32-make -f makefile.vc9 $*
@title vc9 release
