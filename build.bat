@echo off
setlocal enableextensions enabledelayedexpansion

set X_TO_BUILD="%1"
set CONFIG=D
set C_FLAGS=/std:c17 /I. /experimental:c11atomics /GR- /nologo /Gm- /WX /Wall /wd4191 /wd4820 /wd4255 /wd5045

if %CONFIG%==D set C_FLAGS=%C_FLAGS% /GS /Zi /Od /D"_DEBUG" /MTd /RTCs
if %CONFIG%==R set C_FLAGS=%C_FLAGS% /O2 /Gy /MT /D"NDEBUG" /Oi /Ot /GS-

set LINK_FLAGS=/INCREMENTAL:NO /NOLOGO
if %CONFIG%==D set LINK_FLAGS=%LINK_FLAGS% /DEBUG:FULL
if %CONFIG%==R set LINK_FLAGS=%LINK_FLAGS%

if exist *.obj del *.obj
if exist *.pdb del *.pdb
if exist *.exe del *.exe

if %X_TO_BUILD%=="" (
  cl %C_FLAGS% /MP /c *.c
  for %%f in (*.obj) do (
    link %LINK_FLAGS% %%f
  )
)

if not %X_TO_BUILD%=="" (
  cl %C_FLAGS% %X_TO_BUILD%.c /link %LINK_FLAGS%
  if "%2"=="run" if exist %X_TO_BUILD%.exe %X_TO_BUILD%.exe
)

if exist *.obj del *.obj
if exist *.lib del *.lib
if exist *.exp del *.exp
if exist vc140.pdb del vc140.pdb
