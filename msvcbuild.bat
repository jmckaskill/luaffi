@if not defined INCLUDE goto :FAIL

@setlocal
@set LUA_INCLUDE=C:\Lua5.1\include
@set LUA_LIB=C:\Lua5.1\lib\lua5.1.lib
@set DO_DYNASM=C:\Lua5.1\lua.exe dynasm\dynasm.lua

@if "%1"=="debug" goto :DEBUG
@if "%1"=="clean" goto :CLEAN
goto :RELEASE

:RELEASE
@set DO_CL=cl.exe /nologo /c /MD /Ox /W3 /D_CRT_SECURE_NO_DEPRECATE
@set DO_LINK=link.exe /nologo
@set DO_MT=mt.exe /nologo
@goto :COMPILE

:DEBUG
@set DO_CL=cl /nologo /c /MDd /FC /Zi /Od /W3 /D_CRT_SECURE_NO_DEPRECATE /TC
@set DO_LINK=link /nologo /debug
@set DO_MT=mt /nologo
@goto :COMPILE

:COMPILE
%DO_DYNASM% -LN -o call_x86.h call_x86.dasc
%DO_CL% /I"." /I"%LUA_INCLUDE%" /I"msvc" call.c ctype.c ffi.c parser.c
%DO_LINK% /DLL /OUT:ffi.dll "%LUA_LIB%" psapi.lib *.obj
if exist ffi.dll.manifest^
    %DO_MT% -manifest ffi.dll.manifest -outputresource:"ffi.dll;2"

:CLEAN
del *.obj *.manifest

@goto :END
:FAIL
@echo You must open a "Visual Studio .NET Command Prompt" to run this script
:END
