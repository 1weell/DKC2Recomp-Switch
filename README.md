# DKC2Recomp — Nintendo Switch port

Port para Nintendo Switch do [DKC2Recomp](https://github.com/elliotttate/DKC2Recomp), baseado no recompilador estático e no runtime SNES do projeto original.

O objetivo deste repositório é manter somente o host, a configuração de build e a documentação necessários para executar o jogo no Nintendo Switch.

## Estado atual

A build foi testada em hardware real e apresenta:

- boot e execução do jogo;
- controles do Switch funcionando;
- mapeamento físico correto dos botões B/Y/A/X;
- áudio SNES via SDL/libnx;
- apresentação widescreen 16:9 em 1280×720;
- SRAM persistente em `.runtime/`.

A rota widescreen continua sendo validada tela a tela. Pequenos artefatos de transição na introdução podem permanecer em determinadas cenas.

## Requisitos

Para compilar, instale:

- [devkitPro](https://devkitpro.org/);
- devkitA64;
- libnx;
- SDL2 para Switch;
- CMake, Ninja, Python e Rust/Cargo;
- MSYS2 do devkitPro;
- uma ROM própria e legalmente obtida de *Donkey Kong Country 2: Diddy's Kong Quest*, versão North America v1.0.

O projeto espera:

```sh
export DEVKITPRO=/c/devkitPro
export DEVKITA64=/c/devkitPro/devkitA64
```

## ROM compatível

A ROM não é distribuída neste repositório. A geração verifica a revisão North America v1.0 pelo SHA-256:

```text
35421a9af9dd011b40b91f792192af9f99c93201d8d394026bdfb42cbf2d8633
```

Coloque a ROM na raiz do projeto ou informe o caminho diretamente ao gerador.

## Compilação

Execute os comandos a partir do shell MSYS2 do devkitPro:

```sh
export DEVKITPRO=/c/devkitPro
export DEVKITA64=/c/devkitPro/devkitA64

python3 scripts/generate_snesrecomp.py --rom /c/caminho/para/DKC2-USA-v1.0.sfc

cmake -S . -B build-switch -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/switch-devkitA64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DDKC2_BUILD_SNESRECOMP_SWITCH=ON

cmake --build build-switch --parallel 16
```

O artefato principal será gerado em:

```text
build-switch/DKC2RecompSwitch.nro
```

O build usa 16 tarefas paralelas conforme o ambiente de desenvolvimento deste port.

## Instalação no Switch

Copie para o cartão SD:

```text
/switch/DKC2Recomp/DKC2RecompSwitch.nro
/switch/DKC2Recomp/DKC2-USA-v1.0.sfc
```

Depois, abra o `.nro` pelo Homebrew Menu.

O jogo cria e usa os dados persistentes em:

```text
/switch/DKC2Recomp/.runtime/
```

Não publique a ROM nem os arquivos pessoais de save junto com o port.

## Controles

O mapeamento segue a posição física dos botões do controle do Switch:

| Controle Switch | Botão SNES |
| --- | --- |
| B | B |
| Y | Y |
| A | A |
| X | X |
| + | Start |
| − | Select |
| L / R | L / R |
| D-pad | Direcional |

## Widescreen e vídeo

O port ativa a rota widescreen já existente no projeto original. A renderização usa:

- framebuffer lógico de 342×224;
- expansão para 16:9 com as margens adicionais do jogo;
- saída apresentada em 1280×720;
- filtro de escala nearest-neighbor para preservar os pixels originais.

O diagnóstico e a validação das camadas BG1, BG2, BG3 e OBJ estão documentados em [`docs/WIDESCREEN_DIAGNOSTICS.md`](docs/WIDESCREEN_DIAGNOSTICS.md).

## Áudio

O áudio utiliza o callback DSP do runtime SNES e a saída SDL do Switch. A inicialização solicita áudio estéreo S16 em 32.040 Hz, taxa nativa usada pelo jogo.

## Estrutura específica do port

- `runner/switch_main.c` — host principal, vídeo, entrada e ciclo do jogo;
- `runner/switch_host.c` / `runner/switch_compat.c` — compatibilidade do host;
- `runner/switch_music.c` — suporte de música do host;
- `cmake/toolchains/switch-devkitA64.cmake` — toolchain do devkitA64;
- `BUILDING_SWITCH.md` — fluxo detalhado de build e instalação;
- `PORT_STATUS.md` — estado atual e pendências;
- `PORTING_WORKLOG.md` — histórico técnico do port.

Arquivos gerados, builds locais, ROMs e logs são ignorados pelo Git e não fazem parte do repositório público.

## Créditos e agradecimentos

- [Elliott Tate — DKC2Recomp](https://github.com/elliotttate/DKC2Recomp), projeto original e base deste port;
- [snesrecomp](https://github.com/mstan/snesrecomp), recompilador estático e runtime SNES compartilhado;
- [recomp-ui](https://github.com/mstan/recomp-ui), componentes compartilhados do projeto original;
- [H4v0c21 — DKC2 disassembly](https://github.com/H4v0c21/DKC2-disassembly), referência de engenharia reversa e símbolos;
- devkitPro, devkitA64, libnx e SDL2, ferramentas e bibliotecas usadas no port;
- contribuidores da comunidade de recompilação e preservação de jogos.

O conteúdo original de *Donkey Kong Country 2: Diddy's Kong Quest* pertence aos seus respectivos detentores de direitos. A ROM deve ser fornecida pelo usuário e não é distribuída neste projeto.

Consulte os arquivos de licença dos submódulos e dependências para os termos aplicáveis a cada componente.
