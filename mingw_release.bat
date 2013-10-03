@if not exist mingw_release mkdir mingw_release
@if not exist gc6.5\mingw_release mkdir gc6.5\mingw_release
doskey make=make -f makefile.mingw $*
path %path%;c:\mingw\bin
@title mingw release
