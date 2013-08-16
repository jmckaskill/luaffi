@set FFI_SRC=..
@set DSM_DIR=%FFI_SRC%\dynasm
@set DYNASM_LUA=%DSM_DIR%\dynasm.lua

lua.exe %DYNASM_LUA% -LNE -D X32WIN -o call_x86.h %FFI_SRC%\call_x86.dasc
lua.exe %DYNASM_LUA% -LNE -D X64 -o call_x64.h %FFI_SRC%\call_x86.dasc
lua.exe %DYNASM_LUA% -LNE -D X64 -D X64WIN -o call_x64win.h %FFI_SRC%\call_x86.dasc
lua.exe %DYNASM_LUA% -LNE -o call_arm.h %FFI_SRC%\call_arm.dasc
