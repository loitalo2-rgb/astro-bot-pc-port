@echo off
setlocal
title AnyPS5 Relinker — ASTRO BOT PPSA21567
rem ============================================================
rem  Pipeline validado em 02/11/2026:
rem   1. SELF (dump descriptografado) -> ELF via unwrap_self.py
rem      (tool MIT de boykopovar/AnyPS5; recusa containers criptografados)
rem   2. ELF + sce_module -> AstroBot.exe (Windows PE) via relinker.exe
rem      (AnyPS5 v0.1.4, GPL-2.0, hash verificado na ocorrencia do download)
rem  Este script nao descriptografa nada: o dump local ja esta no
rem  formato "AnyPS5 Ready" (segmentos nao criptografados, verificado
rem  pela checagem de entropia do unwrap em 02/11/2026).
rem ============================================================

set "DUMP=D:\Games\abaixar\jogo\PPSA21567"
set "BASE=%~dp0"
set "WORK=%BASE%work"
set "OUT=%BASE%out"

echo.
echo [1/4] Preparando diretorios...
if not exist "%WORK%" mkdir "%WORK%"
if not exist "%OUT%" mkdir "%OUT%"

echo [2/4] Desembrulhando SELF para ELF (eboot + modulos)...
py -3 "%BASE%unwrap_self.py" "%DUMP%\eboot.bin" "%WORK%\eboot_unwrapped.elf"
if errorlevel 1 (echo Falha no unwrap do eboot.bin & exit /b 1)

if not exist "%WORK%\sce_module" mkdir "%WORK%\sce_module"
if exist "%DUMP%\sce_module" (
    for %%F in ("%DUMP%\sce_module\*.prx") do (
        py -3 "%BASE%unwrap_self.py" "%%F" "%WORK%\sce_module\%%~nxF"
        if errorlevel 1 (echo Falha ao desembrulhar %%~nxF & exit /b 1)
    )
)

echo [3/4] Convertendo (ELF - PE Windows)...
"%BASE%relinker.exe" --windows --windows-diagnostics --sce-module-path "%WORK%" "%WORK%\eboot_unwrapped.elf" "%OUT%\AstroBot.exe"
if errorlevel 1 (echo Conversao falhou & exit /b 1)

echo [4/4] Concluido.
echo.
echo Layout para executar (bibliotecas de sistema ainda sao necessarias em libs/):
echo   %OUT%\AstroBot.exe
echo   %OUT%\libs\      *.prx (bibliotecas de sistema do AnyPS5/PortPS5)
echo   %OUT%\app0\      dados do jogo: %DUMP%\data, %DUMP%\sce_sys, eboot.bin original
echo.
echo IMPORTANTE: o Windows Defender pode isolar AstroBot.exe como falso
echo positivo (PE nao assinado auto-gerado). Se bloqueado, restaure o
echo arquivo da quarentena ou crie uma excecao ANTES de executar.
pause
