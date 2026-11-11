@echo off
set EDGE="C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
set HTML="%~dp0Relatorio_Completo_AstroBot_PC.html"
set PDF="%~dp0Relatorio_Completo_AstroBot_PC.pdf"

echo Gerando PDF a partir do HTML...
%EDGE% --headless --disable-gpu --run-all-compositor-stages-before-draw --no-pdf-header-footer --print-to-pdf=%PDF% %HTML%

if exist %PDF% (
    echo [SUCESSO] PDF gerado em: %PDF%
) else (
    echo [ERRO] Falha ao gerar o PDF.
)
