---
name: estado-projeto-atual
description: Relatório de status e progresso do projeto de port de Astro Bot.
metadata:
  type: project
---

# Estado do Projeto: Port Astro Bot para PC (08/11/2026)

## Progresso Atual
- **Infraestrutura:** Repositório Git sincronizado.
- **Integração de NIDs com a Dispatch Table:**
  - Atualizado `src/core/nid_resolver.cpp` para importar `<core/dispatch_table.h>`.
  - Conectadas as funções de I/O de kernel (`sceKernelOpen`, `sceKernelClose`, `sceKernelRead`, `sceKernelLseek`) diretamente para os ponteiros da `g_Dispatch`.
  - Conectada a chamada gráfica do AGC (`sceAgcCreateBuffer`) diretamente ao shim gráfico da `g_Dispatch`.

## Onde Paramos
- Os resolvedores de símbolos do jogo (`NidResolver`) agora delegam as chamadas de sistema diretamente para a nossa nova arquitetura de **Dispatch Table** e **Shims Nativos**.
- O sistema está unificado: toda chamada de I/O ou GPU solicitada pelo binário é roteada de forma limpa e modular.

## Próximos Passos
1. **Compilação e Validação do Boot:** Recompilar o projeto para garantir que as lambdas de ligação c-style compilem sem erros.
2. **Execução:** Rodar o executável para testar a inicialização unificada com a Dispatch Table ativa.

[[status-emulacao-astro-bot]]
[[Relatorio_Completo_AstroBot_PC.html]]
