#ifndef MORPH_GAME_TYPES_H
#define MORPH_GAME_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MORPH_CONTRACT_VERSION 1u
#define MORPH_WIDTH 960
#define MORPH_HEIGHT 540
#define MORPH_WORLD_COUNT 6
#define MORPH_MAX_ENERGY 24
#define MORPH_MAX_BOOSTS 2
#define MORPH_MAX_EVENTS 128
#define MORPH_STEP_SECONDS (1.0 / 60.0)
#define MORPH_MAX_CATCHUP_STEPS 5

/* Pure C types: the coordinator and tests do not depend on a graphics context. */
typedef struct GameVec2 { float x, y; } GameVec2;
typedef struct GameRect { float x, y, width, height; } GameRect;
typedef enum GameMode {
    GAME_INTRO, GAME_PLAY, GAME_PAUSED, GAME_PORTAL,
    GAME_TRANSITION, GAME_LOST, GAME_WON, GAME_ERROR
} GameMode;
typedef enum GameCommand {
    COMMAND_NONE, COMMAND_START, COMMAND_TOGGLE_PAUSE,
    COMMAND_RESUME, COMMAND_RESTART, COMMAND_RETRY
} GameCommand;
typedef enum BoostType { BOOST_SHIELD, BOOST_MAGNET, BOOST_SLOW, BOOST_DOUBLE } BoostType;

typedef struct WorldConfig {
    const char *name;
    float base_speed;          /* logical px / reference step */
    float gap, obstacle_interval, motion, angular_frequency;
    float duration_steps, event_interval, vertical_force;
} WorldConfig;

typedef struct GameState {
    GameMode mode, resume_mode;
    int world, lives;
    uint64_t score;
    float distance, minimum_distance, scroll, speed;
    double world_steps;        /* advances with world scale */
    uint64_t simulation_steps;
    uint32_t run_id, world_id; /* reject stale module state across resets */
} GameState;

typedef struct GameInput {
    bool focused;
    bool impulse_pressed;     /* queued until the next fixed step; consumed once */
    bool pointer_pressed, pointer_inside;
    GameVec2 pointer;          /* already transformed into 960x540 coordinates */
    GameCommand command;
} GameInput;

typedef struct FrameContext {
    float normal_steps;       /* always 1 at the fixed simulation rate */
    float world_steps;        /* normal_steps * world_scale: do not scale again */
    float normal_seconds, world_seconds, ui_seconds;
    float world_scale, scroll_delta, pace;
    uint64_t tick;
} FrameContext;

typedef struct PlayerView {
    GameVec2 position;
    float vertical_velocity, visual_radius, collision_radius;
    float scale, rotation;
    bool invulnerable, touching_floor;
} PlayerView;
typedef struct BoostView { BoostType type; float seconds_left; } BoostView;
typedef struct ItemsView {
    unsigned collected;
    unsigned boost_count;
    BoostView boosts[MORPH_MAX_BOOSTS];
    bool shield;
    unsigned score_multiplier; /* exactly 1 or 2 */
    float world_scale;        /* exactly 1 or 0.68 */
} ItemsView;

typedef enum GameEventType {
    EVENT_OBSTACLE_PASSED, EVENT_FRAGMENT_COLLECTED, EVENT_BOOST_COLLECTED,
    EVENT_DAMAGE, EVENT_SHIELD_ABSORBED, EVENT_PORTAL_CHARGED, EVENT_PORTAL_ENTERED
} GameEventType;
typedef struct GameEvent {
    GameEventType type;
    GameVec2 position;
    unsigned multiplier;     /* reward multiplier at the time of this event */
    unsigned entity_id;
    BoostType boost;
} GameEvent;
typedef struct GameEventBuffer {
    GameEvent values[MORPH_MAX_EVENTS];
    size_t count;
    bool overflow;
} GameEventBuffer;

typedef struct PassageQuery {
    void *context;
    float (*safe_y)(void *, float x, float fallback_y);
    bool (*is_safe)(void *, GameVec2 position, float radius);
} PassageQuery;

typedef struct PortalPose {
    bool overrides_player;
    GameVec2 position;
    float scale, rotation;
} PortalPose;
typedef enum PortalSignal {
    PORTAL_SIGNAL_NONE, PORTAL_SIGNAL_ENTERED,
    PORTAL_SIGNAL_BLACKOUT, PORTAL_SIGNAL_FADE_DONE
} PortalSignal;
typedef struct UiDecision {
    GameCommand command;
    bool consumes_pointer;
} UiDecision;
typedef struct GameViewport { float x, y, width, height, scale; } GameViewport;

#endif
