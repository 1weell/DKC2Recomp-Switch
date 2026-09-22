# Roadmap do nosso port para Nintendo Switch

Criado em: 22/09/2026. Atualizado em: 22/09/2026.
Primeiro lote implementado e testado localmente; aceitação no Switch pendente.

Evidências e limites: [checkpoint de implementação](docs/SWITCH_IMPLEMENTATION_2026-09-22.md).
Os itens marcados abaixo indicam implementação com inspeção, cross-build e/ou
os testes sintéticos indicados nesse registro. Nenhuma marca representa um novo
teste no console; os critérios de aceitação de todos os pacotes seguem abertos.

Este é o plano de trabalho **do nosso port para Switch**. O arquivo
`docs/ROADMAP.md` descreve a evolução do DKC2Recomp e seus recursos
compartilhados; ele continua sendo referência upstream, não o nosso backlog.
Um recurso existir no desktop não significa que já esteja integrado,
otimizado ou validado no Switch.

## 1. Objetivo e prioridades

Entregar um port que permita jogar com áudio estável, progresso preservado,
controles previsíveis e apresentação fluida, tanto no portátil quanto no dock.
Depois, incorporar os recursos de conforto e as melhorias opcionais do recomp.

Ordem de execução proposta:

1. Diagnóstico mínimo, sincronização do áudio, saves e ciclo de vida.
2. Medição e correção da temporização dos frames.
3. Menu, configurações persistentes e controles de dois jogadores.
4. Validação visual e consolidação do widescreen.
5. Save states, rewind, filtros, música externa e recursos adicionais.

Não há datas de entrega prometidas: o esforço depende das medições e dos
testes no console. Cada etapa termina com evidências, não apenas com um NRO
que compilou. Uma correção urgente pode mudar a ordem, com registro do motivo.

## 2. Ponto de partida observado

Inspeção de `runner/switch_main.c`, `runner/switch_host.c`,
`runner/switch_music.c`, `CMakeLists.txt` e rotinas compartilhadas de áudio/SRAM.

| Área | O que existe | Pendência do nosso port |
| --- | --- | --- |
| Build | Perfil devkitA64/libnx, SDL2 e geração de NRO | Registrar versões e tornar cada pacote rastreável |
| Boot | Verificação da ROM e inicialização do runtime | Erros visíveis e recuperação amigável |
| Vídeo | Textura SDL, saída solicitada de 1280×720 e 16:9 fixo | Seleção de proporção, diagnóstico e validação por cenário |
| Áudio | Callback SDL, pedido de 32.040 Hz, ajuste à frequência obtida | Locks do host vazios; revisar concorrência e medir estabilidade |
| SRAM | Leitura no boot e escrita explícita ao sair | Escrita verificada, recuperação e salvamento durante a sessão |
| Input | Mapeamento físico ABXY e dois PadState | Atribuição P1/P2, analógico, reconexão e remapeamento |
| Pacing | VSync mais prazo acumulado a 60 Hz | Medir interação dos dois mecanismos e tratar atrasos/retomadas |
| Diagnóstico | Mensagens em `boot.log` | Caminho relativo, pouca informação e estado fatal não persistido |
| MSU-1 | Adaptador desativado em `switch_music.c` | Integração futura específica para o Switch |

O README relata funcionamento em hardware; outros documentos ainda deixam
validações de áudio, SRAM e widescreen pendentes. Antes de fechar qualquer
marco, identificar qual NRO foi testado e consolidar os resultados. Esta
inspeção não constitui um novo teste no console.

## 3. Regras de acompanhamento

- `[ ]` significa pendente; `[x]` exige implementação e a evidência pertinente.
- Distinguir sempre: implementado, compilado, testado no desktop e validado no
  Switch. Nenhum desses estados substitui automaticamente o seguinte.
- Bugs compartilhados devem ser corrigidos na camada adequada. Não editar C
  gerado manualmente nem alterar a lógica do jogo para ocultar falhas do host.
- Manter ROM, saves, capturas, áudio e fontes gerados fora dos arquivos
  versionados, conforme `AGENTS.md`.
- Preservar o NRO anterior e usar cópias privadas dos saves nos testes de risco.
- Nos marcos de implementação, executar a suíte disponível antes/depois,
  conforme `AGENTS.md`, e registrar testes indisponíveis ou falhas preexistentes.

## 4. SW-00 — Baseline e diagnóstico mínimo

**Prioridade:** P0. **Dependência:** nenhuma. **Esforço relativo:** pequeno/médio.

### O que eu faria

- [x] Registrar revisão do port, revisões dos submódulos, versão do toolchain,
  opções CMake e SHA-256 do NRO usado como baseline.
- [x] Fixar o destino do log em `sdmc:/switch/DKC2Recomp/.runtime/boot.log`.
  Hoje `fopen("boot.log", "w")` depende do diretório de execução e
  `host_report_set_output_directory()` não aplica o caminho recebido.
- [x] Registrar e disponibilizar o estado fatal real; atualmente
  `host_report_has_fatal()` retorna sempre zero.
- [x] Registrar renderer escolhido, fallback para software, dimensões reais
  de saída, frequência/formato/buffer de áudio obtidos e etapas do boot.
- [x] Tratar erros de criação dos diretórios, diferenciando diretório existente
  de falta de acesso ou falha de armazenamento.
- [ ] Mostrar falhas de inicialização na tela quando possível, com mensagem
  legível e saída pelo controle; manter alternativa mínima se o vídeo falhar.
- [x] Preservar um log anterior e limitar volume/frequência das gravações.
  Não gravar métricas no SD a cada frame em builds normais.

### Critério de conclusão

Uma falha de ROM, vídeo, áudio ou armazenamento deixa informação útil, com
identificação da build. Inicializar a partir de diretórios diferentes não muda
o destino esperado dos logs. Não declarar recuperação de erros sem exercitá-la.

## 5. SW-01 — Sincronização e estabilidade do áudio

**Prioridade:** P0. **Dependência:** SW-00 para diagnóstico.
**Esforço relativo:** médio, podendo crescer conforme os resultados.

### Problema observado

O callback SDL chama `RtlRenderAudio()` enquanto o loop executa o jogo.
O runtime usa `RtlApuLock()`/`RtlApuUnlock()` em operações compartilhadas,
mas a implementação Switch é vazia. Isso é um risco de concorrência a
investigar e corrigir; não é um diagnóstico confirmado de estalos no console.

### O que eu faria

- [x] Mapear quais dados da APU/DSP/ring são acessados por cada thread e quais
  chamadas exigem proteção, incluindo reset e futuras cargas de estado.
- [x] Definir a primitiva de sincronização compatível com o host e seu ciclo
  de vida, verificando chamadas aninhadas e ordem de aquisição para evitar deadlock.
- [x] Criar a proteção antes da primeira operação protegida do runtime e
  liberá-la apenas depois de parar/fechar o dispositivo de áudio.
- [ ] Manter a seção crítica curta; não fazer I/O de SD, alocações recorrentes
  ou logging pesado dentro do callback.
- [ ] Medir faltas de amostras, descarte/overflow quando aplicável, ocupação do
  buffer e frequência efetivamente negociada com SDL.
- [ ] Ajustar o tamanho do buffer com medições de latência versus estabilidade.
  O pedido atual de 534 amostras não será tratado como tamanho garantido.
- [ ] Garantir silêncio inicial e transições seguras em pausa, reset e retomada.
- [x] Diferenciar áudio indisponível de áudio desativado pelo usuário.

### Validação e conclusão

- [ ] Testar introdução, música e efeitos simultâneos, troca de fase e morte.
- [ ] Testar repetição de pausa/retomada e fechamento durante reprodução.
- [ ] Fazer uma sessão de pelo menos 60 minutos em hardware, observando
  interrupções audíveis e evolução dos contadores.
- [ ] Registrar frequência e buffer da build aceita; não aprovar apenas pela
  ausência de crash ou por um fingerprint de áudio isolado.

## 6. SW-02 — SRAM confiável e ciclo de vida

**Prioridade:** P0. **Dependências:** SW-00; coordenar pausa do áudio com SW-01.
**Esforço relativo:** médio.

### Problema observado

O host grava SRAM explicitamente ao sair normalmente. A rotina compartilhada
renomeia o save anterior para `.bak`, mas não verifica todos os resultados
de rename, escrita e fechamento. `appletMainLoop()` por si só não documenta
nem implementa todas as políticas de pausa, foco e retomada desejadas.

### O que eu faria

- [ ] Detectar SRAM alterada por mecanismo de dirty state ou comparação de
  conteúdo, com custo medido, sem escrever no SD continuamente.
- [ ] Salvar periodicamente apenas quando houver alteração, em um ponto
  seguro do loop; definir e documentar o intervalo após os testes.
- [x] Escrever primeiro um arquivo temporário, conferir tamanho, resultados
  de escrita/flush/close e só depois promover o novo save.
- [x] Manter um backup válido e testar recuperação de principal ausente ou
  truncado. Conferir tamanho ajuda a detectar truncamento, não toda corrupção.
- [ ] Verificar as garantias reais de rename/persistência no filesystem do
  Switch; não prometer atomicidade contra perda de energia sem evidência.
- [ ] Avisar na interface quando não for possível salvar; não exibir sucesso
  enquanto a gravação falhou.
- [x] Definir comportamento para foco perdido, HOME, suspensão e retomada,
  utilizando os eventos que a plataforma efetivamente disponibiliza.
- [ ] Pausar simulação/áudio quando apropriado e renovar o prazo dos frames
  ao retomar, sem tentar recuperar todo o tempo em suspensão.
- [ ] Salvar ao sair pelo menu e realizar encerramento ordenado dos recursos.

### Validação e conclusão

- [ ] Criar progresso, salvar, fechar e reabrir usando SRAM real.
- [ ] Repetir suspensão/retomada em gameplay e menus.
- [ ] Exercitar falhas controladas de escrita e recuperação de backup usando
  cópias privadas; não arriscar o único save do jogador.
- [ ] Documentar a janela máxima esperada de perda de progresso caso haja
  fechamento abrupto antes do próximo autosave. Não garantir salvamento em
  eventos nos quais o processo não recebe oportunidade de executar código.

## 7. SW-03 — Pacing, desempenho e consumo

**Prioridade:** P1. **Dependências:** SW-00 e áudio suficientemente estável.
**Esforço relativo:** médio/alto, guiado por medição.

### O que eu faria

- [x] Instrumentar `RtlRunFrame`, `Dkc2DrawPpuFrame`, upload da textura e
  `SDL_RenderPresent` separadamente.
- [ ] Coletar média, percentis 95/99, máximo e frames atrasados em janelas
  curtas; exibir resumo opcional e persistir apenas agregados.
- [ ] Comparar VSync, espera por software e a combinação atual. Confirmar a
  cadência esperada pelo runtime antes de mudar os 60 Hz do host.
- [x] Tratar atraso grande e retomada com ressincronização explícita; evitar
  rajadas de frames para alcançar um deadline antigo.
- [ ] Preservar velocidade do jogo e sincronização audiovisual ao decidir
  políticas de atraso. Não alterar o tempo da simulação para maquiar FPS.
- [ ] Medir 4:3 e 16:9 nas mesmas rotas, no portátil e dock, com condições
  registradas e clocks padrão como referência inicial.
- [ ] Otimizar apenas os maiores custos comprovados: cópias de framebuffer,
  conversões, renderização PPU, apresentação ou trechos do runtime.
- [ ] Avaliar flags de compilação/LTO individualmente se houver benefício
  mensurável, preservando correção e possibilidade de diagnóstico.

### Critério de conclusão

Relatório antes/depois com a mesma rota, build e configuração identificadas,
sem regressão de velocidade ou áudio. A meta é apresentação consistente na
cadência escolhida; metas numéricas de percentil e margem de CPU serão fixadas
após a baseline. Overclock não será requisito para considerar a base saudável.

## 8. SW-04 — Menu e configurações do Switch

**Prioridade:** P1. **Dependências:** SW-01/SW-02 para pausa e saída seguras.
**Esforço relativo:** médio/alto.

### O que eu faria

- [x] Criar um menu leve usando a infraestrutura existente quando adequado,
  sem importar dependências desktop incompatíveis com o perfil Switch.
- [x] Definir um atalho configurável que não retire Start/Select do jogo.
- [ ] Oferecer Retomar, Vídeo, Áudio, Controles, Reiniciar e Sair.
- [x] Implementar arquivo de configuração versionado em `.runtime`, com
  defaults seguros, validação de limites e recuperação de arquivo inválido.
- [x] Permitir 4:3/16:9, volume e modo de escala; explicar opções em linguagem
  de jogador, sem expor detalhes internos desnecessários.
- [x] Ao mudar proporção, atualizar buffers, pitch e textura de forma segura;
  definir se a alteração pode ocorrer ao vivo ou exige reinício controlado.
- [x] Preservar a proporção escolhida, com barras quando necessário, em vez
  de esticar automaticamente qualquer framebuffer para a saída inteira.
- [ ] Tornar o texto legível na tela portátil e manter navegação previsível.
- [x] Confirmar ações destrutivas como reiniciar uma partida quando necessário.

### Critério de conclusão

Todas as funções operam só com controle; configurações sobrevivem ao reboot;
um arquivo inválido não impede o boot; abrir o menu pausa corretamente e
fechá-lo não provoca salto de tempo ou explosão de áudio acumulado.

## 9. SW-05 — Controles, reconexão e dois jogadores

**Prioridade:** P1. **Dependência:** SW-04 para remapeamento persistente.
Correções de atribuição podem entrar antes do menu. **Esforço:** médio.

### O que eu faria

- [x] Revisar `padInitialize` de P1/P2: ambos incluem Handheld hoje. Verificar
  se essa configuração duplica entradas e definir ownership inequívoco.
- [x] Manter o ABXY físico já corrigido e testar cada bit SNES do mapeamento.
- [x] Adicionar analógico como direcional, com zona morta e política explícita
  para diagonais e direções opostas.
- [ ] Identificar conexão/desconexão e impedir botão preso após reconexão.
- [ ] Permitir escolher/remapear controles por jogador e restaurar padrões.
- [x] Definir quais estilos são suportados: portátil, par Joy-Con e Pro
  Controller primeiro; Joy-Con individual exige mapeamento e validação próprios.
- [ ] Separar validação do modo original de dois jogadores da integração do
  TEAM/co-op simultâneo do recomp. Não declarar paridade entre esses modos.
- [ ] Integrar rumble opcional após confirmar os eventos disponíveis, com
  intensidade limitada, teste por jogador e desligamento ao pausar/sair.

### Critério de conclusão

Matriz de controles preenchida para cada estilo anunciado, P1/P2 sem inputs
duplicados e reconexão funcional. Testar movimentos simultâneos e navegação
do menu com dois controles conectados.

## 10. SW-06 — Fidelidade visual e widescreen

**Prioridade:** P1 para glitches conhecidos; P2 para ampliação de cobertura.
**Dependências:** seleção 4:3/16:9 e baseline de desempenho.
**Esforço relativo:** alto e incremental.

### O que eu faria

- [ ] Capturar o glitch da introdução em uma build identificada e reproduzir
  em 4:3 e 16:9, distinguindo falha de PPU, margem, upload ou apresentação.
- [ ] Comparar cenas equivalentes com o caminho desktop/referência adequado,
  usando o fluxo de `docs/WIDESCREEN_DIAGNOSTICS.md` quando aplicável.
- [ ] Conferir o centro original separadamente das margens; uma margem boa
  não compensa alteração indevida dos pixels centrais.
- [ ] Testar scrolling horizontal/vertical, parallax, chuva, água, lava,
  brambles, colmeias, arenas, mapas, bônus e efeitos Mode-7.
- [ ] Incluir personagens, inimigos e objetos nas bordas: terreno preenchido
  não prova que animação, clipping ou comportamento estejam corretos.
- [ ] Exercitar entrada de fase, morte, checkpoint, reinício e saída de bônus.
- [ ] Manter uma lista por família de cenário: validado, parcial, defeito
  conhecido ou ainda não testado. Definir fallback para casos não suportados.
- [ ] Decidir o default de proporção com base na cobertura medida e registrar
  a decisão; o 16:9 fixo atual não é evidência de suporte integral.

### Critério de conclusão

Glitch conhecido reproduzido e resolvido ou claramente isolado; corpus de
rotas com comparação antes/depois e inspeção em velocidade normal no console.
Não declarar widescreen completo a partir de intro, attract mode ou screenshots.

## 11. SW-07 — Recursos de conforto e extras

**Prioridade:** P2/P3. **Dependência:** marcos básicos aceitos.
Cada recurso será opcional e terá custo de memória/CPU/SD documentado.

### SW-07A — Save states

- [ ] Reutilizar serialização existente, com identificação de formato, ROM e
  compatibilidade de build; rejeitar estados incompatíveis de forma legível.
- [ ] Implementar slots e proteção contra sobrescrita acidental.
- [ ] Coordenar carga com áudio, framebuffer e estado do host.
- [ ] Definir explicitamente a relação entre estado carregado e SRAM persistida.
- [ ] Validar carga em gameplay, transição e dois jogadores, sem botões presos.

### SW-07B — Rewind

- [ ] Medir tamanho/custo dos snapshots antes de escolher duração e frequência.
- [ ] Usar buffer circular com orçamento de memória limitado e configuração
  para desativar completamente o recurso.
- [ ] Invalidar histórico em reset ou carga incompatível e tratar áudio ao recuar.
- [ ] Verificar que gravar snapshots não introduz stutter nem gravações contínuas
  no SD; documentar interações com captura determinística de inputs.

### SW-07C — Filtros e apresentação

- [ ] Integrar filtros leves, começando por escala e um CRT opcional.
- [ ] Comparar nitidez, proporção e custo em portátil/dock; avaliar shaders
  conforme suporte real do backend, sem prometer paridade desktop antecipada.
- [ ] Manter um caminho simples de referência e medir cada preset isoladamente.

### SW-07D — Música externa/MSU-1

- [ ] Substituir o stub por integração compatível com o Switch, preservando o
  áudio original e fallback de faixa ausente.
- [ ] Implementar leitura com buffering apropriado do SD, fora do callback.
- [ ] Expor pasta, ativação e volume separado; validar transições e loops.
- [ ] Definir sincronização da posição musical com save/load e rewind antes
  de declarar esses recursos compatíveis entre si.

### SW-07E — Personagens e modos do recomp

- [ ] Inventariar APIs, assets externos e opções necessários para Donkey/Kiddy
  e TEAM/co-op, distinguindo código presente de funcionalidade acessível.
- [ ] Integrar um recurso por vez e testar animais, cordas, transporte/arremesso,
  colisões, morte e mudanças de fase com ambos os jogadores.
- [ ] Manter dependências de conteúdo externas e documentar limites conhecidos.

Netplay, 21:9 e substituição ampla do renderer ficam fora dos primeiros
pacotes. Só entram mediante objetivo definido e avaliação específica de custo.

## 12. SW-08 — Testes e preparação de releases

**Prioridade:** transversal; inicia em SW-00 e acompanha todos os marcos.

### Automação

- [x] Extrair políticas testáveis do host quando necessário: mapping,
  configuração, prazos e decisões de salvamento.
- [x] Adicionar testes sintéticos de comportamento e falhas para mudanças
  relevantes, sem exigir ROM nos testes públicos.
- [x] Manter testes desktop de lógica separados do cross-build; o perfil
  Switch atualmente desativa testes desktop e isso não comprova cobertura.
- [x] Criar uma verificação de build Switch, links e artefatos esperados.
- [ ] Usar corpus privado existente para regressões compartilhadas, registrando
  separadamente o que não foi executado por falta de ambiente ou fixtures.

### Matriz mínima de hardware

| Cenário | Verificar |
| --- | --- |
| Boot válido | ROM, vídeo, áudio, input e caminhos de dados |
| ROM ausente/inválida | Erro legível e saída previsível |
| Intro e attract | Glitches, áudio e estabilidade |
| Gameplay horizontal/vertical | Pacing, bordas e controles |
| Morte, bônus e troca de fase | Transições visuais e sonoras |
| Save e reinício do aplicativo | Persistência do progresso |
| HOME/suspensão/retomada | Áudio, inputs e relógio dos frames |
| Reconexão e dois jogadores | Atribuição correta e ausência de botões presos |
| Portátil e dock | Imagem, controle e tempos de frame |
| Sessão prolongada | Memória, áudio, stutter e progresso preservado |

### Registro de cada candidato

Guardar identificação da build e do NRO, ambiente do console, modos testados,
configurações, duração, passos, resultado esperado/observado, logs e localização
das evidências privadas. Anotar explicitamente os cenários não executados.

Atualizar `PORT_STATUS.md`, `PORTING_WORKLOG.md`, instruções de build e README
com o estado real do port. Atualizar documentação compartilhada quando uma
mudança afetar efetivamente o runtime ou seus contratos.

## 13. Pacotes de entrega propostos

| Pacote | Conteúdo | Condição para fechar |
| --- | --- | --- |
| A — Base confiável | SW-00, SW-01, SW-02 e baseline SW-03 | Diagnóstico utilizável, áudio revisado, saves e retomada validados |
| B — Uso diário | Pacing SW-03, menu SW-04 e controles SW-05 | Configurações persistentes, fluidez medida e P1/P2 previsíveis |
| C — Validação visual | SW-06 e ampliação da matriz | Introdução investigada e suporte por cenário documentado |
| D — Conforto | SW-07A/B/C, separadamente | Cada opção validada com custo conhecido e sem regressão da base |
| E — Expansão | SW-07D/E | Áudio externo e recursos extras aceitos no Switch |

Esses nomes são marcos de trabalho, não números de versão já reservados.
SW-08 acompanha todos eles. O pacote seguinte não apaga pendências de aceitação
do anterior; trabalho independente pode avançar sem declarar o marco concluído.

## 14. Primeiro lote de tarefas executáveis

1. Identificar o NRO atual e reconciliar o que já foi validado no console.
2. Corrigir destino/estado do logger e registrar configuração real de SDL.
3. Auditar acessos concorrentes e implementar o contrato de locks da APU.
4. Revisar escrita/backup de SRAM e adicionar salvamento controlado por alteração.
5. Implementar políticas de pausa/retomada e ressincronização do relógio.
6. Medir a mesma rota antes/depois e executar a matriz mínima do pacote A.
7. Registrar resultados e somente então marcar os itens correspondentes.

## Histórico deste roadmap

- **22/09/2026:** criação do roadmap próprio do port Switch a partir da análise
  local. Nenhuma correção, build ou validação em hardware foi feita como parte
  desta edição documental. Todos os novos itens permanecem pendentes.

- **22/09/2026 — implementação:** 30 itens com evidência local; SRAM verificada,
  mutex da APU, log fixo, métricas, menu/configuração e controles básicos.
  Autosave de 30 s e hooks de foco implementados, mas medição de custo e
  testes no console pendentes. Menu Controles oferece apenas o atalho;
  remapeamento por jogador continua pendente. Métricas de ring medem ocupação
  e entradas vazias, sem cobertura completa de underrun/overflow. A validação
  visual, a comparação de pacing, save states, rewind, CRT, MSU-1 e extras
  não foram declarados concluídos.
