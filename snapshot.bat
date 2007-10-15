suneido -dump stdlib
suneido -dump suneidoc
del suneido%1.zip
zip -9 suneido%1.zip suneido.exe stdlib.su suneidoc.su scilexer.dll libhpdf.dll clucene.dll suneido%1.txt
du -h suneido%1.zip
