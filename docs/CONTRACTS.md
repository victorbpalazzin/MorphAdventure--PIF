# Contrato de integração — Morph Adventure / C99 / raylib 5.5

**Versão 1. Entrega inicial da pessoa 1.** Este documento e os cabeçalhos de `src/core/` são a base compartilhada. O jogo completo ainda depende das pessoas 2–6. A pessoa 7 é dona do build definitivo, dos testes de campanha e do pacote de distribuição.

## Referência e escopo

- Base Git: `75fcb07` de `origin/main`, com `MorphAdventure.html` e README já enviados pelo grupo. A branch local desta entrega é `migration/core`.
- HTML preservado: `MorphAdventure.html`. SHA-256 observado: `3AF7952C02BCC1950664726373ABA2C84ADA87BE63B9B306C71406EF801557AF` (bytes da cópia local; conversão CRLF/LF por Git pode mudar esse hash).
- Alvo inicial: desktop Windows x64; C99; **raylib 5.5** fixada. Não usar “latest” nem versões diferentes entre colegas. A pessoa 7 pode preparar CMake com uma dependência oficial fixada e download na primeira configuração.
- Não há migração de arte/jogabilidade neste núcleo. O executável sem módulos mostra uma tela de diagnóstico explícita; não tem módulos vazios disfarçados de jogo.
- Consultadas as APIs oficiais de [raylib](https://www.raylib.com/cheatsheet/cheatsheet.html) e o [release 5.5](https://github.com/raysan5/raylib/releases/tag/5.5). A validação compilou contra os cabeçalhos reais de 5.5.

## Propriedade e limites

| Pessoa | Arquivos | Responsabilidade |
|---|---|---|
| 1 | `src/main.c`, `src/core/`, este contrato | Entrada, tempo, estados, vidas, pontos, coordenação e composição |
| 2 | `src/player/` | Física, personagens, hit feedback e primitivas de colisão |
| 3 | `src/worlds/`, `assets/worlds/` se usado | Cenários, parallax, recursos visuais decorativos |
| 4 | `src/obstacles/` | Configurações dos seis mundos, obstáculos, geometria, perigos e passagem segura |
| 5 | `src/items/` | Fragmentos, boosts, energia e seus timers |
| 6 | `src/ui/`, `src/effects/` | Interface, partículas, portais e fade |
| 7 | `CMakeLists.txt`, `cmake/`, `tests/`, scripts de pacote | Build, regressão e distribuição |

O estado central é mutável apenas pelo coordenador. Callbacks recebem `const GameState *`; nunca remover `const`, guardar o endereço da entrada temporária nem editar vidas/pontos/mundo por conta própria. Cada módulo mantém seu estado privado através de `void *context`. Não copiar tipos do contrato para outros cabeçalhos. Mudanças incompatíveis exigem acordo e incremento de `MORPH_CONTRACT_VERSION`.

Limites de interface: 6 mundos, 24 fragmentos, 2 boosts ativos, 128 eventos por passo. Capacidade excedida da fila causa erro explícito; não descartar recompensa silenciosamente. Pessoa 4 e pessoa 5 devem limitar suas próprias listas, reciclar objetos fora da tela e registrar os limites de entidades no handoff.

## Arquivos do núcleo e como conectar módulos

- `game_types.h`: tipos compartilhados, enums e constantes; não depende da raylib.
- `module_api.h`: todas as assinaturas de callbacks e `GameModules`; fonte de verdade das APIs.
- `game.h`/`game.c`: coordenador e validação de contrato.
- `viewport.h`/`viewport.c`: escala e conversão de entrada, testáveis sem janela.
- `main.c`: adaptador raylib; coleta entrada, renderiza em textura lógica 960×540, mantém proporções.

Cada responsável cria o próprio `.h/.c` com a seguinte factory pública, incluindo `core/module_api.h`:

```c
/* src/player/player.h */
PlayerApi PlayerModuleCreate(void);
/* src/worlds/worlds.h */
WorldsApi WorldsModuleCreate(void);
/* src/obstacles/obstacles.h */
ObstaclesApi ObstaclesModuleCreate(void);
/* src/items/items.h */
ItemsApi ItemsModuleCreate(void);
/* src/ui/ui.h */
UiApi UiModuleCreate(void);
/* src/effects/effects.h */
EffectsApi EffectsModuleCreate(void);
```

A factory fornece uma tabela completa de callbacks e contexto com vida útil maior que `Game`. Não desenhar, carregar textura nem abrir janela na factory. `life.init(context)` aloca/carrega os recursos depois de `InitWindow`; `life.shutdown` libera após a última renderização, antes de `CloseWindow`. Uma factory pode usar uma estrutura privada estática para a única instância de jogo; testes que exigirem múltiplas instâncias devem receber contextos independentes.

Pessoa 1 escreverá `src/core/composition.c` quando as entregas reais estiverem disponíveis:

```c
GameModules MorphCreateModules(void) {
    GameModules m = {0};
    m.contract_version = MORPH_CONTRACT_VERSION;
    m.player = PlayerModuleCreate();
    m.worlds = WorldsModuleCreate();
    m.obstacles = ObstaclesModuleCreate();
    m.items = ItemsModuleCreate();
    m.ui = UiModuleCreate();
    m.effects = EffectsModuleCreate();
    return m;
}
```

O build integrado habilitará `MORPH_MODULES_LINKED` e compilará essa composição e os módulos. Sem essa macro, a janela informa todos os módulos pendentes e não permite começar uma falsa partida. Não criar implementações vazias para fazer `ready=true`. Composição parcial não é campanha integrada; cada colega pode testar o próprio módulo num executável isolado preparado com a pessoa 7.

## Ciclo de vida e falhas

`GameInit` recebe as tabelas por valor. Verifica versão e todas as funções obrigatórias. Se faltam módulos, retorna `true` para a janela de diagnóstico, com `ready=false` e bits em `missing_modules`. Só inicializa módulos quando todas as tabelas estão completas. Inicializa na ordem player/worlds/obstacles/items/ui/effects. Libera na ordem inversa.

`init` que falha deve desfazer sua própria inicialização parcial; o core libera apenas os módulos anteriores que terminaram com sucesso. `GameShutdown` pode ser chamado de novo sem dupla liberação.

Pessoa 4 fornece `config(context, world)` para todos os índices 0..5, mesmo que a etapa 1 só tenha o Ar desenhado. O ponteiro retornado é estável até o shutdown. Configuração inválida, fila excedida ou estado impossível dos itens levam a `GAME_ERROR`, com mensagem visível; não progredir silenciosamente.

## Tempo, entrada e coordenadas

- Simulação fixa: `MORPH_STEP_SECONDS = 1/60`. `FrameContext.normal_steps = 1`; use valores por passo do HTML para física.
- `normal_seconds = ui_seconds = 1/60`. Boosts duram segundos normais. `world_steps = 1 × world_scale`; `world_seconds = normal_seconds × world_scale`.
- Escala normal é 1; slow motion é 0,68. O core atualiza timers dos itens antes de consultar a escala, e aplica a escala uma vez. Física do personagem usa `normal_steps`, não `world_steps`.
- O core integra `speed`, `distance`, `scroll` e `world_steps`. Obstáculos/itens movem suas posições com `frame.scroll_delta`, sem acrescentar de novo ao deslocamento global. Cenários consultam `state.scroll`.
- `frame.pace` é o fator de ritmo calculado uma vez pelo coordenador. Pessoa 4 usa `config.obstacle_interval / (0.98 + (frame.pace - 1) × 0.55)` no spawn e não recalcula outro relógio de ritmo.
- Trava longa: o acumulador limita o atraso recebido a 0,1 s e executa no máximo cinco passos por frame; excesso de atraso é descartado para evitar teleporte/espiral de atraso. Isso não é promessa de manter tempo de parede quando o computador não acompanha.
- Perder foco pausa gameplay, portal ou transição; voltar exige P/Continuar. Nenhum tempo acumulado é reaplicado. `ui.input` continua disponível na pausa, mas callbacks de atualização não.
- Impulso vem de borda de Espaço/↑/W, ou clique/toque inicial na área lógica, e permanece pendente até o próximo passo. É consumido uma vez, mesmo com vários passos no mesmo frame.
- `ui.input` retorna `UiDecision`: um comando e se consumiu o clique. Tecla/comando de início/pausa/reinício não executa física naquele frame. Um clique em botão não vira impulso.
- `GameInput.pointer` já está em pixels lógicos. Consulte `pointer_inside`; barras pretas e bordas externas não são cliques de jogo. Não aplicar conversão de escala outra vez.
- O protótipo HTML usa repetição de eventos do navegador; a base C usa acionamento por borda deliberadamente para não ligar a frequência de impulso ao sistema operacional. Uma política de repetição futura deve ser acordada e testada, não adicionada isoladamente.

## Ordem de um passo de gameplay

1. Ações da UI/teclado e perda de foco são resolvidas antes do acumulador.
2. Atualizar timers de boosts no tempo normal, consultar escala e multiplicador.
3. Atualizar ritmo, velocidade, distância limitada, deslocamento contínuo e relógio do mundo.
4. Atualizar obstáculos/ambiente com a posição anterior do jogador, geometria móvel e permissão de spawn.
5. Atualizar física com input pendente e força ambiental; ler `PlayerView` atualizado.
6. Drenar obstáculos ultrapassados e aplicar pontos uma única vez.
7. Se chão ou perigo toca jogador, processar um impacto: respeitar invulnerabilidade → consumir escudo → aplicar knockback/invulnerabilidade → descontar vida apenas sem escudo. A derrota interrompe o passo antes de coleta/portal.
8. Coletar fragmentos e boosts; processar fila de eventos em ordem.
9. Reconsultar energia e avaliar portal. Atualizar efeitos, cenário e UI sem duplicar regras.

`take_passed` esvazia o contador toda vez que é chamado; o core não sabe os IDs de obstáculos. `collect_step` atualiza IDs/energia no módulo items e enfileira exatamente um `EVENT_FRAGMENT_COLLECTED` por coleta válida. `event.multiplier` registra 1 ou 2 no instante da recompensa: um boost pego depois no mesmo passo não dobra retroativamente uma coleta anterior. O core aplica os valores 10/35/100; módulos não recebem pontos arbitrários.

## Responsabilidade detalhada dos módulos

### Player / pessoa 2

`step` recebe força ambiental; aplica física e reduz invulnerabilidade em tempo normal. `view` retorna estado sem alterá-lo. `touching_floor` sinaliza contato danoso ao coordenador. `apply_hit` aplica velocidade -4,5 e 75 passos de invulnerabilidade, inclusive quando escudo absorve. `set_portal_pose` só aplica posição/escala/rotação se `overrides_player=true`. Quando o portal libera o controle, preserve a posição inicial do novo mundo e retome escala 1.

`reset_world` coloca o personagem em (180,245), velocidade 0, escala 1, raio visual 23 e raio de colisão 18. `draw` consulta boosts para escudo/ímã, sem manter timers duplicados.

Primitivas a implementar em `src/player/collision.h/.c`:

```c
bool CollisionCircleRect(GameVec2 center, float radius, GameRect rect);
bool CollisionCirclePolygon(GameVec2 center, float radius,
                            const GameVec2 *vertices, size_t count);
```

`GameRect` usa largura/altura positivas; polígonos são simples, na ordem das arestas, com pelo menos três pontos, em coordenadas lógicas. Teste arestas/interior e documente contato tangente. A implementação HTML usa distância estritamente menor que o raio.

### Worlds / pessoa 3

`step` mantém animação decorativa; `draw` só desenha, usando `state.scroll` para parallax. Recursos privados são criados no init/reset e liberados corretamente. Não criar obstáculos ou causar dano. Paletas compartilhadas podem ser expostas por cabeçalho próprio após acordo com pessoa 4/6, sem alterar o core.

### Obstacles / pessoa 4

Tabela da versão atual:

| Mundo | speed | gap | interval | motion | frequency | duration_steps | event_interval | force |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ar | 4.2 | 210 | 92 | 12 | .019 | 1500 | 440 | .065 |
| Água | 4.6 | 202 | 86 | 16 | .021 | 2100 | 410 | .080 |
| Floresta | 5.0 | 194 | 80 | 20 | .023 | 2700 | 380 | 0 |
| Gelo | 5.4 | 186 | 74 | 24 | .025 | 3300 | 350 | .090 |
| Terra | 5.8 | 178 | 68 | 28 | .027 | 3900 | 320 | .105 |
| Fogo | 6.2 | 170 | 62 | 32 | .029 | 4500 | 290 | 0 |

`step(..., spawn)` só gera obstáculos/eventos especiais quando `spawn=true`. Entidades existentes continuam saindo. No modo PORTAL, apenas limpeza visual/deslocamento: cessar perigos, sem novo dano. Use `frame.scroll_delta` inclusive nesse modo (já reduzido a 35% pelo core).

`touches_player` verifica perigos já ativos, nunca os que ainda estão em aviso. Deve ser consulta sem consumir vidas/escudos. `path_clear` confirma que não há mais obstáculo perigoso à frente/atravessando a hitbox; o modo portal assume trajeto seguro. `passages` oferece consultas sem mutação com contexto válido por todo o `collect_step`, sem retornar ponteiros para variáveis de pilha. As formas desenhadas e colisões devem coincidir.

### Items / pessoa 5

`view` é consulta pura. Quantidade não pode exceder `GameEnergyRequired(world)`; multiplicador deve ser exatamente 1 ou 2; escala deve ser 1 ou 0,68. `boosts[]` usa até dois elementos, com tipo/segundos restantes (escudo usa valor negativo para indicar carga sem cronômetro). `shield` acompanha a carga real.

`step_timers` usa segundos normais. `collect_step` usa deslocamento/tempo de mundo para geração, movimento e magnetismo conforme a referência, mantém IDs únicos e reposição de perdidos. Só emite `EVENT_FRAGMENT_COLLECTED` e `EVENT_BOOST_COLLECTED`; o core emite outros eventos de feedback. Use `GameEnqueueEvent`; propague `overflow` se falhar. `consume_shield` retorna true uma única vez e remove o efeito. Nenhum callback altera vidas/pontos/mundo.

`reset_world` limpa energia/itens/boosts. Portal e transição congelam timers dos boosts como no HTML; x2 presente ao entrar no portal vale para +100. Regras: escudo uma colisão, ímã 5 s, slow 4 s, x2 7 s; máximo dois; novo terceiro substitui o ativo mais antigo; renovar tipo existente não duplica efeitos.

### UI e Effects / pessoa 6

UI transforma cliques em comandos; ações são retornadas ao core. Para derrotado/vencedor, “Tentar/Jogar novamente” usa RETRY; reinício global usa RESTART e volta ao início. `step` anima bateria/score/HUD, `draw` não modifica estado.

Effects recebe eventos de feedback e atualiza partículas com limite de capacidade. `begin_portal` começa a animação; `step_portal` controla seu relógio e retorna sinais na sequência:

1. `PORTAL_SIGNAL_ENTERED` quando o personagem entra: o core paga +100 com x2 e muda PORTAL → TRANSITION.
2. `PORTAL_SIGNAL_BLACKOUT` no instante opaco: se mundo 5, core muda para WON. Caso contrário, incrementa mundo uma vez e reseta player/worlds/obstacles/items/ui, **preservando vidas e score**.
3. `switch_world_under_fade` limpa partículas/atualiza paleta preservando a animação de fade; não reiniciar o timer do portal. Em seguida, `portal_pose` deve liberar o controle do jogador (`overrides_player=false`).
4. `PORTAL_SIGNAL_FADE_DONE` ao revelar o novo mundo: core muda TRANSITION → PLAY.

Retorne cada sinal uma vez; repetições de ENTERED/BLACKOUT são protegidas pelo core, mas a pessoa 6 não deve depender disso para avançar timers. `life.reset_world` faz reset completo apenas no início de uma partida/mundo fora do fade preservado. Pausar suspende todos esses callbacks de tempo e retoma o modo exato.

Ordem de desenho: cenário → obstáculos → itens → portal → personagem → partículas → HUD/telas → camada de transição. A camada final de fade cobre HUD na troca de mundo. Em `GAME_WON`, o core não chama `draw_transition`: o blackout final não pode esconder a tela de vitória, pois seu relógio já parou. Nas demais fases, inclusive pausa durante transição, o callback continua disponível. Todo desenho acontece dentro do render target lógico iniciado por `main.c`; os módulos não chamam `BeginDrawing`/`EndDrawing` nem abrem uma segunda janela.

## Máquina de estados

```text
INTRO --START--> PLAY
PLAY --vidas=0--> LOST --RETRY--> PLAY (nova partida)
PLAY --meta + percurso + caminho livre--> PORTAL
PORTAL --ENTERED--> TRANSITION
TRANSITION --BLACKOUT--> próximo mundo, ainda em TRANSITION
TRANSITION --FADE_DONE--> PLAY
TRANSITION --BLACKOUT no Fogo--> WON --RETRY--> PLAY (nova partida)
PLAY/PORTAL/TRANSITION --P ou perda de foco--> PAUSED
PAUSED --P ou RESUME--> modo anterior, sem salto de tempo
qualquer modo normal --RESTART--> INTRO (nova partida)
violação de contrato --> ERROR (diagnóstico; requer correção do módulo)
```

`NewRun` é interno: restaura três vidas, zera score e inicia no Ar. `BeginWorld` é interno: preserva vidas/score e reseta dados de fase. Nenhum módulo deve chamar resets de outros módulos.

## O que continua pendente

- Factories e implementações reais das pessoas 2–6; registro em composition.c pela pessoa 1 quando recebidos.
- Build CMake/dependência automática e testes integrados pela pessoa 7.
- Arte/paridade visual, jogabilidade completa, som se futuramente solicitado, balanceamento humano e distribuição final.
- Teste real em outros sistemas e celulares. A base desta entrega não promete suporte nativo mobile.

Para a pessoa 7, os comandos exatos já executados, o teste puro C e o status desta entrega estão em `docs/handoffs/pessoa-1.md`.
