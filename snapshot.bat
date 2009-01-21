suneido -dump stdlib
suneido -dump suneidoc
suneido -dump imagebook
call saveaway suneido.db
suneido -load stdlib
suneido -load suneidoc
suneido -load imagebook
del suneido%1.zip
zip -9 suneido%1.zip suneido.exe suneido.db scilexer.dll libhpdf.dll clucene.dll
du -h suneido%1.zip
del suneido.db
unsave suneido.db
