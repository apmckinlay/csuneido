SET testDir=%1
SET profileDir=%2
IF EXIST %profileDir%\vs2017suneido.exe (
	copy %profileDir%\vs2017suneido.exe %testDir%\suneido.exe
)
cd %testDir%
echo EXE compact: START
start /w suneido.exe -compact
echo EXE compact: END
echo EXE setup: START
start /W suneido.exe setup.go
echo EXE setup: END
echo EXE allCA: START
start /W suneido.exe allCA.go
echo EXE allCA: END
echo EXE allUS: START
start /W suneido.exe allUS.go
echo EXE allUS: END
echo EXE simpleCA: START
start /W suneido.exe simpleCA.go
echo EXE simpleCA: END
echo EXE simpleUS: START
start /W suneido.exe simpleUS.go
echo EXE simpleUS: END
IF EXIST %testDir%\*.pgc (
	move %testDir%\*.pgc %profileDir%
)
exit