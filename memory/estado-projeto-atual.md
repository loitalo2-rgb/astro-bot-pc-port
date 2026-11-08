---
name: estado-projeto-atual
description: Relatório de status e progresso do projeto de port de Astro Bot.
metadata:
  type: project
---

# Estado do Projeto: Port Astro Bot para PC (08/11/2026)

## Progresso Atual
- **Infraestrutura:** Repositório Git sincronizado.
- **Validação de Arquitetura:** Implementada a demonstração de *Despacho Local* com sucesso.
- **Implementação da Dispatch Table:** Criada a estrutura `SyscallDispatchTable` (`src/core/dispatch_table.h` e `.cpp`). Agora, o projeto possui um ponto centralizado para rotear chamadas de sistema (I/O e GPU) para *Shims* nativos, substituindo o antigo `KernelEmulator` monolítico por uma arquitetura de tabela de despacho modular.

## Onde Paramos
- A estrutura de dados (Dispatch Table) e a inicialização de *Shims* padrão estão prontas.
- O projeto está estruturado de forma que cada nova funcionalidade do jogo exija apenas a adição de um ponteiro na tabela e sua respectiva implementação de *Shim*.

## Próximos Passos
1. **Integração no Motor Principal:** Substituir os locais onde o jogo ainda invoca `KernelEmulator::SceKernel...` pela chamada via `g_Dispatch.SceKernel...`.
2. **Expansão de Shim Gráfico:** Implementar `Shim_GpuCreateBuffer` utilizando o pipeline Vulkan existente.
3. **Mapeamento de Assets:** Expandir a leitura dos arquivos `.jxm` utilizando a nova camada de I/O por despacho.

[[status-emulacao-astro-bot]]
[[Relatorio_Completo_AstroBot_PC.html]]
