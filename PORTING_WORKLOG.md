# DKC2Recomp — histórico do port para Nintendo Switch

Este arquivo registra o que foi preparado neste port, por que algumas etapas
precisam recompilar muitos arquivos e qual é o estado atual do projeto.

## Origem

- Projeto-base: [elliotttate/DKC2Recomp](https://github.com/elliotttate/DKC2Recomp)
- Diretório deste port: `DKC2Recomp-Switch`
- Submódulos inicializados:
  - `snesrecomp`
  - `recomp-ui`

O projeto usa a arquitetura de recompilação estática do `snesrecomp`: o código
65816 do jogo é convertido em C privado e ligado a um runtime SNES comum.

## ROM usada

Foram encontradas várias cópias da ROM na raiz. Todas foram verificadas por
tamanho e SHA-256 antes da limpeza.

- Arquivo mantido: `DKC2-USA-v1.0.sfc`
- Tamanho esperado: `4,194,304` bytes
- SHA-256 esperado:
  `35421a9af9dd011b40b91f792192af9f99c93201d8d394026bdfb42cbf2d8633`

A cópia que inicialmente tinha o nome `DKC2-USA-v1.0.sfc` estava errada e
tinha o hash `b79c2bb86f6fc76e1fc61c62fc16d51c664c381e58bc2933be643bbc4d8b610c`.
Ela foi removida. As cópias duplicadas corretas também foram removidas, ficando
somente uma ROM correta com o nome canônico acima.

A ROM e os diretórios de build são ignorados pelo Git; a ROM não é distribuída
nem incorporada ao código-fonte.

## Geração do código privado

O gerador oficial foi executado usando a ROM correta. A execução terminou com:

- `3323` raízes analisadas;
- `3474` variantes AOT exatas;
- `0` variantes LLE;
- `12` bancos gerados;
- overrides de widescreen e co-op aplicados.

Os fontes gerados estão em `generated/snesrecomp/` e são privados/ignorados
pelo Git, como no fluxo original do projeto.

## Arquivos adicionados para o Switch

- `cmake/toolchains/switch-devkitA64.cmake`
  - seleciona `devkitA64`, libnx e portlibs do devkitPro;
  - usa `switch.specs`;
  - habilita PIC/PIE e flags ARM adequadas ao Switch.
- `runner/switch_main.c`
  - inicialização libnx;
  - leitura dos controles Joy-Con/controle padrão;
  - janela, renderer e textura SDL2;
  - apresentação do framebuffer SNES 256x224;
  - loop de jogo a 60 Hz;
  - caminhos padrão para ROM e saves no cartão SD.
- `runner/switch_host.c`
  - callbacks de host, diagnóstico e ciclo básico do runtime.
- `runner/switch_music.c`
  - mantém desativada apenas a camada opcional de música externa/MSU;
  - o áudio original do SNES é consumido pelo DSP compartilhado e enviado ao
    dispositivo SDL no host do Switch.
- `runner/switch_compat.h` e `runner/switch_compat.c`
  - compatibilidade de `strdup`/alocação para a compilação C11 no toolchain ARM.
- `BUILDING_SWITCH.md`
  - instruções de geração, configuração, compilação e layout do SD.
- `PORT_STATUS.md`
  - checklist e estado técnico do port.

## Alterações no CMake

Foi criado o perfil `DKC2_BUILD_SNESRECOMP_SWITCH=ON`.

Nesse perfil:

- o build desktop e os testes desktop são desabilitados;
- os hosts POSIX/Win32 e componentes MSU1 não entram no executável do Switch;
- o host SDL2 específico do Switch é usado;
- as fontes geradas são agrupadas por banco e divididas em bibliotecas estáticas
  menores para evitar limites de comprimento de linha do `ar` e do linker;
- as bibliotecas gráficas do devkitPro são ligadas na ordem necessária:
  `SDL2`, `EGL`, `stdc++`, `drm_nouveau`, `glapi`, `nx` e `m`;
- ao final, `nacptool` e `elf2nro` produzem o pacote `.nro`.

## Por que recompilou tudo novamente?

Houve duas rodadas longas de recompilação por motivos diferentes:

1. A primeira mudança alterou a lista de bibliotecas do link (`EGL`,
   `drm_nouveau` e `glapi`). Isso fez o CMake/Ninja regenerar e recompilar a
   árvore de fontes gerados.
2. O linker então acusou `read-only segment has dynamic relocations`. A causa
   foi a ausência de `CMAKE_POSITION_INDEPENDENT_CODE ON` no toolchain local.
   Essa flag altera as opções dos objetos compilados, portanto uma nova
   compilação completa é esperada e necessária.

Todas as compilações do Switch foram executadas com **16 jobs paralelos**:

```text
cmake --build build-switch --parallel 16
```

## Estado no momento deste registro

- ROM correta selecionada e duplicatas removidas;
- código privado gerado com sucesso;
- configuração CMake do Switch concluída;
- host Switch e compatibilidade compilando;
- dependências SDL2/EGL/libnx resolvidas;
- build atualizado concluído novamente com 16 jobs;
- `DKC2RecompSwitch.nro` regenerado depois da correção de input e áudio.

O build de desktop foi configurado separadamente para validar que o projeto
continua configurável fora do perfil Switch.

## Primeiro teste em hardware

O primeiro `.nro` foi executado com sucesso no Switch: inicialização, vídeo e
controles básicos funcionaram. O teste revelou:

- ABXY invertidos por causa da diferença entre a nomenclatura física do
  controle Xbox/SDL e a nomenclatura física do Switch;
- áudio ausente, porque o host ainda não tinha aberto um dispositivo SDL;
- um pequeno glitch visual na introdução.

ABXY foi corrigido no `MapNpad`. O host agora abre áudio estéreo a 32.040 Hz e
usa `RtlRenderAudio` para consumir o ring do DSP. O glitch da introdução ainda
precisa de um frame de referência/captura para ser diagnosticado sem adivinhação.

## Limitações atuais

- Ainda não há teste em console Switch nesta máquina.
- A camada opcional de música externa/MSU ainda está stubada; o áudio original
  SPC/SNES já é enviado pelo DSP para o dispositivo SDL do Switch.
- O host Switch agora ativa o widescreen 16:9 do runtime original, com 43
  colunas extras por lado, framebuffer de 342x224 e apresentação 1280x720.
- A estabilidade visual do 16:9 ainda precisa ser validada com capturas e o
  workflow de diagnóstico, especialmente nas transições da introdução.
- O diagnóstico deve seguir `docs/WIDESCREEN_DIAGNOSTICS.md`: margens são
  classificadas por camada e devem ser validadas por capturas, não apenas por
  esticamento da imagem.
- O caminho esperado para instalação é:

```text
sdmc:/switch/DKC2Recomp/DKC2RecompSwitch.nro
sdmc:/switch/DKC2Recomp/DKC2-USA-v1.0.sfc
```

- O jogo deve ser iniciado somente com uma ROM que corresponda ao hash esperado.

## Próximos passos

1. Confirmar o link e o `.nro`.
2. Corrigir qualquer erro de inicialização encontrado no emulador ou console.
3. Integrar a saída de áudio do Switch.
4. Validar saves, resolução, input dos dois jogadores e estabilidade de longo
   prazo.

## 22/09/2026 — primeiro lote do SWITCH_ROADMAP

Implementados diagnóstico, sincronização APU, SRAM temporária verificada com
backup, autosave por alteração, hooks de foco, ressincronização do relógio,
métricas de fases e ring, menu ZL+ZR, configuração persistente, 4:3/16:9,
volume/escala, analógico e ownership de P1/P2. O runtime compartilhado e os
submódulos não foram modificados. O texto do menu usa SDL2_test já instalado.

A suíte anterior tinha 32 executáveis/caminhos indisponíveis. A build limpa
MSYS (com -D_GNU_SOURCE para APIs POSIX do launcher existente) passou 50/50,
incluindo falhas simuladas de SRAM. O verificador de artefatos tem teste Python
separado. Não houve teste novo no console nem uso de SRAM real do jogador.
Detalhes, baseline e limites em docs/SWITCH_IMPLEMENTATION_2026-09-22.md.
