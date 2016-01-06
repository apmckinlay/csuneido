suneido -dump stdlib
suneido -dump Contrib
suneido -dump suneidoc
suneido -dump imagebook
suneido -dump mylib
suneido -dump mybook
suneido -dump mycontacts
copy z:\software\suneido\translatelanguage.su
call saveaway suneido.db
suneido -load stdlib
suneido -load Contrib
suneido -load suneidoc
suneido -load imagebook
suneido -load translatelanguage.su
copy z:\software\suneido\jsuneido.jar
del suneido%1.zip
zip -9 suneido%1.zip suneido.exe suneido.db scilexer.dll clucene.dll zlibwapi.dll hunspell.exe en_US.aff en_US.dic splash.bmp jsuneido.jar mylib.su mybook.su mycontacts.su
del jsuneido.jar
del suneido.db
call unsave suneido.db
du -h suneido%1.zip
