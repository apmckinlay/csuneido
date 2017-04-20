@if not exist mingw64_release mkdir mingw64_release
@if not exist gc6.5\mingw64_release mkdir gc6.5\mingw64_release
doskey make=mingw32-make -f makefile.mingw OUTPUT=mingw64_release $*
path C:\Program Files (x86)\mingw-w64\i686-6.3.0-posix-dwarf-rt_v5-rev1\mingw32\bin;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;c:\program files (x86)\bin
@title mingw64 release
