@echo off

if not /%1/ == // goto ok
echo Usage: snapshot yymmdd
goto done

:ok
cls
@echo on
rd /s/q \suneido%1
md \suneido%1
copy suneido.exe \suneido%1
copy splash.bmp \suneido%1
copy vercheck\release\vercheck.exe \suneido%1
copy scilexer.dll \suneido%1
copy libhpdf.dll \suneido%1
copy clucene.dll \suneido%1
copy building.txt \suneido%1
copy installing.txt \suneido%1
copy upgrading.txt \suneido%1
copy release%1.txt \suneido%1
copy release%1.htm \suneido%1
xcopy /i release%1 \suneido%1\release%1
md \suneido%1\installer
copy installer\*.bmp \suneido%1\installer
copy installer\suneidoinstall.iss \suneido%1\installer

md \suneido%1\source
copy *.h \suneido%1\source
copy *.c \suneido%1\source
copy *.cpp \suneido%1\source
del \suneido%1\source\tmp.*
del \suneido%1\source\temp.*
copy suneido.rc \suneido%1\source
copy COPYING \suneido%1\source

md \suneido%1\source\res
copy res\*.* \suneido%1\source\res

md \suneido%1\source\vercheck
copy vercheck\vercheck.dsp \suneido%1\source\vercheck
copy vercheck\vercheck.cpp \suneido%1\source\vercheck
copy vercheck\vercheck.rc \suneido%1\source\vercheck
copy vercheck\resource.h \suneido%1\source\vercheck
copy vercheck\vercheck.txt \suneido%1\source\vercheck

md \suneido%1\source\mingw_debug
md \suneido%1\source\mingw_release
md \suneido%1\source\vc7_debug
md \suneido%1\source\vc7_release
md \suneido%1\source\vc8_debug
md \suneido%1\source\vc8_release
copy makefile.common \suneido%1\source
copy makefile.mingw \suneido%1\source
copy makefile.vc? \suneido%1\source
copy vc?_*.bat \suneido%1\source
copy mingw_*.bat \suneido%1\source

md \suneido%1\source\gc
copy \dev\gc65\makefile \suneido%1\source\gc
copy \dev\gc65\makefile.mingw \suneido%1\source\gc
copy \dev\gc65\makefile.vc? \suneido%1\source\gc
copy \dev\gc65\*.h \suneido%1\source\gc
copy \dev\gc65\*.s \suneido%1\source\gc
copy \dev\gc65\*.c \suneido%1\source\gc
xcopy /s/i \dev\gc65\include \suneido%1\source\gc\include
copy \dev\gc65\gc.lib \suneido%1\source
md \suneido%1\source\gc\mingw_debug
md \suneido%1\source\gc\mingw_release
md \suneido%1\source\gc\vc7_debug
md \suneido%1\source\gc\vc7_release
md \suneido%1\source\gc\vc8_debug
md \suneido%1\source\gc\vc8_release
md \suneido%1\source\gc\tests
copy \dev\gc65\tests\test.c \suneido%1\source\gc\tests

suneido -dump mylib
move mylib.su \suneido%1
suneido -dump mybook
move mybook.su \suneido%1
suneido -dump mycontacts
move mycontacts.su \suneido%1

suneido -dump stdlib
suneido -dump suneidoc
suneido -dump imagebook
copy /y z:\software\suneido\translatelanguage.su
call saveaway suneido.db
suneido -load stdlib
suneido -load suneidoc
suneido -load imagebook
suneido -load translatelanguage
move suneido.db \suneido%1
call unsave suneido.db
move stdlib.su \suneido%1
move suneidoc.su \suneido%1
move imagebook.su \suneido%1
move translatelanguage.su \suneido%1

cd \suneido%1

:done
