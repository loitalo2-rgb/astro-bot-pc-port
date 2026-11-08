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
- **Correções:** Resolvido o crash de memória (heap mirror) que impedia o carregamento do jogo no Windows.
- **Transição de Abordagem:** Migração iniciada de uma emulação HLE (`KernelEmulator`) para uma abordagem de **Port Nativo (Shims/Hooks)** inspirada no `dkb-pc-recomp`.
- **Desenvolvimento do Injetor:** Criado o arquivo `src/injector_main.cpp` contendo lógica de *Pattern Scanning* para varrer o `eboot.bin` e interceptar chamadas I/O (`sceKernelOpen`, `sceKernelRead`) traduzindo-as diretamente para a Win32 API.

## Onde Paramos
- O script `src/injector_main.cpp` foi gerado e integrado ao projeto.
- Foram implementados os shims baseados em Win32 (`CreateFileA`, `ReadFile`) para eliminar dependências pesadas de C runtime (`FILE*`) e abstrações lentas de emulação.
- Sincronização e commit de documentação no repositório GitHub realizados.

## Próximos Passos
1. **Compilar e Testar o Injetor:** Compilar o `src/injector_main.cpp` com Clang/GCC para obter os offsets exatos no `eboot.bin` do PS5 em tempo de execução.
2. **Integração MinHook:** Adicionar a biblioteca `MinHook` para aplicar o desvio de memória (*hooking*) direto durante a execução nativa.
3. **Decodificação de Áudio:** Integrar o decodificador `libatrac9` no pipeline de áudio.

[[status-emulacao-astro-bot]]
[[Relatorio_Completo_AstroBot_PC.html]]
