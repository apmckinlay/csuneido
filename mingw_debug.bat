@if not exist mingw_debug mkdir mingw_debug
@if not exist gc6.5\mingw_debug mkdir gc6.5\mingw_debug
doskey make=make -f makefile.mingw DEBUG=1 $*
path %path%;c:\mingw\bin
@title mingw debug
