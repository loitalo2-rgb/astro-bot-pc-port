# Astro Bot PC Port & Emulation Research

Repositório dedicado ao estudo, engenharia reversa, emulação e prototipagem de suporte para execução de jogos de PlayStation 5 (com foco especial em **Astro Bot** e ferramentas correlatas) em ambiente PC/Windows.

## 🚀 Sobre o Projeto
Este projeto reúne ferramentas de desenvolvimento, parsers de assets (como formatos `.jxmd` e `.gnf`), protótipos de backend gráfico em **Vulkan**, emulação de kernel/chamadas do sistema, gerenciamento de patches de tradução/configuração e relatórios técnicos aprofundados sobre a arquitetura dos jogos de PS5 no PC.

## 📂 Estrutura do Repositório
- `src/`: Código-fonte em C++ para carregamento de executáveis (`eboot.bin`), recompiler de shaders, backend Vulkan, gerenciamento de áudio, kernel e assets.
- `config/`: Arquivos de configuração, INI e XMLs de patches para jogos específicos (como *Astro Bot* e *inFAMOUS Second Son*).
- `Relatorio_Completo_AstroBot_PC.html` (.pdf): Documentação técnica completa sobre o projeto de porte, atualizada a cada etapa.
- `RELATORIO_PORTE_ASTRO_BOT_ANYPS5.md` (.html/.pdf): Registro histórico das etapas anteriores.

## 🧱 Estado Atual (12 de novembro de 2026)
- **Build:** `cmake --build build/clang` (Windows, Clang/LLVM).
- **Testes:** `ctest --test-dir build/clang --output-on-failure` → **12/12**.
- **Loader:** inspeção SELF e reconstrução SELF→ELF em memória, fail-closed. Nenhum payload é descriptografado e nenhum código do jogo é executado.
- **Assets:** censo offline (166.717 arquivos, 148,83 GiB, 0 ilegíveis), validação de 28.949 GNF e manifesto de corpus com 71 representantes (sem versionar bytes do jogo).
- **Gráficos:** gravador de comandos próprio `AGCR` (validação fail-closed, replay offline e dump via `ASTRO_CMD_DUMP`).
- **Imports/NIDs:** catálogo offline do eboot real — 1.747 imports, 1.747 NIDs únicos, 60 módulos, 63 bibliotecas SCE; **147 conhecidos (8,4%)**, 48 com stub nativo e **1.600 sem mapeamento**. O plano de vinculação aponta **1.228 de 2.930 slots simbólicos vinculáveis (41,9%)** e recusa a aplicação fail-closed. Bloqueio seguinte: ponte de ABI SysV→Win64. Ver as seções correspondentes no relatório.

## 🛠️ Ferramentas de diagnóstico
- `diag_eboot <arquivo> [saida.elf]`: inspeção SELF e reconstrução para análise.
- `diag_asset_census <pasta>`: censo de assets por formato.
- `diag_gnf_validate <pasta>`: validação de layout GNF e manifesto de corpus.
- `diag_command_stream <arquivo.agcr> [--dump saida.txt]`: análise de streams AGCR (`--demo` grava um stream sintético).
- `diag_imports <eboot.bin|imagem.elf> [--bind] [--list-missing N]`: catálogo de imports/NIDs, cobertura do resolvedor e viabilidade de vinculação (binding).

## 🔌 Camada de vinculação e ABI
- **ABI:** ponte SysV AMD64 → Microsoft x64 por thunks gerados em memória executável, cobrindo até 6 argumentos inteiros/ponteiros, com cache por alvo. `NidResolver::GetCallableStub` devolve o endereço chamável pelo convidado. Limites: sem float misto, variádicos, 7+ argumentos ou retorno de struct.
- **Binding:** planejador que classifica cada relocação (RELATIVE, 64, GLOB_DAT, JUMP_SLOT) e aplica de forma atômica somente quando todos os slots têm stub. Hoje 1.228 de 2.930 slots são vinculáveis; a aplicação é recusada por design.

## ⚠️ Limites
A execução do jogo continua inviável e é recusada por design: faltam 1.600 NIDs mapeados, semântica de kernel/libc/GPU para os stubs, cobertura de assinaturas mais complexas na ponte de ABI e um runtime de CPU/ABI/GNM. O projeto não contorna criptografia, DRM ou controles de acesso do console. Nenhum byte do jogo é versionado.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
