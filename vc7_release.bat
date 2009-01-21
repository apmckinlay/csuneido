@if not exist vc7_release mkdir vc7_release
@if not exist gc6.5\vc7_release mkdir gc6.5\vc7_release
call "C:\Program Files\Microsoft Visual C++ Toolkit 2003\vcvars32.bat"
@echo on
call "c:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2\setenv" /XP32 /RETAIL
@echo on
@rem need to reverse these or you get the wrong versions
set LIB=C:\Program Files\Microsoft Visual C++ Toolkit 2003\lib;C:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2\Lib
doskey make=make -f makefile.vc7 $*
@title vc7 release
