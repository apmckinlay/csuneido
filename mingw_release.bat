@if not exist mingw_release mkdir mingw_release
@if not exist gc6.5\mingw_release mkdir gc6.5\mingw_release
doskey make=mingw32-make -f makefile.mingw $*
path c:\mingw\bin;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;c:\program files (x86)\bin
@title mingw release
