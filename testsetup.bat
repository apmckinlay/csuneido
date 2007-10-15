if not /%1/ == // goto ok
echo Usage: testsetup yymmdd
goto done

:ok
cls
rd /s/q \suneido

\dev\suneido\installer\output\suneidosetup%1

cd \suneido
suneido -t TestRunner.RunAll();Exit()
suneido -load mylib
suneido -load mybook
suneido -load mycontacts

cd source
call vc8_release
cd gc
make -f makefile.vc8
copy vc8_release\gc.lib ..\gc_vc8_release.lib
cd ..
make -f makefile.vc8
cd ..
source\vc8_release\suneido -t TestRunner.RunAll();Exit()

:done
