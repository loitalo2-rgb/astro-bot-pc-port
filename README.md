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
- **Testes:** `ctest --test-dir build/clang --output-on-failure` → **10/10**.
- **Loader:** inspeção SELF e reconstrução SELF→ELF em memória, fail-closed. Nenhum payload é descriptografado e nenhum código do jogo é executado.
- **Assets:** censo offline (166.717 arquivos, 148,83 GiB, 0 ilegíveis), validação de 28.949 GNF e manifesto de corpus com 71 representantes (sem versionar bytes do jogo).
- **Gráficos:** gravador de comandos próprio `AGCR` (validação fail-closed, replay offline e dump via `ASTRO_CMD_DUMP`).
- **Imports/NIDs:** catálogo offline do eboot real — 1.747 imports, 1.747 NIDs únicos, 60 módulos, 63 bibliotecas SCE; **71 conhecidos (4,1%)**, 46 com stub nativo e **1.676 sem mapeamento**. Ver a seção correspondente no relatório.

## 🛠️ Ferramentas de diagnóstico
- `diag_eboot <arquivo> [saida.elf]`: inspeção SELF e reconstrução para análise.
- `diag_asset_census <pasta>`: censo de assets por formato.
- `diag_gnf_validate <pasta>`: validação de layout GNF e manifesto de corpus.
- `diag_command_stream <arquivo.agcr> [--dump saida.txt]`: análise de streams AGCR (`--demo` grava um stream sintético).
- `diag_imports <eboot.bin|imagem.elf> [--list-missing N]`: catálogo de imports/NIDs e cobertura do resolvedor.

## ⚠️ Limites
A execução do jogo continua inviável e é recusada por design: faltam 1.676 NIDs mapeados, semântica de kernel/libc/GPU para os stubs e um runtime de CPU/ABI/GNM. O projeto não contorna criptografia, DRM ou controles de acesso do console. Nenhum byte do jogo é versionado.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
