---
name: estado-projeto-atual
description: Relatório de status e progresso do projeto de port de Astro Bot.
metadata:
  type: project
---

# Estado do Projeto: Port Astro Bot para PC (08/11/2026)

## Progresso Atual
- **Infraestrutura:** Repositório Git sincronizado no GitHub: [loitalo2-rgb/astro-bot-pc-port](https://github.com/loitalo2-rgb/astro-bot-pc-port).
- **Validação de Arquitetura de Port Nativo:** Implementada a demonstração segura de **Despacho Local via Tabela de Ponteiros (Function Pointer Swapping)** em `tests/hook_demo/local_dispatch_demo.cpp`.
- **Segurança de Execução:** Substituída a abordagem de injeção de memória remota (insegura/instável) por um modelo de inicialização de *Dispatch Table* interna, garantindo 100% de estabilidade sem risco de acesso indevido à memória (`VirtualProtectEx`/`WriteProcessMemory` eliminados).

## Onde Paramos
- O teste de despacho local foi compilado e executado com sucesso, demonstrando a transição fluida entre chamadas originais e *shims* baseados em Win32 (`CreateFileA`/`ReadFile`).
- O código de demonstração anterior (`injector.cpp` / `target.cpp`) foi descontinuado em favor do padrão seguro de compilação/linkagem nativa.

## Próximos Passos
1. **Estruturação da Tabela de Despacho (Dispatch Table):** Organizar o `src/common/types.h` ou um novo arquivo `src/core/dispatch_table.h` com todos os ponteiros de chamadas de sistema que o Astro Bot utiliza.
2. **Implementação do Shim de Vulkan/Graphics:** Mapear as chamadas de exibição e buffers de geometria utilizando a mesma estratégia de despacho local segura.
3. **Mapeamento de Assets:** Expandir a leitura dos arquivos `.jxm` utilizando a camada de I/O por despacho.

[[status-emulacao-astro-bot]]
[[Relatorio_Completo_AstroBot_PC.html]]
