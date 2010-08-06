@if not exist vc10_release mkdir vc10_release
@if not exist gc6.5\vc10_release mkdir gc6.5\vc10_release
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x86
@echo on
doskey make=make -f makefile.vc10 $*
@title vc10 release
