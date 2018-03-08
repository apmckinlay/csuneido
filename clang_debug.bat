@if not exist clang_debug mkdir clang_debug
@if not exist gc6.5\clang_debug mkdir gc6.5\clang_debug
doskey make=c:\mingw\bin\mingw32-make -f makefile.clang DEBUG=1 OUTPUT=clang_debug $*
path C:\Program Files (x86)\LLVM\bin;c:\mingw\bin;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;c:\program files (x86)\bin
@title clang debug
