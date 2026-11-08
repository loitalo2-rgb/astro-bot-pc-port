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
- **Implementação da Dispatch Table:** Criada a estrutura `SyscallDispatchTable` (`src/core/dispatch_table.h` e `.cpp`).
- **Integração no Motor Principal:**
  - `src/main.cpp` atualizado para incluir `<core/dispatch_table.h>` e invocar `InitializeDispatchTable()` na inicialização do aplicativo.
  - `CMakeLists.txt` atualizado para compilar e incluir `src/core/dispatch_table.cpp` dentro da biblioteca estática `AstroBotCore`.

## Onde Paramos
- A Dispatch Table agora é oficialmente inicializada como a primeira etapa de bootstrapping do aplicativo no `main.cpp`.
- O build CMake está configurado e integrado para garantir a compilação limpa em todas as plataformas target.

## Próximos Passos
1. **Substituição de Chamadas legadas:** Mapear os pontos de entrada no `eboot_loader.cpp` para invocarem `g_Dispatch` diretamente em vez das chamadas do `KernelEmulator`.
2. **Integração Vulkan-Memory:** Ligar `g_Dispatch.SceGpuCreateBuffer` aos heaps e buffers `VkBuffer` do `vulkan_backend.cpp`.

[[status-emulacao-astro-bot]]
[[Relatorio_Completo_AstroBot_PC.html]]
