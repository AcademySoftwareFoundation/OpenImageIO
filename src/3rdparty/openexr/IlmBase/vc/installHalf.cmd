@echo off
set deploypath=..\..\..\..\..\..\Deploy
set src=..\..\..\..\Half

if not exist %deploypath% mkdir %deploypath%

set intdir=%1%
if %intdir%=="" set intdir=Release
echo Installing into %intdir%
set instpath=%deploypath%\lib\%intdir%
if not exist %instpath% mkdir %instpath%
copy ..\%intdir%\Half.lib %instpath%
copy ..\%intdir%\Half.exp %instpath%

set instpath=%deploypath%\bin\%intdir%
if not exist %instpath% mkdir %instpath%
copy ..\%intdir%\Half.dll %instpath%

cd %src%
set instpath=..\..\..\Deploy\include
if not exist %instpath% mkdir %instpath%

copy half.h %instpath%
copy halfFunction.h %instpath%
copy halfLimits.h %instpath%

