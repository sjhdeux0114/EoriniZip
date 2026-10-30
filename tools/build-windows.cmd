@echo off
setlocal
cd /d "%~dp0.."
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "KZ_VS=%%i"
if not defined KZ_VS (
  echo Visual Studio C++ build tools are required.
  exit /b 1
)
call "%KZ_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if defined KZ_SDK_INCLUDE set "INCLUDE=%KZ_SDK_INCLUDE%;%INCLUDE%"
if defined KZ_SDK_LIB set "LIB=%KZ_SDK_LIB%;%LIB%"
if defined KZ_SDK_BIN set "PATH=%KZ_SDK_BIN%;%PATH%"
if not exist build mkdir build
rc /nologo /I native /fo build\windows.res native\windows.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /DNOMINMAX /I native native\core.cpp native\process.cpp native\windows.cpp build\windows.res /Fo:build\ /Fe:build\EoriniZip.exe /link /SUBSYSTEM:WINDOWS comctl32.lib ole32.lib shell32.lib uuid.lib user32.lib gdi32.lib msimg32.lib advapi32.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /DNOMINMAX /I native native\core.cpp native\process.cpp tests\core_tests.cpp /Fo:build\ /Fe:build\core_tests.exe
if errorlevel 1 exit /b 1
build\core_tests.exe
exit /b %errorlevel%
