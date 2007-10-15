cls
rd /s/q \suneido
unzip -q \suneido.zip -d \suneido

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
