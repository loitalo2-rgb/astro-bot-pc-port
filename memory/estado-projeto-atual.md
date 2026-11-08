---
name: estado-projeto-atual
description: Relatório de status e progresso do projeto de port de Astro Bot.
metadata:
  type: project
---

# Estado do Projeto: Port Astro Bot para PC (08/11/2026)

## Progresso Atual
- **Infraestrutura:** Repositório Git sincronizado.
- **Transição de Abordagem:** Migração total para abordagem de **Port Nativo (Shims/Hooks)** via injeção de código em tempo de execução.
- **Desenvolvimento do Injetor:** Atualizado `src/injector_main.cpp` com implementação manual de *JMP Hooks* (`VirtualProtect` + *absolute jump*). O injetor agora aponta para os offsets reais (`sceKernelOpen` @ `0x81a8a08`, `sceKernelRead` @ `0x82e55a9`) extraídos do `eboot.bin`.

## Onde Paramos
- Implementamos a lógica de desvio de execução (*hooking*) sem dependências externas.
- O injetor está pronto para ser compilado e executado, visando desviar chamadas originais do jogo para as funções nativas (`Shim_SceKernelOpen`, `Shim_SceKernelRead`).

## Próximos Passos
1. **Compilação do Injetor Final:** Compilar a nova versão do `injector_main.cpp` e verificar se não há violação de acesso ao memory hook.
2. **Teste de Interceptação:** Rodar o injetor junto ao jogo e verificar no log de console se as mensagens `[Shim] Interceptado` aparecem, confirmando o sucesso do *hooking*.
3. **Refinamento dos Shims:** Completar a lógica de tradução de arquivos dentro dos shims para garantir a leitura correta dos dados do jogo.

[[status-emulacao-astro-bot]]
[[Relatorio_Completo_AstroBot_PC.html]]
