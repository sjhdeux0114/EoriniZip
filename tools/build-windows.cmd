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
if not defined KZ_BUILD_DIR set "KZ_BUILD_DIR=build"
if not exist "%KZ_BUILD_DIR%" mkdir "%KZ_BUILD_DIR%"
call tools\build-formats.cmd
if errorlevel 1 exit /b 1
rc /nologo /I native /fo %KZ_BUILD_DIR%\windows.res native\windows.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /DNOMINMAX /I native native\core.cpp native\process.cpp native\windows.cpp %KZ_BUILD_DIR%\windows.res /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\EoriniZip.exe /link /SUBSYSTEM:WINDOWS comctl32.lib ole32.lib shell32.lib uuid.lib user32.lib gdi32.lib msimg32.lib advapi32.lib comdlg32.lib gdiplus.lib normaliz.lib ole32.lib oleaut32.lib uuid.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /DNOMINMAX /I native native\core.cpp native\process.cpp tests\core_tests.cpp /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\core_tests.exe /link normaliz.lib ole32.lib oleaut32.lib uuid.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /DNOMINMAX /I native native\core.cpp native\process.cpp tests\split_tests.cpp /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\split_tests.exe /link normaliz.lib ole32.lib oleaut32.lib uuid.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /LD /DUNICODE /D_UNICODE /DNOMINMAX /I native native\shell_menu.cpp /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\EoriniZipShell_v051.dll /link /DEF:native\shell_menu.def gdi32.lib shell32.lib ole32.lib user32.lib uuid.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /LD /DUNICODE /D_UNICODE /DNOMINMAX /I native native\modern_shell.cpp /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\EoriniZipModernShell_v051.dll /link /DEF:native\modern_shell.def shell32.lib ole32.lib user32.lib uuid.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /DNOMINMAX tests\shell_menu_tests.cpp /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\shell_menu_tests.exe /link shell32.lib ole32.lib user32.lib uuid.lib
if errorlevel 1 exit /b 1
%KZ_BUILD_DIR%\shell_menu_tests.exe "%CD%\%KZ_BUILD_DIR%\EoriniZipShell_v051.dll"
if errorlevel 1 exit /b 1
%KZ_BUILD_DIR%\core_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /DNOMINMAX /I native tests\windows_password_tests.cpp /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\windows_password_tests.exe /link advapi32.lib
if errorlevel 1 exit /b 1
%KZ_BUILD_DIR%\windows_password_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /DNOMINMAX /I native tests\windows_clipboard_tests.cpp /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\windows_clipboard_tests.exe /link user32.lib shell32.lib
if errorlevel 1 exit /b 1
%KZ_BUILD_DIR%\windows_clipboard_tests.exe
if errorlevel 1 exit /b 1
%KZ_BUILD_DIR%\windows_clipboard_tests.exe --integration
if errorlevel 78 exit /b 1
if errorlevel 77 (echo Private clipboard integration skipped: isolated station unavailable.) else if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /DNOMINMAX tests\modern_shell_tests.cpp /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\modern_shell_tests.exe /link shell32.lib ole32.lib user32.lib uuid.lib
if errorlevel 1 exit /b 1
%KZ_BUILD_DIR%\modern_shell_tests.exe "%CD%\%KZ_BUILD_DIR%\EoriniZipModernShell_v051.dll"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /I native tests\dialog_resource_tests.cpp /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\dialog_resource_tests.exe
if errorlevel 1 exit /b 1
%KZ_BUILD_DIR%\dialog_resource_tests.exe "%CD%\%KZ_BUILD_DIR%\EoriniZip.exe"
exit /b %errorlevel%
