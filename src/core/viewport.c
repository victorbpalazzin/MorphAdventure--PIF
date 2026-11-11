#include "viewport.h"
#include <math.h>

GameViewport ViewportFit(int width, int height)
{
    GameViewport v = {0};
    if (width <= 0 || height <= 0) return v;
    v.scale = fminf((float)width / MORPH_WIDTH, (float)height / MORPH_HEIGHT);
    v.width = MORPH_WIDTH * v.scale;
    v.height = MORPH_HEIGHT * v.scale;
    v.x = (width - v.width) * 0.5f;
    v.y = (height - v.height) * 0.5f;
    return v;
}

bool ViewportToLogical(GameViewport v, GameVec2 screen, GameVec2 *logical)
{
    if (logical) *logical = (GameVec2){0};
    if (!logical || v.scale <= 0 || !isfinite(screen.x) || !isfinite(screen.y)) return false;
    logical->x = (screen.x - v.x) / v.scale;
    logical->y = (screen.y - v.y) / v.scale;
    return logical->x >= 0 && logical->x < MORPH_WIDTH &&
           logical->y >= 0 && logical->y < MORPH_HEIGHT;
}
