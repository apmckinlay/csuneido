@if not exist mingw32_release mkdir mingw32_release
@if not exist gc6.5\mingw32_release mkdir gc6.5\mingw32_release
doskey make=mingw32-make -f makefile.mingw OUTPUT=mingw32_release $*
path C:\mingw32\bin;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;c:\program files (x86)\bin
@title mingw32 release
