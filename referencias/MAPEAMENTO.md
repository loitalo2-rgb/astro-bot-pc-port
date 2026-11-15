# Referências úteis para o projeto de recombinação (PS5 → PC)

Dois projetos analisados e extraídos/clonados como referência:

1. **issn_pc** (port nativo de inFAMOUS Second Son, PS4 → Windows) — scripts extraídos em `referencias/issn-scripts/`
2. **dkb-pc-recomp** (recombinação estática de Donkey Kong Bananza, Switch 2 → Windows) — clonado em `dkb-pc-recomp/`

---

## 1. issn-scripts/ — o mais próximo do nosso caso (SELF x86-64 da Sony)

O PS4 e o PS5 usam SELF (ELF assinado) e x86-64 — mesma classe de problema do nosso projeto.

| Arquivo | O que faz | Por que importa aqui |
|---|---|---|
| `prepare.py` | Inspeciona SELF já em plaintext e prepara o probe de entry-point | Referência direta para o nosso `unwrap_self.py` / extração de ELF |
| `isa_rewrite.py` | Varre segmentos executáveis com iced-x86; encontra instruções ausentes em CPUs antigas (MOVBE, F16C, BMI1, LZCNT, SSE4A) e gera stubs | Reescrita x86 em imagem vinculada — mesmo problema que teremos se o eboot usar instruções fora do alvo |
| `link_libc.py` | Liga a libc convidado | Referência para resolver imports/símbolos do eboot |
| `link_modules.py` | Liga módulos do sistema | Mapeamento de imports de módulos do jogo |
| `guest_memory.py` | Layout de memória convidado | Endereçamento/mapeamento de memória do jogo |
| `patches.py` | Sistema de patches do launcher | Aplicar correções/patches sobre a imagem |
| `game_profiles.py` | Perfis por jogo/região (CUSA00223 etc.) | Estrutura de perfil por título |
| `content_profile.py` | Perfil de conteúdo | — |
| `graphics_options.py` | Opções gráficas do launcher | Integração de upscalers/AA no launcher |
| `save_backup.py` | Backup de saves (rotativo diário + pré-importação) | Sistema de backup reutilizável |
| `steam_shortcut.py` | "Adicionar à Steam" | Integração com Steam |
| `mods.py` | Sistema de mods | — |
| `game_update.py` | Atualizações do jogo | — |
| `windebug.py` | Depuração no Windows | — |
| `glyphs.py` / `infamous_glyphs.py` / `infamous_prompts.py` | Glyphs/prompts de controller | Ícones de botões (DualShock/Xbox) |
| `../issn-run.py` | Orquestrador: prepara imagem e inicia o probe (env vars, restart, shader seed) | Modelo de launcher/runner |
| `../issn-patches/*.xml` | Perfis de patch (inFAMOUS, GoW III) | Formato de perfil de patch |

**Destaques para estudar primeiro:** `prepare.py`, `isa_rewrite.py`, `link_libc.py`, `guest_memory.py`.

---

## 2. dkb-pc-recomp/ — arquitetura de recombinação estática + host Vulkan

Mesmo sendo ARM64 (Switch 2), a arquitetura e o processo são referência de alto nível.

### Documentação (ler primeiro)
| Arquivo | Conteúdo |
|---|---|
| `docs/architecture.md` | Arquitetura completa (compartilhada com TotK Recomp) |
| `docs/bananza-plan.md` | Plano fase a fase (F0 CPU → F5 Playable) |
| `docs/devlog.md` | Log diário de progresso — como planejar e reportar |
| `docs/bananza-shaders.md` | Trabalho de shaders NVN 2 |
| `docs/building.md` | Toolchain (MSYS2 UCRT64 Clang, Ninja, Python 3.12, .NET) |
| `docs/dkb-testing.md` | Processo de teste |
| `docs/maxwell.md` | Encodings Maxwell (tradução de shaders) |
| `AGENTS.md` / `CLAUDE.md` | Instruções para agentes de IA no repo — modelo de organização |

### Runtime — camada host (análogo ao nosso `src/kernel`, `src/gpu`)
| Arquivo | Análogo no nosso projeto |
|---|---|
| `runtime/kernel.c` (93 KB) + `kernel.h` | Nossa camada de kernel/serviços |
| `runtime/loader.c` | Carregamento da imagem do jogo |
| `runtime/memory.c` | Gerenciamento de memória convidado |
| `runtime/fs_host.c` (85 KB) + `fs.h` | Sistema de arquivos (RomFS → host) |
| `runtime/input.c`, `input_map.c`, `input_sdl.c`, `input_win.c` | Nossa camada `src/input` |
| `runtime/audio_mixer.c`, `audren.c` | Áudio |
| `runtime/boot.c`, `main.c` | Boot/main do port |
| `runtime/cfg.c`, `cfg_probe.c` | Configurações |
| `runtime/ov_*.c` (overlay: system, input, gfx, movie, media) | Overlay/menus |
| `runtime/frame_rate.c`, `perf.c`, `perfmon.c` | Controle de FPS/perf |

### Runtime — gráficos NVN 2 → Vulkan (análogo ao nosso `src/gpu`, GNM → Vulkan)
| Arquivo | Função |
|---|---|
| `runtime/nvn.h` (88 KB) | Definições NVN |
| `runtime/nvn_api_cmd.c`, `nvn_api_core.c`, `nvn_api_obj.c`, `nvn_api_nvn2.c` | Front-end da API do console |
| `runtime/nvn_procs.inc`, `nvn_procs_nvn2.inc` | Tabela de procedimentos (dispatch) |
| `runtime/nvn_probe.c` | Probes da API |
| `runtime/vk_backend.c`, `vk_device.c`, `vk_memory.c`, `vk_pipeline.c` (228 KB!), `vk_exec*.c`, `vk_texture.c`, `vk_sync.c` | Backend Vulkan completo — referência para o nosso backend |
| `runtime/gfx_shader.c` (148 KB) | Pipeline de shaders |
| `runtime/vk_fsr.c`, `fsr_easu_spv.h`, `fsr_rcas_spv.h` | FSR embutido |
| `runtime/tex_astc.c`, `tex_deswizzle.c` | Decodificação/deswizzle de texturas do console |
| `runtime/gfx_window_win.c`, `gfx_window_sdl.c` | Janela (Win32/SDL) |

### recomp/ — gerador de código (menos relevante para x86-64, mas bom de estudar)
| Arquivo | Função |
|---|---|
| `recomp/lifter.py` | Lifts ARM64 → C (não se aplica a PS5, mas mostra a técnica) |
| `recomp/gen.py` | Gera o código C a partir dos módulos |
| `recomp/patches.py` + `patches-*.txt` | Sistema de patches por versão do jogo |
| `recomp/jumptables.py`, `simd_c.py`, `regs.py` | Tabelas de salto, SIMD, registradores |

### tools/ — ferramentas de build e teste
| Arquivo | Função |
|---|---|
| `tools/build_game.py` | Build completo do jogo |
| `tools/extract.py` | Extração dos arquivos do dump |
| `tools/prepare_shaders.py`, `pack_shader_cache.py` | Preparação/cache de shaders (modelo para o nosso shader seed) |
| `tools/make_release.py` | Empacotamento da release |
| `tools/check_content.py` | Guarda de conteúdo (nada do jogo no repo) — CI |
| `tools/run_game.py` | Execução |

### tests/ — testes (modelo de processo)
- Testes diferenciais (C vs referência), testes de ABI, testes de geometria/shaders
- `tests/diff/` — testes diferenciais contra emulador de referência (Unicorn)

---

## 3. Licenças (obrigatório respeitar ao reusar código)

- **issn_pc**: GPL-2.0-or-later
- **dkb-pc-recomp**: GPL-3.0-or-later (partes de TotK Recomp: MIT)

Ao reusar trechos: manter licença e atribuição. Nenhum arquivo de jogo em nenhum dos projetos — seguir a mesma regra no nosso.

---

## 4. Próximos passos sugeridos

1. Ler `dkb-pc-recomp/docs/architecture.md` e `bananza-plan.md` (modelo de plano para o nosso projeto)
2. Estudar `referencias/issn-scripts/prepare.py` + `isa_rewrite.py` junto com o nosso `AnyPS5/unwrap_self.py`
3. Comparar `runtime/kernel.c` + `loader.c` com o nosso `src/kernel` e `src/main.cpp`
4. Estudar `runtime/nvn_api_*.c` + `vk_*.c` como referência para traduzir GNM → Vulkan no nosso `src/gpu`
5. Adotar o padrão de testes de `dkb-pc-recomp/tests/`
