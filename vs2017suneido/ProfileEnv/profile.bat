cd %1
echo Profiling...
del %2\*.pgc 2>nul
start /w %2\vs2017suneido.exe -u -t
start /w %2\vs2017suneido.exe -compact
start /w %2\vs2017suneido.exe test.go
