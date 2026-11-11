#ifndef MORPH_MODULE_API_H
#define MORPH_MODULE_API_H
#include "game_types.h"

/* Every callback is required. Context is owned by its module and outlives Game.
 * init returns false on failure and cleans its own partial initialization.
 * shutdown is called only for modules whose init succeeded, in reverse order.
 * Readers must not cast away const or retain transient pointer arguments. */
typedef struct ModuleLife {
    void *context;
    bool (*init)(void *);
    void (*reset_world)(void *, const GameState *, const WorldConfig *);
    void (*shutdown)(void *);
} ModuleLife;

/* Person 2. */
typedef struct PlayerApi {
    ModuleLife life;
    void (*step)(void *, const GameState *, const FrameContext *, const GameInput *, float force);
    PlayerView (*view)(void *);
    void (*apply_hit)(void *); /* knockback -4.5, invulnerability 75 normal steps */
    void (*set_portal_pose)(void *, PortalPose);
    void (*draw)(void *, const GameState *, const ItemsView *);
} PlayerApi;

/* Person 3. Decorative only. */
typedef struct WorldsApi {
    ModuleLife life;
    void (*step)(void *, const GameState *, const FrameContext *);
    void (*draw)(void *, const GameState *);
} WorldsApi;

/* Person 4. config returns a stable pointer for every world 0..5. */
typedef struct ObstaclesApi {
    ModuleLife life;
    const WorldConfig *(*config)(void *, int world);
    void (*step)(void *, const GameState *, const FrameContext *, PlayerView, bool spawn);
    float (*vertical_force)(void *);
    bool (*touches_player)(void *, PlayerView);
    bool (*path_clear)(void *, PlayerView);
    unsigned (*take_passed)(void *); /* drain exactly-once rewards, including portal cleanup */
    PassageQuery (*passages)(void *);
    void (*draw)(void *, const GameState *);
} ObstaclesApi;

/* Person 5. Only collection/boost events are accepted from collect_step.
 * Failed event enqueue must set overflow: never silently lose rewards. */
typedef struct ItemsApi {
    ModuleLife life;
    void (*step_timers)(void *, const FrameContext *);
    ItemsView (*view)(void *);
    bool (*consume_shield)(void *);
    void (*collect_step)(void *, const GameState *, const FrameContext *, PlayerView,
                         PassageQuery, GameEventBuffer *);
    void (*draw)(void *, const GameState *);
} ItemsApi;

/* Person 6. Pointer decision runs once per rendered frame, even during pause.
 * step/animation callbacks never run in pause. draw must not advance timers. */
typedef struct UiApi {
    ModuleLife life;
    UiDecision (*input)(void *, const GameState *, const GameInput *);
    void (*step)(void *, const GameState *, const FrameContext *, const ItemsView *);
    void (*draw)(void *, const GameState *, const ItemsView *);
} UiApi;
typedef struct EffectsApi {
    ModuleLife life;
    void (*event)(void *, const GameEvent *);
    void (*step_particles)(void *, const GameState *, const FrameContext *);
    void (*begin_portal)(void *, const GameState *, PlayerView);
    PortalSignal (*step_portal)(void *, const GameState *, const FrameContext *);
    PortalPose (*portal_pose)(void *);
    /* Preserve fade after BLACKOUT while resetting particles and world palette. */
    void (*switch_world_under_fade)(void *, const GameState *, const WorldConfig *);
    void (*draw_portal)(void *, const GameState *);
    void (*draw_particles)(void *, const GameState *);
    void (*draw_transition)(void *, const GameState *);
} EffectsApi;

typedef struct GameModules {
    unsigned contract_version;
    PlayerApi player;
    WorldsApi worlds;
    ObstaclesApi obstacles;
    ItemsApi items;
    UiApi ui;
    EffectsApi effects;
} GameModules;

enum {
    MODULE_PLAYER = 1u << 0, MODULE_WORLDS = 1u << 1,
    MODULE_OBSTACLES = 1u << 2, MODULE_ITEMS = 1u << 3,
    MODULE_UI = 1u << 4, MODULE_EFFECTS = 1u << 5,
    MODULE_ALL = (1u << 6) - 1u
};

#endif
