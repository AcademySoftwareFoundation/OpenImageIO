@echo off

if exist ..\..\..\..\IlmImf\b44ExpLogTable.h goto skip

set intdir=%1%
if %intdir%=="" set intdir=Release

cl /nologo /DOPENEXR_DLL /GR /GX /I..\..\..\..\..\..\Deploy\include ..\..\..\..\IlmImf\b44ExpLogTable.cpp /MD /Feb44ExpLogTable.exe /link "..\..\..\..\..\..\Deploy\lib\Release\Half.lib"
copy ..\..\..\..\..\..\Deploy\bin\%intdir%\Half.dll .
.\b44ExpLogTable.exe > ..\..\..\..\IlmImf\b44ExpLogTable.h
del Half.dll
del .\b44ExpLogTable.obj .\b44ExpLogTable.exe

:skip
