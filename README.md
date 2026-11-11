# AstroPort — ASTRO BOT no PC (PPSA21567)

Projeto de porte/pesquisa do **ASTRO BOT (PS5, PPSA21567)** com pipeline próprio em
C++: inspeção e reconstrução de containers SELF, mapeamento de ELF64, relocations,
camada de runtime e renderização. Nada é executado sem que a estrutura do binário
seja entendida e validada — a política do projeto é **fail-closed** desde o início.

> Este repositório **não contém** dumps do jogo (nem `eboot.bin`, nem `*.prx`) por
> questões de direitos autorais. Todo diagnóstico usa apenas arquivos locais do
> próprio usuário, em modo somente-leitura.

## Estado atual do pipeline próprio

| Etapa | Estado |
|---|---|
| Leitura de ELF64 direto (metadados, PT_LOAD, PT_DYNAMIC) | ✅ implementado |
| Inspeção de container SELF (tabela de registros 0x40..0x400, flags, entropia) | ✅ implementado |
| Reconstrução SELF→ELF64 (dumps não criptografados, fail-closed) | ✅ implementado |
| Mapeamento da imagem virtual a partir dos PT_LOAD | ✅ implementado |
| Relocations `R_X86_64_RELATIVE` (tipo 8, sym 0) | ✅ implementado |
| Censo offline de assets (GNF/JXMD/SELF/ELF/SPIR-V) | ✅ implementado |
| Tabela central de formatos GNF→Vulkan (fail-closed p/ IDs desconhecidos) | ✅ implementado |
| Gravador de comandos AGCR + replay offline (sem GPU, sem GNM do convidado) | ✅ implementado |
| Resolução de imports / NIDs (≈2.930 referências pendentes) | ⏳ próxima etapa |
| Execução / runtime PS5 | ❌ recusada por design |

### Regras de reconstrução SELF→ELF (implementação própria)

Sem copiar código de terceiros (conceito absorvido do ecossistema AnyPS5/PortPS5,
implementação independente em C++):

1. **Registros cobertos** (flag `0x800`, índice em `flags >> 20`): bytes copiados
   verbatim da posição do registro no container; `p_offset` reapontado de forma
   congruente com `p_vaddr` módulo page-alignment.
2. **Segmentos internos não cobertos** (PT_GNU_EH_FRAME, PT_GNU_RELRO, PT_TLS,
   PT_NOTE/PT_DYNAMIC internos): resolvidos pela relação vaddr→arquivo do PT_LOAD
   que os contém (sem duplicação de dados).
3. **Segmentos restantes sem vaddr** após o último registro: copiados em sequência.
4. **Container que termina após o último registro**: segmentos de metadados
   restantes são emitidos sem bytes de arquivo (comportamento documentado); um
   `PT_LOAD` nessa situação é **recusado**.

**Recusas fail-closed:** flags de criptografia (`0x2`) ou compressão (`0x8`),
entropia ≥ 7,5 bits/byte em payload mapeado, tamanho de registro ≠ `p_filesz`,
índice de segmento inválido, ou qualquer estrutura não explicada pelas regras.

### Validação

- Suite de testes (CTest): **9/9 passando**, incluindo `self_inspection_validation`,
  `self_reconstruction_validation` (fixtures sintéticos + 5 casos de recusa),
  `asset_census_validation` (GNF/JXMD/SELF/ELF/SPIR-V, formatos mapeados e não mapeados,
  geometria de tile e layout de mips GNF) e `command_stream_validation`
  (gravação AGCR, round-trip binário, replay e 12 casos de recusa).
- Diagnóstico em dump real (somente leitura): `eboot.bin` de 259.312.583 bytes
  reconstruído em imagem ELF64 de **258.866.503 bytes**, 14 program headers
  (6 de registros, 6 via PT_LOAD, 1 trailing, 1 sem dados), auto-validado pelo
  próprio parser e mapeado em imagem virtual de **270.609.168 bytes**.
- Censo offline do dump (leitura apenas, cabeçalhos): **166.717 arquivos / 148,83 GiB**
  em 56 s; 28.949 texturas GNF (110,20 GiB, todas válidas) e 15.503 malhas JXMD
  (26,18 GiB, incluindo 170 arquivos `.jmt` reconhecidos pelo magic).
- Validação de layout GNF (leitura apenas): **28.949 arquivos em 9,5 s**, 0 cabeçalhos
  inválidos e 0 payloads passando do fim do container. Invariante confirmado em 100%
  do dump: o payload declarado nunca é menor que a soma linear dos mips. A regra exata
  de padding por formato ainda não é assumida; o loader só recusa o que viola o
  invariante. O validador gera um **manifesto de corpus com 71 representantes**
  (formato/tile/versão/mips + FNV-1a 64 dos primeiros 1 MiB), sem copiar bytes do jogo.

## Build

Requer CMake 3.20+, Ninja e LLVM-MinGW (Clang) no PATH — ou MSVC.

```bat
build_clang.bat
```

Ou manualmente:

```bat
cmake -S . -B build\clang -G Ninja -DCMAKE_C_COMPILER=clang.exe -DCMAKE_CXX_COMPILER=clang++.exe
cmake --build build\clang --target AstroBotPC
ctest --test-dir build\clang --output-on-failure
```

Ferramentas de diagnóstico:

```bat
build\clang\diag_eboot.exe <caminho\eboot.bin> [saida-reconstruida.elf]
```

O segundo argumento é opcional: sem ele, a reconstrução é feita apenas em memória
(análise), sem gravar nada.

```bat
build\clang\diag_census.exe <raiz-do-dump> [censo.csv]
```

O censo lê apenas os cabeçalhos (até 96 bytes por arquivo): classifica GNF, JXMD,
SELF, ELF64 e SPIR-V, agrega formatos/tamanhos e nunca decodifica payload. O CSV
opcional lista cada container reconhecido com <code>path,size,kind,valid,detail</code>.

```bat
build\clang\diag_gnf_validate.exe <raiz-do-dump> [manifesto-corpus.csv]
```

Valida a cadeia de mips e o tiling de cada GNF a partir do cabeçalho, compara o
payload declarado com três hipóteses de tamanho (linear, mips alinhados a 64 KiB e
total arredondado a 64 KiB) e relata arquivos cuja leitura passaria do fim do
container. O CSV opcional é um manifesto de corpus (caminho, formato, dimensões,
payload, estimativas e hash FNV-1a dos primeiros 1 MiB) — apenas metadados, nunca
bytes proprietários.

```bat
build\clang\diag_command_stream.exe <arquivo.agcr> [--dump saida.txt]
build\clang\diag_command_stream.exe --demo saida.agcr
```

Analisa um stream de comandos AGCR gravado pela porta: valida o arquivo e resume
frames, draws, índices, texturas e formatos GNF. Com `ASTRO_CMD_DUMP=<arquivo>`
o executável grava o stream no encerramento. `--demo` gera um stream sintético
(identificado como tal) para exercitar formato e validador sem GPU. O AGCR é o
formato neutro desta porta — não é GNM/AGC do PS5 e não afirma compatibilidade
binária com ele.

## Relatórios

- `Relatorio_Completo_AstroBot_PC.html` / `.pdf` — relatório canônico de progresso
  (todas as etapas, decisões e medições reais). O PDF é regerado com
  `generate_pdf.bat`.
- `RELATORIO_PORTE_ASTRO_BOT_ANYPS5.*` — experimento histórico com o ecossistema
  AnyPS5 (relinker/PRX) usado como referência, não como runtime.
- `Relatorio_Status_AstroBot_PC.*` e `RELATORIO_CONTINUIDADE_PERFORMANCE.pdf` —
  relatórios anteriores de status/performance.

## Terceiros e licenças

- **LibAtrac9** (MIT) — decodificação ATRAC9; ver `THIRD_PARTY_NOTICES.txt`.
- **`AnyPS5/unwrap_self.py`** — script do AnyPS5 (GPL-2.0), mantido **sem
  modificações** apenas como ferramenta externa reproduzível; notas de
  proveniência e licença em `AnyPS5/LICENCA_E_PROVENIENCIA.txt`. Nenhum código
  GPL é derivado no núcleo do AstroPort.
- O binário `relinker.exe` (GPL-2.0) **não é versionado**; foi validado por hash
  na release oficial e o procedimento está em `AnyPS5/Relinkar_AstroBot.bat`.

## Aviso

Este projeto é de pesquisa e engenharia reversa estrutural com fins educacionais.
Não distribui, descriptografa nem contorna proteções de conteúdo: a reconstrução
SELF→ELF só aceita containers **já descriptografados** e recusa qualquer payload
com marcação ou entropia de criptografia. Nenhum código do jogo é executado.

## Histórico (experimentos anteriores com AnyPS5)

Trabalho anterior documentado nos relatórios: conversão do dump para executável
Windows via relinker do AnyPS5 (2.930 imports NID analisados, 2 módulos PRX
convertidos), com crash de alocação de GPU resolvido e carregamento até a tela de
título em ambiente de teste. Essa linha foi mantida apenas como referência
técnica; o desenvolvimento ativo é o pipeline próprio descrito acima.
