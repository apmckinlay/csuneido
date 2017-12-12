@if not exist clang mkdir clang
@if not exist gc6.5\clang mkdir gc6.5\clang
doskey make=c:\mingw\bin\mingw32-make -f makefile.clang DEBUG=1 OUTPUT=clang $*
path C:\Program Files (x86)\LLVM\bin;c:\mingw\bin;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;c:\program files (x86)\bin
@title clang
