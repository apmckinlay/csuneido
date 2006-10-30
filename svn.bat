@echo off
xcopy /d /y /i *.h ..\suneido_svn
xcopy /d /y *.c ..\suneido_svn
xcopy /d /y *.cpp ..\suneido_svn
xcopy /d /y makefile.* ..\suneido_svn
xcopy /d /y depends8 ..\suneido_svn
xcopy /d /y suneido.rc ..\suneido_svn
xcopy /d /y /i res ..\suneido_svn\res
xcopy /d /y /i installer\*.bmp ..\suneido_svn\installer
xcopy /d /y installer\SuneidoInstall.iss ..\suneido_svn\installer
xcopy /d /y vc?.bat ..\suneido_svn
xcopy /d /y svn.bat ..\suneido_svn
xcopy /d /y /i ..\gc6.5\*.c ..\suneido_svn\gc6.5
xcopy /d /y ..\gc6.5\*.h ..\suneido_svn\gc6.5
xcopy /d /y ..\gc6.5\*.cpp ..\suneido_svn\gc6.5
xcopy /d /y ..\gc6.5\makefile.vc7 ..\suneido_svn\gc6.5
xcopy /d /y ..\gc6.5\makefile.vc8 ..\suneido_svn\gc6.5
xcopy /d /y ..\gc6.5\makefile.mingw ..\suneido_svn\gc6.5
xcopy /d /y /s /i ..\gc6.5\tests\*.c ..\suneido_svn\gc6.5\tests
xcopy /d /y /s /i ..\gc6.5\include ..\suneido_svn\gc6.5\include
md ..\suneido_svn\vc7
md ..\suneido_svn\vc8
