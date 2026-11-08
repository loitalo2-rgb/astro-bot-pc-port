# Astro Bot PC Port & Emulation Research

Repositório dedicado ao estudo, engenharia reversa, emulação e prototipagem de suporte para execução de jogos de PlayStation 5 (com foco especial em **Astro Bot** e ferramentas correlatas) em ambiente PC/Windows.

## 🚀 Sobre o Projeto
Este projeto reúne ferramentas de desenvolvimento, parsers de assets (como formatos `.jxmd`), protótipos de backend gráfico em **Vulkan**, emulação de kernel/chamadas do sistema, gerenciamento de patches de tradução/configuração e relatórios técnicos aprofundados sobre a arquitetura dos jogos de PS5 no PC.

## 📂 Estrutura do Repositório
- `src/`: Código-fonte em C++ para carregamento de executáveis (`eboot.bin`), recompiler de shaders, backend Vulkan, gerenciamento de áudio, kernel e assets.
- `config/` e `build_clang/`: Arquivos de configuração, INI e XMLs de patches para jogos específicos (como *Astro Bot* e *inFAMOUS Second Son*).
- `RELATORIO_PORTE_ASTRO_BOT_ANYPS5.md` (.html/.pdf): Documentação técnica completa sobre o projeto de porte.
- `linkedin/`: Recursos visuais, capturas de tela e assets para demonstrações profissionais e portfólio.

---
🤖 Generated with [Claude Code](https://claude.com/claude-code)
