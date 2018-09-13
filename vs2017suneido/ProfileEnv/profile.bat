@echo off
SET testDir=%1
SET profileDir=%2
IF EXIST %profileDir%\vs2017suneido.exe (
	copy %profileDir%\vs2017suneido.exe %testDir%\suneido.exe
)
cd %testDir%

echo EXE compact: START
start /w suneido.exe -compact
echo EXE compact: END

echo Suneido setup: START
start /W suneido.exe setup
echo Suneido setup: END

for %%f in (*.go) do (
	echo %%~nf: START
	start /W suneido.exe %%~f
	echo %%~nf: END
)
IF EXIST %testDir%\*.pgc (
	move %testDir%\*.pgc %profileDir%
)

exit