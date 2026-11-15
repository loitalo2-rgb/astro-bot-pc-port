@echo off
title Astro Bot PC - Running
cd /d "D:\Jogos ps4\AstroHybrid\AstroBotPC-AstroHybrid-1.0.9\Build\RunHybrid"

set APS5_GPU_ENGINE=kyty
set "KYTY_BARRIER_BATCH="
set "KYTY_DISPATCH_OVERLAP="
set "KYTY_WAVE64_WARPS="
set "ASTROHYBRID_COMMIT_EPOCH_FASTPATH=0"
set "KYTY_BUFFER_RANGE_FIXES=1"
set KYTY_HANG_WATCHDOG=0
set ASTROHYBRID_CACHE_TITLE_ID=PPSA21567
set KYTY_GPU_HOST_MODULE=astrohybrid_host.dll
set APS5_PAD_TRIGGER_STATE=0
set APS5_SAVEDATA_TRANSFER_FALLBACK=1
set ASTROHYBRID_PAD_MOTION=1
set ASTROHYBRID_SHADER_CACHE=1
set ASTRO_UPSCALER_BACKEND=fsr41
set ASTRO_UPSCALER_QUALITY=performance
set ASTRO_FRAME_GENERATION=1
set KYTY_CP_PRIORITY=2
set KYTY_DRAW_PREP_PRIORITY=2
set KYTY_DRAW_PREP_SPIN_US=10
set KYTY_DRAW_PREP_COLD_SPIN_US=5
set KYTY_GPU_WIDTH=2560
set KYTY_GPU_HEIGHT=1440

if exist "u59-preset.cmd" call "u59-preset.cmd"

echo ========================================================
echo   Iniciando Astro Bot PC (2560x1440)
echo   Upscaler: FSR41 (PERFORMANCE) ^| FrameGen: 1
echo   GPU: AMD Radeon RX 9060 XT (16 GB VRAM)
echo ========================================================
AstroBot.exe -perfDisplay false %*
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo O jogo encerrou com codigo %ERRORLEVEL%
    pause
)
