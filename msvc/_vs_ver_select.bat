@rem Look for Visual Studio 2010 
@if not defined VS100COMNTOOLS goto select-2008
@set CUR_MSVS_TOOL=%VS100COMNTOOLS%
@set CUR_MSVS_VERSION=2010
@if not exist "%CUR_MSVS_TOOL%..\..\vc\vcvarsall.bat" goto select-2008
@goto RUN

:select-2008 
@rem Look for Visual Studio 2008 
@if not defined VS90COMNTOOLS goto select-2005
@set CUR_MSVS_TOOL=%VS90COMNTOOLS%
@set CUR_MSVS_VERSION=2008
@if not exist "%CUR_MSVS_TOOL%..\..\vc\vcvarsall.bat" goto select-2005
@goto RUN

:select-2005 
@rem Look for Visual Studio 2005 
@if not defined VS80COMNTOOLS goto notfound
@set CUR_MSVS_TOOL=%VS80COMNTOOLS%
@set CUR_MSVS_VERSION=2005
@if not exist "%CUR_MSVS_TOOL%..\..\vc\vcvarsall.bat" goto notfound
@goto RUN

:notfound
@echo Can not find VS 2005, 2008, or 2010 on your PC.

:RUN
@echo Start to build by using VS%CUR_MSVS_VERSION%.
@call  "%CUR_MSVS_TOOL%..\..\vc\vcvarsall.bat"