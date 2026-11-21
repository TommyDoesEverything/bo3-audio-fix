@echo off
rem Builds build\winmm.dll (release) and build\debug\winmm.dll (logs mixer calls).
rem Needs Visual Studio's C++ tools (x64) and Python with pefile (pip install pefile).
setlocal
cd /d "%~dp0"

if defined VSCMD_ARG_TGT_ARCH goto :tools_ready
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`call "%%VSWHERE%%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (echo Visual Studio C++ tools not found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
:tools_ready

python tools\gen_exports.py || exit /b 1
python tools\gen_exports.py --debug || exit /b 1

call :build gen build "" || exit /b 1
call :build gen-debug build\debug "/DBO3FIX_DEBUG" || exit /b 1
echo Built build\winmm.dll and build\debug\winmm.dll
exit /b 0

:build
set GEN=build\%1
set OUT=%2
if not exist %OUT%\obj mkdir %OUT%\obj
ml64 /nologo /c /Fo%OUT%\obj\stubs.obj %GEN%\stubs.asm || exit /b 1
cl /nologo /O2 /W4 /MT /GS %~3 /I%GEN% /c /Fo%OUT%\obj\proxy.obj src\proxy.c || exit /b 1
link /nologo /DLL /OUT:%OUT%\winmm.dll /DEF:%GEN%\winmm.def /IMPLIB:%OUT%\obj\winmm.lib %OUT%\obj\proxy.obj %OUT%\obj\stubs.obj kernel32.lib || exit /b 1
exit /b 0
