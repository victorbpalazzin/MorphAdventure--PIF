#include "raylib.h"
#include "core/game.h"
#include "core/viewport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if RAYLIB_VERSION_MAJOR != 5 || RAYLIB_VERSION_MINOR != 5
#error This project currently pins raylib 5.5. Coordinate version changes with person 7.
#endif

/* Person 1 adds this composition function after the real modules are delivered.
 * Without it, this executable explicitly shows the integration status only. */
#ifdef MORPH_MODULES_LINKED
GameModules MorphCreateModules(void);
#endif

static void DrawDiagnostic(const Game *g, GameInput input)
{
    Color ink = {232, 237, 224, 255}, gold = {238, 207, 143, 255};
    ClearBackground((Color){12, 23, 37, 255});
    DrawRectangleGradientV(0, 0, MORPH_WIDTH, MORPH_HEIGHT, (Color){26, 54, 70, 255}, (Color){12, 23, 37, 255});
    DrawText("MORPH ADVENTURE", 48, 40, 34, gold);
    DrawText("C99 + raylib 5.5 | Base de integracao - Pessoa 1", 50, 86, 19, ink);
    DrawText("Esta janela ainda nao e o jogo migrado.", 50, 126, 23, gold);
    DrawText("O jogo sera habilitado quando os modulos reais estiverem conectados.", 50, 160, 18, ink);
    static const char *names[] = {"Pessoa 2: personagem e fisica", "Pessoa 3: cenarios e parallax",
        "Pessoa 4: obstaculos e ambiente", "Pessoa 5: energia e boosts",
        "Pessoa 6: interface", "Pessoa 6: efeitos e portais"};
    for (unsigned i = 0; i < 6; i++) {
        int y = 216 + (int)i * 32;
        bool missing = (g->missing_modules & (1u << i)) != 0;
        DrawCircle(58, y + 9, 5, missing ? gold : GREEN);
        DrawText(names[i], 77, y, 18, ink);
        DrawText(missing ? "PENDENTE" : "CONTRATO PRESENTE", 605, y, 17, missing ? gold : GREEN);
    }
    DrawText("Proximo passo: pessoa 7 configura o build; equipe implementa os modulos.", 50, 440, 17, ink);
    DrawText("Redimensione a janela. As barras externas nao recebem cliques do jogo.", 50, 468, 16, ink);
    if (g->error[0]) DrawText(g->error, 50, 496, 16, RED);
    else DrawText("ESC fecha | Contrato: docs/CONTRACTS.md", 50, 496, 16, gold);
    if (input.pointer_inside) DrawCircleLines((int)input.pointer.x, (int)input.pointer.y, 8, gold);
}

int main(int argc, char **argv)
{
    int smoke_frames = 0, frame = 0;
    const char *capture = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--smoke-frames") == 0 && i + 1 < argc) smoke_frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc) capture = argv[++i];
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1100, 700, "Morph Adventure - base C/raylib (pessoa 1)");
    if (!IsWindowReady()) return 1;
    SetWindowMinSize(480, 270);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    RenderTexture2D target = LoadRenderTexture(MORPH_WIDTH, MORPH_HEIGHT);
    if (!IsRenderTextureValid(target)) { CloseWindow(); return 1; }
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    GameModules modules = {0};
    modules.contract_version = MORPH_CONTRACT_VERSION;
#ifdef MORPH_MODULES_LINKED
    modules = MorphCreateModules();
#endif
    Game game;
    bool initialized = GameInit(&game, &modules);
    bool previous_touch = false;
    while (!WindowShouldClose() && !IsKeyPressed(KEY_ESCAPE)) {
        GameViewport viewport = ViewportFit(GetScreenWidth(), GetScreenHeight());
        GameInput input = {0};
        input.focused = IsWindowFocused() && !IsWindowMinimized();
        input.impulse_pressed = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
        if (IsKeyPressed(KEY_P)) input.command = COMMAND_TOGGLE_PAUSE;
        if (game.state.mode == GAME_INTRO && input.impulse_pressed) input.command = COMMAND_START;
        bool touch = GetTouchPointCount() > 0;
        Vector2 point = touch ? GetTouchPosition(0) : GetMousePosition();
        input.pointer_inside = ViewportToLogical(viewport, (GameVec2){point.x, point.y}, &input.pointer);
        input.pointer_pressed = touch ? !previous_touch : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        previous_touch = touch;
        GameAdvance(&game, (double)GetFrameTime(), input);
        BeginTextureMode(target);
        ClearBackground((Color){12, 23, 37, 255});
        if (!game.ready || game.state.mode == GAME_ERROR) DrawDiagnostic(&game, input);
        else GameDraw(&game);
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        if (viewport.scale > 0) DrawTexturePro(target.texture,
            (Rectangle){0, 0, MORPH_WIDTH, -MORPH_HEIGHT},
            (Rectangle){viewport.x, viewport.y, viewport.width, viewport.height},
            (Vector2){0}, 0, WHITE);
        EndDrawing();
        frame++;
        if (smoke_frames > 0 && frame >= smoke_frames) {
            if (capture) TakeScreenshot(capture);
            break;
        }
    }
    printf("core-shell: initialized=%d ready=%d missing=0x%02x frames=%d\n",
           initialized, game.ready, game.missing_modules, frame);
    GameShutdown(&game);
    UnloadRenderTexture(target);
    CloseWindow();
    return initialized ? 0 : 1;
}
