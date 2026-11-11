#ifndef MORPH_VIEWPORT_H
#define MORPH_VIEWPORT_H
#include "game_types.h"
GameViewport ViewportFit(int window_width, int window_height);
bool ViewportToLogical(GameViewport viewport, GameVec2 screen, GameVec2 *logical);
#endif
