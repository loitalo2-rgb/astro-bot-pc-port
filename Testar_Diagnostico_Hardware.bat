@echo off
title ASTRO BOT PC — Diagnóstico de Hardware e Pipeline
cd /d "%~dp0build\clang"
echo ========================================================
echo   Executando Teste de Diagnostico do ASTRO BOT PC...
echo ========================================================
echo.
test_pipeline.exe
echo.
pause
