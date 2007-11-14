@if not exist ace_release mkdir ace_release
@if not exist gc6.5\ace_release mkdir gc6.5\ace_release
doskey make=make -f makefile.mingw $*
@title mingw ace release
