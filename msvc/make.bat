
@if "%1"=="debug" goto :RUN
@if "%1"=="release" goto :RUN
@if "%1"=="test" goto :RUN
@if "%1"=="clean" goto :CLEAN

@echo make debug	: build ffi.dll (debug version)
@echo make release 	: build ffi.dll (release version)
@echo make test 	: build test and run test case.
@echo make clean	: clean the generated files.
@goto END

:RUN
@call _vs_ver_select.bat
@call _ffi_build.bat %1
@goto END

:CLEAN
call clean.bat
@goto END

:END
@pause
