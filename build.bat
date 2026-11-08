@echo off
:: ==============================================================
:: build.bat — Script de compilação ASTRO BOT PC Port
:: Requer: Visual Studio 2022 Build Tools OU LLVM/Clang + CMake
:: ==============================================================
setlocal enabledelayedexpansion
title ASTRO BOT PC — Build Script

echo.
echo ========================================================
echo   ASTRO BOT PC Port — Sistema de Build
echo ========================================================

:: Verifica se o CMake está disponível
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERRO] CMake nao encontrado. Instale em: https://cmake.org/download/
    echo.
    echo Alternativamente, instale via winget:
    echo   winget install Kitware.CMake
    pause
    exit /b 1
)

:: Cria diretório de build
if not exist build mkdir build
cd build

:: Gera os arquivos de projeto
echo [Build] Gerando projeto CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release

if %errorlevel% neq 0 (
    echo [ERRO] Falha na geracao do CMake!
    cd ..
    pause
    exit /b 1
)

:: Compila
echo [Build] Compilando em modo Release (x64)...
cmake --build . --config Release --parallel

if %errorlevel% neq 0 (
    echo [ERRO] Falha na compilacao!
    cd ..
    pause
    exit /b 1
)

cd ..
echo.
echo ========================================================
echo   Build concluida com sucesso!
echo   Executavel: build\Release\AstroBotPC.exe
echo ========================================================
echo.
pause
