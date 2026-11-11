# Handoff da pessoa 1 — entrega inicial da etapa 1

## Resultado real

A base C99 e raylib 5.5 foi implementada na branch **local** `migration/core`, a partir de `origin/main` em `75fcb07`. O HTML e o README existentes foram preservados. Nenhum push/publicação foi feito.

Esta entrega **não é o jogo inteiro migrado** nem a etapa 1 integrada do grupo. Ela entrega o contrato, o coordenador testável e uma janela gráfica de diagnóstico para desbloquear os colegas. Não foram implementados player, cenários, obstáculos, coletáveis ou portais reais no lugar deles.

## Arquivos entregues

- `src/main.c`: janela raylib, input, foco, render target 960×540, redimensionamento proporcional, modo de diagnóstico e smoke test com screenshot.
- `src/core/game_types.h`: estado, comandos, eventos, views e unidades.
- `src/core/module_api.h`: tabelas de funções dos módulos e lifecycle.
- `src/core/game.h/.c`: loop fixo, pausa/foco, vidas, pontuação, resets, coordenação de energia e portal, validação de dados e falhas explícitas.
- `src/core/viewport.h/.c`: transformação da janela e rejeição de cliques nas barras.
- `docs/CONTRACTS.md`: contrato versão 1, factories, estados, callbacks, proprietários e valores do jogo.
- `docs/handoffs/pessoa-1/validation/core_check.c`: fixture de integração do core, sem janela e sem raylib. Módulos simulados são EXCLUSIVOS desse teste; não entram no executável de produção.

## Pessoa 7: faça isto a seguir

1. Receba este commit/pacote completo. Leia `docs/CONTRACTS.md`; não gere novos tipos compartilhados com nomes diferentes.
2. Prepare `CMakeLists.txt` com C99 e **raylib 5.5 fixa**, sem depender de caminhos deste computador. Use dependência oficial fixada, com download na configuração inicial se escolhida essa estratégia. Suporte instalação local/offline quando disponível. Não instalar compiladores silenciosamente.
3. Crie alvos distintos: `morph-core-shell` (diagnóstico real com raylib) e `core-check` (teste puro C). A campanha completa só pode habilitar `MORPH_MODULES_LINKED` quando houver módulos reais e composition.c.
4. Você pode mover/adaptar `validation/core_check.c` para `tests/` mantendo o propósito e a cobertura. Não copie a lógica de produção para os testes.
5. Fixe instruções de build/distribuição; valide execução limpa. Depois comunique à pessoa 1 quais factories estão prontas para a composição real.

## Como foi compilado aqui

Ambiente observado: Windows x64, GCC 16.2.0 do MSYS2 UCRT64, raylib 5.5 oficial `raylib-5.5_win64_mingw-w64.zip`, backend GLFW/OpenGL 3.3, Intel UHD Graphics. A raylib foi extraída numa pasta temporária de trabalho, sem instalação no sistema. Não foi necessário CMake para esta verificação pontual; o build compartilhado é a próxima entrega da pessoa 7.

Na raiz do repositório, com o include/lib de raylib acessíveis (substitua os caminhos pelo ambiente local):

```powershell
# Crie build/ se ainda não existir. Não coloque os binários em src/.
New-Item -ItemType Directory -Force build | Out-Null
$raylibRoot = 'CAMINHO/DA/raylib-5.5_win64_mingw-w64'
gcc -std=c99 -Wall -Wextra -Werror -Wpedantic -Isrc `
  "-I$raylibRoot/include" src/main.c src/core/game.c src/core/viewport.c `
  "-L$raylibRoot/lib" -lraylib -lopengl32 -lgdi32 -lwinmm -o build/morph-core.exe

gcc -std=c99 -Wall -Wextra -Werror -Wpedantic -Isrc `
  docs/handoffs/pessoa-1/validation/core_check.c src/core/game.c src/core/viewport.c `
  -lm -o build/core-check.exe

./build/core-check.exe
./build/morph-core.exe --smoke-frames 5 --capture core-shell.png
```

Execute uma etapa apenas se a compilação anterior terminou com sucesso. Não confunda o executável diagnóstico com a versão final. Em Linux/macOS as flags de link diferem; não foram testadas nesta entrega.

## Evidências de validação

- Revisão antes do merge: verificações do teste usam `CHECK`, que permanece ativo com `NDEBUG`; inicialização/coleta não desaparecem em Release. Testes executados também com `-O2 -DNDEBUG`.
- Regressão do fade final: a campanha simulada verifica que a camada de transição continua sendo desenhada entre mundos e durante pausa, mas deixa de ser desenhada em vitória.
- Compilação do core e janela com `-std=c99 -Wall -Wextra -Werror -Wpedantic`: passou.
- Teste puro C: passou; verifica 120 passos de simulação em dois segundos com frames de 30, 60, 120 e 144 Hz; impulso pendente/consumo único; pausa/foco; descarte de atraso; mouse dentro/fora e consumido pela UI; transformação de viewport; lifecycle/falha parcial; score; escudo/dano; seis transições e vida persistente; eventos/estado inválidos.
- Teste da campanha usa **estado preparado e módulos simulados**, não uma partida real jogada. Ele prova a coordenação do core, não os algoritmos dos colegas nem a dificuldade final.
- Janela real raylib: iniciou, desenhou cinco frames, exportou screenshot e terminou com exit code 0. Log: `core-shell: initialized=1 ready=0 missing=0x3f frames=5`. `ready=0` é esperado: seis interfaces ainda pendentes.
- Screenshot inspecionado: tela de diagnóstico legível, sem texto cortado, com barras proporcionais e nomes dos módulos ausentes.
- Não executados: gameplay real em C, integração dos módulos futuros, paridade visual dos seis mundos, avaliação humana de dificuldade e execução em outras plataformas.

## Dependências para a próxima etapa da pessoa 1

Quando as pessoas 2–6 entregarem as factories listadas no contrato, a pessoa 1 poderá escrever `src/core/composition.c`, conectar as funções e testar o Ar integrado. Só depois vêm campanha completa e validação final. Não é necessário reescrever este coordenador para começar os módulos.

Se algum colega precisar mudar assinatura, unidade ou responsabilidade, registre a proposta no próprio handoff e combine a alteração com a pessoa 1. Não crie um segundo GameState.
