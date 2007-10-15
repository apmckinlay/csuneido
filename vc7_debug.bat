@if not exist vc7_debug mkdir vc7_debug
@if not exist gc6.5\vc7_debug mkdir gc6.5\vc7_debug
call "C:\Program Files\Microsoft Visual C++ Toolkit 2003\vcvars32.bat"
@echo on
call "c:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2\setenv" /XP32 /DEBUG
@echo on
@rem need to reverse these or you get the wrong versions
SET LIB=C:\Program Files\Microsoft Visual C++ Toolkit 2003\lib;C:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2\Lib
doskey make=make -f makefile.vc7 DEBUG=1 $*
@title vc7 debug
