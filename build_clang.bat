@echo off
setlocal
title ASTRO BOT PC - Clang Build

set "CLANGXX=C:\Users\italo\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261006-ucrt-x86_64\bin\x86_64-w64-mingw32-clang++.exe"
if not exist "%CLANGXX%" set "CLANGXX=clang++.exe"
for %%I in ("%CLANGXX%") do set "CLANG_DIR=%%~dpI"
set "CLANG_C=%CLANG_DIR%clang.exe"
if not exist "%CLANG_C%" set "CLANG_C=clang.exe"

set "BUILD_DIR=build\clang"

echo.
echo ================================================================
echo   ASTRO BOT PC - Build com Clang e CMake
echo ================================================================

cmake -S . -B "%BUILD_DIR%" -G Ninja ^
    "-DCMAKE_C_COMPILER=%CLANG_C%" ^
    "-DCMAKE_CXX_COMPILER=%CLANGXX%"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --target AstroBotPC benchmark_load test_pipeline test_audio_at9
if errorlevel 1 exit /b 1

ctest --test-dir "%BUILD_DIR%" --output-on-failure -R at9_decoder_validation
if errorlevel 1 exit /b 1

if not exist "%BUILD_DIR%\config\patches" mkdir "%BUILD_DIR%\config\patches"
copy /Y config\patches\*.* "%BUILD_DIR%\config\patches\" >nul 2>&1
if not exist "%BUILD_DIR%\savedata" mkdir "%BUILD_DIR%\savedata"
if not exist "%BUILD_DIR%\cache" mkdir "%BUILD_DIR%\cache"

echo.
echo ================================================================
echo   Build e validacao do decoder concluidos com sucesso!
echo   Executavel: %BUILD_DIR%\AstroBotPC.exe
echo ================================================================
echo.
