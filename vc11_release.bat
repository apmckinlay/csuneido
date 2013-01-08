@if not exist vc11_release mkdir vc11_release
@if not exist gc6.5\vc11_release mkdir gc6.5\vc11_release
call "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\vcvarsall.bat" x86
@echo on
doskey make=make -f makefile.vc11 $*
@title vc11 release
