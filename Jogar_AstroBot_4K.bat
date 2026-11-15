@echo off
title Astro Bot PC - 4K Ultra HD (3840x2160)
set "HYBRID=D:\Jogos ps4\AstroHybrid\AstroBotPC-AstroHybrid-1.0.9"
cd /d "%HYBRID%\Build\RunHybrid"

set APS5_GPU_ENGINE=kyty
set "KYTY_BARRIER_BATCH="
set "KYTY_DISPATCH_OVERLAP="
set "KYTY_WAVE64_WARPS="
set "ASTROHYBRID_COMMIT_EPOCH_FASTPATH=0"
set "KYTY_BUFFER_RANGE_FIXES=1"
set KYTY_HANG_WATCHDOG=0
set ASTROHYBRID_CACHE_TITLE_ID=PPSA21567
set KYTY_GPU_HOST_MODULE=astrohybrid_host.dll
set APS5_PAD_TRIGGER_STATE=1
set APS5_SAVEDATA_TRANSFER_FALLBACK=1
set ASTROHYBRID_PAD_MOTION=1
set ASTROHYBRID_SHADER_CACHE=1
set KYTY_CP_PRIORITY=2
set KYTY_DRAW_PREP_PRIORITY=2
set KYTY_DRAW_PREP_SPIN_US=10
set KYTY_DRAW_PREP_COLD_SPIN_US=5
set KYTY_GPU_WIDTH=3840
set KYTY_GPU_HEIGHT=2160

if exist "u59-preset.cmd" call "u59-preset.cmd"

echo ========================================================
echo   Iniciando Astro Bot PC em 4K Ultra HD
echo   GPU: AMD Radeon RX 9060 XT (16 GB VRAM)
echo   Perfil: Fidelidade Visual Maxima
echo ========================================================
AstroBot.exe %*
pause
