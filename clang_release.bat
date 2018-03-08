@if not exist clang_release mkdir clang_release
@if not exist gc6.5\clang_release mkdir gc6.5\clang_release
doskey make=c:\mingw\bin\mingw32-make -f makefile.clang OUTPUT=clang_release $*
path C:\Program Files (x86)\LLVM\bin;c:\mingw\bin;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;c:\program files (x86)\bin
@title clang release
