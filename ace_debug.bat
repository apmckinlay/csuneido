@if not exist ace_debug mkdir ace_debug
@if not exist gc6.5\ace_debug mkdir gc6.5\ace_debug
doskey make=make -f makefile.ace DEBUG=1 $*
@title mingw ace debug
