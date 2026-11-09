---
name: estado-projeto-atual
description: Relatório de status e progresso do projeto de port de Astro Bot.
metadata:
  type: project
---

# Estado do Projeto: Port Astro Bot para PC (08/11/2026)

## Progresso Atual
- **Infraestrutura e Compilação:** Repositório Git sincronizado. Binário `AstroBotPC.exe` compilado com Clang/Ninja.
- **Testes Unitários:** 100% dos testes da suíte CTest aprovados (7/7):
  - `elf_metadata_validation`
  - `self_inspection_validation`
  - `self_reconstruction_validation`
  - `at9_decoder_validation`
  - `gameplay_movement_validation`
  - `level_collision_validation`
  - `software_rasterizer_validation`
- **Diagnóstico do Eboot real (`eboot.bin` - PPSA21567):** Inspecionados com sucesso os 11 registros do container SELF, extração de metadados ELF64 e validação da tabela de relocations e PLT/JMPREL.
- **Subsistemas Operacionais:** Dispatch Table, NID Resolver, Rasterizador Software com depth buffer, Colisão com JXMD e Decodificador ATRAC9 validados.

## Onde Paramos
- Todo o framework base de emulação e os testes unitários encontram-se verdes e validados com os assets reais de Astro Bot.

## Próximos Passos
1. **Expansão de Stubs de Sistema:** Mapear e implementar stubs adicionais de chamadas `sce...` identificadas no binário do jogo.
2. **Integração Completa do Loop de Runtime:** Conectar os fluxos de áudio WASAPI e renderização em cenários reais de gameplay.

[[status-emulacao-astro-bot]]
[[Relatorio_Completo_AstroBot_PC.html]]
