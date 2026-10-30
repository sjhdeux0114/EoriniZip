@echo off
setlocal
if not defined KZ_BUILD_DIR set "KZ_BUILD_DIR=build"
if exist ".tmp\cargo\bin\cargo.exe" (
  set "CARGO_HOME=%CD%\.tmp\cargo"
  set "RUSTUP_HOME=%CD%\.tmp\rustup"
  set "PATH=%CD%\.tmp\cargo\bin;%PATH%"
)
where cargo >nul 2>nul
if errorlevel 1 (
  echo Rust 1.88 or later is required to build ALZ and EGG engines.
  exit /b 1
)
set "RUSTFLAGS=-C target-feature=+crt-static"
set "CARGO_TARGET_DIR=%CD%\build-cargo"
cargo build --locked --release --manifest-path third_party\unegg\Cargo.toml
if errorlevel 1 exit /b 1
cargo build --locked --release --manifest-path third_party\unalz\Cargo.toml
if errorlevel 1 exit /b 1
cargo build --locked --release --manifest-path third_party\easycrab\Cargo.toml
if errorlevel 1 exit /b 1
copy /y build-cargo\release\easycrab.exe "%KZ_BUILD_DIR%\eorinizip-easycrab.exe" >nul
if errorlevel 1 exit /b 1
cl /nologo /O2 /EHsc /MT /DNOJIT third_party\zpaq\zpaq.cpp third_party\zpaq\libzpaq.cpp /Fo:%KZ_BUILD_DIR%\ /Fe:%KZ_BUILD_DIR%\eorinizip-zpaq.exe /link advapi32.lib
if errorlevel 1 exit /b 1
copy /y build-cargo\release\unegg.exe "%KZ_BUILD_DIR%\eorinizip-unegg.exe" >nul
if errorlevel 1 exit /b 1
copy /y build-cargo\release\unalz.exe "%KZ_BUILD_DIR%\eorinizip-unalz.exe" >nul
exit /b %errorlevel%
