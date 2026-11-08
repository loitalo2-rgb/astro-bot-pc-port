---
name: estado-projeto-atual
description: Relatório de status e progresso do projeto de port de Astro Bot.
metadata:
  type: project
---

# Estado do Projeto: Port Astro Bot para PC (08/11/2026)

## Progresso Atual
- **Infraestrutura:** Repositório Git inicializado e sincronizado no GitHub: [loitalo2-rgb/astro-bot-pc-port](https://github.com/loitalo2-rgb/astro-bot-pc-port).
- **Core Engine:** Implementado motor básico de emulação (kernel, Vulkan backend, carregador de assets `.jxm`).
- **Correções:** Resolvido o crash de memória (heap mirror) que impedia o carregamento do jogo no Windows, documentado no relatório técnico.
- **Transição de Abordagem:** O projeto está migrando de uma emulação HLE (KernelEmulator) para uma abordagem de **Port Nativo (Shims/Hooks)**, inspirada pela arquitetura do `dkb-pc-recomp`.

## Onde Paramos
- Identificamos os símbolos das syscalls (`sceKernelOpen`, `sceKernelRead`, etc.) no `eboot.bin` do jogo.
- Definimos que o próximo passo crítico é a extração dos endereços de memória (offsets) dessas funções para a implementação dos **Hooks nativos**.
- A análise automática do binário de PS5 foi limitada pela natureza do formato do arquivo e restrições de ambiente; estamos na fase de preparação para extração técnica dos endereços.

## Próximos Passos
1. **Extração de Símbolos:** Executar o script de varredura (a ser gerado) para obter os offsets das funções principais (`sceKernel...`).
2. **Implementação de Hooks:** Criar o `main.cpp` injetor usando `MinHook` para redirecionar chamadas nativas do jogo para os seus *shims*.
3. **Decodificação de Áudio:** Integrar `libatrac9` conforme planejado.

[[status-emulacao-astro-bot]]
[[Relatorio_Completo_AstroBot_PC.html]]
