
@setlocal

@set MYCOMPILE=cl /nologo /MD /O2 /W3 /c /D_CRT_SECURE_NO_DEPRECATE
@set MYLINK=link /nologo
@set MYMT=mt /nologo

@set FFI_SRC=..
@set LUA_SRC=.\lua51
@set LUA_LIB=%LUA_SRC%\lua51.lib
@set LUA_EXE=lua.exe
@set LUA_DLL=lua51.dll


@set _CC=cl.exe /nologo /c /MDd /FC /Zi /Od /W3 /WX /D_CRT_SECURE_NO_DEPRECATE /DLUA_FFI_BUILD_AS_DLL /I"msvc"
@set _LINK=link /nologo /debug
@set _MT=mt /nologo

@if "%1"=="release" goto :RELEASE
@if "%1"=="test" goto :TEST


:RELEASE
@set _CC=cl.exe /nologo /c /MD /Ox /W3 /Zi /WX /D_CRT_SECURE_NO_DEPRECATE /DLUA_FFI_BUILD_AS_DLL /I"msvc"
@set _LINK=link.exe /nologo /debug
@set _MT=mt.exe /nologo
@goto :COMPILE

:COMPILE
call _generate_call_h.bat
%_CC% /I"." /I"%LUA_SRC%" /DLUA_DLL_NAME="%LUA_DLL%" %FFI_SRC%\call.c %FFI_SRC%\ctype.c %FFI_SRC%\ffi.c %FFI_SRC%\parser.c
%_LINK% /DLL /OUT:ffi.dll "%LUA_LIB%" *.obj
if exist ffi.dll.manifest^
    %_MT% -manifest ffi.dll.manifest -outputresource:"ffi.dll;2"
@goto :CLEAN_OBJ

:TEST
%_CC% /Gd %FFI_SRC%\test.c /I"." /Fo"test_cdecl.obj"
%_CC% /Gz %FFI_SRC%\test.c /I"." /Fo"test_stdcall.obj"
%_CC% /Gr %FFI_SRC%\test.c /I"." /Fo"test_fastcall.obj"
%_LINK% /DLL /OUT:test_cdecl.dll test_cdecl.obj
%_LINK% /DLL /OUT:test_stdcall.dll test_stdcall.obj
%_LINK% /DLL /OUT:test_fastcall.dll test_fastcall.obj
if exist test_cdecl.dll.manifest^
    %_MT% -manifest test_cdecl.dll.manifest -outputresource:"test_cdecl.dll;2"
if exist test_stdcall.dll.manifest^
    %_MT% -manifest test_stdcall.dll.manifest -outputresource:"test_stdcall.dll;2"
if exist test_fastcall.dll.manifest^
    %_MT% -manifest test_fastcall.dll.manifest -outputresource:"test_fastcall.dll;2"
%LUA_EXE% %FFI_SRC%\test.lua
@goto :CLEAN_OBJ

:CLEAN_OBJ
del *.obj *.manifest *.ilk *.pdb

@goto :END

:FAIL
@echo You must open a "Visual Studio .NET Command Prompt" to run this script, please run make.bat at first.
pause

:END

