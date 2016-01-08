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
copy \dev\jsuneido\jsuneido.jar
copy \dev\jsuneido\lib\lucene*.jar
del suneido%1.zip
zip -9 suneido%1.zip suneido.exe suneido.db scilexer.dll clucene.dll zlibwapi.dll hunspell.exe en_US.aff en_US.dic splash.bmp jsuneido.jar lucene*.jar mylib.su mybook.su mycontacts.su release.html
del jsuneido.jar
del lucene*.jar
del suneido.db
call unsave suneido.db
du -h suneido%1.zip
