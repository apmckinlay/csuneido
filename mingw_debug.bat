@if not exist mingw_debug mkdir mingw_debug
@if not exist gc6.5\mingw_debug mkdir gc6.5\mingw_debug
doskey make=mingw32-make -f makefile.mingw DEBUG=1 $*
path c:\mingw\bin;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;c:\program files (x86)\bin
@title mingw debug
