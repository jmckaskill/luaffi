@if not defined INCLUDE goto :FAIL

@setlocal
@set LUA_INCLUDE=C:\Lua5.1\include
@set LUA_LIB=C:\Lua5.1\lib\lua5.1.lib
@set LUA_EXE=C:\Lua5.1\lua.exe

@if "%1"=="debug" goto :DEBUG
@if "%1"=="clean" goto :CLEAN
goto :RELEASE

:RELEASE
@set DO_CL=cl.exe /nologo /c /MD /Ox /W3 /WX /D_CRT_SECURE_NO_DEPRECATE /I"msvc"
@set DO_LINK=link.exe /nologo
@set DO_MT=mt.exe /nologo
@goto :COMPILE

:DEBUG
@set DO_CL=cl /nologo /c /MDd /FC /Zi /Od /W3 /WX /D_CRT_SECURE_NO_DEPRECATE /I"msvc"
@set DO_LINK=link /nologo /debug
@set DO_MT=mt /nologo
@goto :COMPILE

:COMPILE
%LUA_EXE% dynasm\dynasm.lua -LN -o call_x86.h call_x86.dasc
%DO_CL% /I"." /I"%LUA_INCLUDE%" call.c ctype.c ffi.c parser.c
%DO_LINK% /DLL /OUT:ffi.dll "%LUA_LIB%" *.obj
if exist ffi.dll.manifest^
    %DO_MT% -manifest ffi.dll.manifest -outputresource:"ffi.dll;2"

:TEST
%DO_CL% /Gd test.c /Fo"test_cdecl.obj"
%DO_CL% /Gz test.c /Fo"test_stdcall.obj"
%DO_LINK% /DLL /OUT:test_cdecl.dll test_cdecl.obj
%DO_LINK% /DLL /OUT:test_stdcall.dll test_stdcall.obj
if exist test_cdecl.dll.manifest^
    %DO_MT% -manifest test_cdecl.dll.manifest -outputresource:"test_cdecl.dll;2"
if exist test_stdcall.dll.manifest^
    %DO_MT% -manifest test_stdcall.dll.manifest -outputresource:"test_stdcall.dll;2"

%LUA_EXE% test.lua

:CLEAN
del *.obj *.manifest test_*.dll

@goto :END
:FAIL
@echo You must open a "Visual Studio .NET Command Prompt" to run this script
:END
