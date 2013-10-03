@if not exist vc11_release mkdir vc11_release
@if not exist gc6.5\vc11_release mkdir gc6.5\vc11_release
call "%ProgramFiles(x86)%\Microsoft Visual Studio 11.0\VC\vcvarsall.bat" x86
set INCLUDE=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Include;%INCLUDE%
set PATH=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Bin;%PATH%;c:\mingw\bin
set LIB=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Lib;%LIB%
set CL=/D_USING_V110_SDK71_;%CL%
set LINK=/SUBSYSTEM:WINDOWS,5.01 %LINK%
@echo on
doskey make=make -f makefile.vc11 $*
@title vc11 release
