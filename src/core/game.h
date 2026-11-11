#ifndef MORPH_GAME_H
#define MORPH_GAME_H
#include "module_api.h"

typedef struct Game {
    GameState state;
    GameModules modules;
    const WorldConfig *config;
    double accumulator;
    bool pending_impulse, ready, transition_switched;
    unsigned missing_modules, initialized_modules;
    char error[192];
} Game;

/* False means an actual contract/init failure. Missing modules leave a valid,
 * non-playable diagnostic shell: ready=false and missing_modules set. */
bool GameInit(Game *game, const GameModules *modules);
void GameShutdown(Game *game);
void GameAdvance(Game *game, double seconds, GameInput input);
void GameDraw(Game *game);
unsigned GameEnergyRequired(int world);
float GameEnergyFraction(unsigned collected, int world);
bool GameEnqueueEvent(GameEventBuffer *buffer, GameEvent event);
const char *GameModeName(GameMode mode);

#endif
