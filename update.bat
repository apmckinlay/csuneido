@echo off
REM used when the database format changes
REM dumps with the old exe (suneido.exe) 
REM and loads the the new exe (suneido_new.exe)
REM and finally replaces the old exe wih the new one
if not exist suneido.exe goto rename
if not exist suneido.db goto rename
start /w suneido -dump
start /w suneido_new -load
del database.su
:rename
move /y suneido_new.exe suneido.exe
