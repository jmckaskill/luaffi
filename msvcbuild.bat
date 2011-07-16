@if not defined INCLUDE goto :FAIL

@setlocal
@set LUA_INCLUDE=C:\Lua5.1\include
@set LUA_LIB=C:\Lua5.1\lib\lua5.1.lib
@set LUA_EXE=C:\Lua5.1\lua.exe
@set LUA_DLL=lua5.1.dll
@rem @set LUA_INCLUDE=C:\SCM\lua5.2\src
@rem @set LUA_LIB=C:\SCM\lua5.2\src\lua5.2.lib
@rem @set LUA_EXE=C:\Lua5.1\lua.exe

@set DO_CL=cl.exe /nologo /c /MDd /FC /Zi /Od /W3 /WX /D_CRT_SECURE_NO_DEPRECATE /I"msvc"
@set DO_LINK=link /nologo /debug
@set DO_MT=mt /nologo

@if "%1"=="debug" goto :COMPILE
@if "%1"=="test" goto :COMPILE
@if "%1"=="clean" goto :CLEAN

:RELEASE
@set DO_CL=cl.exe /nologo /c /MD /Ox /W3 /Zi /WX /D_CRT_SECURE_NO_DEPRECATE /I"msvc"
@set DO_LINK=link.exe /nologo /debug
@set DO_MT=mt.exe /nologo

:COMPILE
%LUA_EXE% dynasm\dynasm.lua -D X32WIN -LN -o call_x86.h call_x86.dasc
%LUA_EXE% dynasm\dynasm.lua -D X64 -LN -o call_x64.h call_x86.dasc
%LUA_EXE% dynasm\dynasm.lua -D X64 -D X64WIN -LN -o call_x64win.h call_x86.dasc
%DO_CL% /I"." /I"%LUA_INCLUDE%" /DLUA_DLL_NAME="%LUA_DLL%" call.c ctype.c ffi.c parser.c
%DO_LINK% /DLL /OUT:ffi.dll "%LUA_LIB%" *.obj
if exist ffi.dll.manifest^
    %DO_MT% -manifest ffi.dll.manifest -outputresource:"ffi.dll;2"

%DO_CL% /Gd test.c /Fo"test_cdecl.obj"
%DO_CL% /Gz test.c /Fo"test_stdcall.obj"
%DO_LINK% /DLL /OUT:test_cdecl.dll test_cdecl.obj
%DO_LINK% /DLL /OUT:test_stdcall.dll test_stdcall.obj
if exist test_cdecl.dll.manifest^
    %DO_MT% -manifest test_cdecl.dll.manifest -outputresource:"test_cdecl.dll;2"
if exist test_stdcall.dll.manifest^
    %DO_MT% -manifest test_stdcall.dll.manifest -outputresource:"test_stdcall.dll;2"

@if "%1"=="test" %LUA_EXE% test.lua
@if "%1"=="test-release" %LUA_EXE% test.lua
@goto :CLEAN_OBJ

:CLEAN
del *.dll
:CLEAN_OBJ
del *.obj *.manifest
@goto :END

:FAIL
@echo You must open a "Visual Studio .NET Command Prompt" to run this script
:END

