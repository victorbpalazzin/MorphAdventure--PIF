#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool LifeComplete(ModuleLife life)
{
    return life.init && life.reset_world && life.shutdown;
}

static unsigned Missing(const GameModules *m)
{
    unsigned missing = 0;
    if (!LifeComplete(m->player.life) || !m->player.step || !m->player.view ||
        !m->player.apply_hit || !m->player.set_portal_pose || !m->player.draw) missing |= MODULE_PLAYER;
    if (!LifeComplete(m->worlds.life) || !m->worlds.step || !m->worlds.draw) missing |= MODULE_WORLDS;
    if (!LifeComplete(m->obstacles.life) || !m->obstacles.config || !m->obstacles.step ||
        !m->obstacles.vertical_force || !m->obstacles.touches_player || !m->obstacles.path_clear ||
        !m->obstacles.take_passed || !m->obstacles.passages || !m->obstacles.draw) missing |= MODULE_OBSTACLES;
    if (!LifeComplete(m->items.life) || !m->items.step_timers || !m->items.view ||
        !m->items.consume_shield || !m->items.collect_step || !m->items.draw) missing |= MODULE_ITEMS;
    if (!LifeComplete(m->ui.life) || !m->ui.input || !m->ui.step || !m->ui.draw) missing |= MODULE_UI;
    if (!LifeComplete(m->effects.life) || !m->effects.event || !m->effects.step_particles ||
        !m->effects.begin_portal || !m->effects.step_portal || !m->effects.portal_pose ||
        !m->effects.switch_world_under_fade || !m->effects.draw_portal ||
        !m->effects.draw_particles || !m->effects.draw_transition) missing |= MODULE_EFFECTS;
    return missing;
}

static ModuleLife LifeAt(Game *g, unsigned i)
{
    ModuleLife lives[] = {g->modules.player.life, g->modules.worlds.life,
        g->modules.obstacles.life, g->modules.items.life, g->modules.ui.life, g->modules.effects.life};
    return lives[i];
}

static void Fail(Game *g, const char *message)
{
    snprintf(g->error, sizeof(g->error), "%s", message);
    g->state.mode = GAME_ERROR;
    g->accumulator = 0;
    g->pending_impulse = false;
}

unsigned GameEnergyRequired(int world)
{
    return world >= 0 && world < MORPH_WORLD_COUNT ? 4u * ((unsigned)world + 1u) : 0u;
}

float GameEnergyFraction(unsigned collected, int world)
{
    unsigned required = GameEnergyRequired(world);
    if (!required) return 0;
    return collected >= required ? 1.0f : (float)collected / required;
}

bool GameEnqueueEvent(GameEventBuffer *buffer, GameEvent event)
{
    if (!buffer) return false;
    if (buffer->count >= MORPH_MAX_EVENTS) { buffer->overflow = true; return false; }
    buffer->values[buffer->count++] = event;
    return true;
}

static bool ConfigValid(const WorldConfig *c)
{
    return c && c->name && isfinite(c->base_speed) && c->base_speed > 0 &&
        isfinite(c->duration_steps) && c->duration_steps > 0 &&
        isfinite(c->base_speed * c->duration_steps) &&
        isfinite(c->gap) && c->gap > 36 && c->gap < MORPH_HEIGHT &&
        isfinite(c->motion) && c->motion >= 0 &&
        isfinite(c->obstacle_interval) && c->obstacle_interval > 0 &&
        isfinite(c->angular_frequency) && c->angular_frequency >= 0 &&
        isfinite(c->event_interval) && c->event_interval > 0 && isfinite(c->vertical_force);
}

static bool BeginWorld(Game *g, int world, bool preserve_fade)
{
    if (world < 0 || world >= MORPH_WORLD_COUNT) { Fail(g, "Invalid world index."); return false; }
    const WorldConfig *cfg = g->modules.obstacles.config(g->modules.obstacles.life.context, world);
    if (!ConfigValid(cfg)) { Fail(g, "Missing/invalid world configuration from person 4."); return false; }
    g->config = cfg;
    g->state.world = world;
    g->state.world_id++;
    g->state.distance = g->state.scroll = 0;
    g->state.minimum_distance = cfg->base_speed * cfg->duration_steps;
    g->state.world_steps = 0;
    g->state.speed = cfg->base_speed;
    g->pending_impulse = false;
    /* Lives/score intentionally untouched. Effects retain fade at the midpoint. */
    for (unsigned i = 0; i < 5; i++) {
        ModuleLife life = LifeAt(g, i);
        life.reset_world(life.context, &g->state, cfg);
    }
    EffectsApi *fx = &g->modules.effects;
    if (preserve_fade) fx->switch_world_under_fade(fx->life.context, &g->state, cfg);
    else fx->life.reset_world(fx->life.context, &g->state, cfg);
    return true;
}

static void NewRun(Game *g, GameMode mode)
{
    g->state.lives = 3;
    g->state.score = 0;
    g->state.run_id++;
    g->state.simulation_steps = 0;
    g->state.mode = mode;
    g->state.resume_mode = GAME_PLAY;
    g->accumulator = 0;
    g->pending_impulse = false;
    g->transition_switched = false;
    (void)BeginWorld(g, 0, false);
}

bool GameInit(Game *g, const GameModules *modules)
{
    if (!g || !modules) return false;
    memset(g, 0, sizeof(*g));
    g->modules = *modules;
    g->state.mode = GAME_INTRO;
    g->state.resume_mode = GAME_PLAY;
    g->state.lives = 3;
    if (modules->contract_version != MORPH_CONTRACT_VERSION) {
        Fail(g, "Incompatible module contract version."); return false;
    }
    g->missing_modules = Missing(modules);
    if (g->missing_modules) return true; /* Explicit diagnostic shell, no fake modules. */
    for (unsigned i = 0; i < 6; i++) {
        ModuleLife life = LifeAt(g, i);
        if (!life.init(life.context)) {
            Fail(g, "Module initialization failed. See module log.");
            GameShutdown(g); return false;
        }
        g->initialized_modules |= 1u << i;
    }
    for (int i = 0; i < MORPH_WORLD_COUNT; i++) {
        if (!ConfigValid(modules->obstacles.config(modules->obstacles.life.context, i))) {
            Fail(g, "World configurations 0..5 must all be available.");
            GameShutdown(g); return false;
        }
    }
    g->ready = true;
    NewRun(g, GAME_INTRO);
    return g->state.mode != GAME_ERROR;
}

void GameShutdown(Game *g)
{
    if (!g) return;
    for (int i = 5; i >= 0; i--) {
        if (g->initialized_modules & (1u << i)) {
            ModuleLife life = LifeAt(g, (unsigned)i);
            life.shutdown(life.context);
        }
    }
    g->initialized_modules = 0;
    g->ready = false;
}

static bool Active(GameMode mode)
{
    return mode == GAME_PLAY || mode == GAME_PORTAL || mode == GAME_TRANSITION;
}

static void Pause(Game *g)
{
    if (Active(g->state.mode)) { g->state.resume_mode = g->state.mode; g->state.mode = GAME_PAUSED; }
    g->accumulator = 0;
    g->pending_impulse = false;
}

static bool Command(Game *g, GameCommand command)
{
    GameMode before = g->state.mode;
    if (command == COMMAND_RESTART) NewRun(g, GAME_INTRO);
    else if (command == COMMAND_START && before == GAME_INTRO) NewRun(g, GAME_PLAY);
    else if (command == COMMAND_RETRY && (before == GAME_LOST || before == GAME_WON)) NewRun(g, GAME_PLAY);
    else if (command == COMMAND_TOGGLE_PAUSE && Active(before)) Pause(g);
    else if ((command == COMMAND_RESUME || command == COMMAND_TOGGLE_PAUSE) && before == GAME_PAUSED) {
        g->state.mode = g->state.resume_mode;
        g->accumulator = 0;
        g->pending_impulse = false;
    } else return false;
    return true;
}

static ItemsView ReadItems(Game *g)
{
    ItemsView v = g->modules.items.view(g->modules.items.life.context);
    if (v.collected > GameEnergyRequired(g->state.world) || v.boost_count > MORPH_MAX_BOOSTS ||
        (v.score_multiplier != 1 && v.score_multiplier != 2) ||
        (!isfinite(v.world_scale)) || (fabsf(v.world_scale - 1) > 0.0001f && fabsf(v.world_scale - 0.68f) > 0.0001f)) {
        Fail(g, "ItemsView violates the shared contract.");
    }
    return v;
}

static void Notify(Game *g, GameEventType type, GameVec2 position, unsigned multiplier)
{
    GameEvent event = {0};
    event.type = type; event.position = position; event.multiplier = multiplier;
    g->modules.effects.event(g->modules.effects.life.context, &event);
}

static void Reward(Game *g, unsigned points, unsigned multiplier)
{
    if (multiplier != 1 && multiplier != 2) { Fail(g, "Invalid event reward multiplier."); return; }
    uint64_t addition = (uint64_t)points * multiplier;
    if (UINT64_MAX - g->state.score < addition) { Fail(g, "Score overflow."); return; }
    g->state.score += addition;
}

static void Damage(Game *g, PlayerView player)
{
    if (player.invulnerable || g->state.mode != GAME_PLAY) return;
    bool shield = g->modules.items.consume_shield(g->modules.items.life.context);
    g->modules.player.apply_hit(g->modules.player.life.context);
    if (!shield) g->state.lives--;
    Notify(g, shield ? EVENT_SHIELD_ABSORBED : EVENT_DAMAGE, player.position, 1);
    if (g->state.lives <= 0) {
        g->state.lives = 0;
        g->state.mode = GAME_LOST;
        g->pending_impulse = false;
        g->accumulator = 0;
    }
}

static float Pace(const GameState *s)
{
    float progress = s->distance / s->minimum_distance;
    if (progress < 0.12f) return 0.98f;
    if (progress > 0.86f) return 1.1f + 0.025f * sinf((float)s->world_steps * 0.012f);
    return 1.025f + 0.085f * sinf((float)fmod(s->world_steps, 840.0) / 840.0f * 6.28318530718f);
}

static FrameContext Frame(Game *g, float scale)
{
    FrameContext f = {0};
    f.normal_steps = 1; f.world_steps = scale; f.world_scale = scale; f.pace = 1;
    f.normal_seconds = f.ui_seconds = (float)MORPH_STEP_SECONDS;
    f.world_seconds = (float)MORPH_STEP_SECONDS * scale;
    f.tick = g->state.simulation_steps;
    return f;
}

static void Step(Game *g, GameInput input)
{
    GameModules *m = &g->modules;
    g->state.simulation_steps++;
    ItemsView items = ReadItems(g);
    if (g->state.mode == GAME_ERROR) return;
    FrameContext f = Frame(g, 1);
    if (g->state.mode == GAME_PLAY) {
        m->items.step_timers(m->items.life.context, &f);
        items = ReadItems(g);
        if (g->state.mode == GAME_ERROR) return;
        f = Frame(g, items.world_scale);
        f.pace = Pace(&g->state);
        g->state.speed += (g->config->base_speed * f.pace - g->state.speed) * 0.025f * f.world_steps;
        f.scroll_delta = g->state.speed * f.world_steps;
        g->state.scroll += f.scroll_delta;
        g->state.distance = fminf(g->state.minimum_distance, g->state.distance + f.scroll_delta);
        g->state.world_steps += f.world_steps;
        PlayerView player = m->player.view(m->player.life.context);
        bool spawn = items.collected < GameEnergyRequired(g->state.world) || g->state.distance < g->state.minimum_distance;
        m->obstacles.step(m->obstacles.life.context, &g->state, &f, player, spawn);
        m->player.step(m->player.life.context, &g->state, &f, &input, m->obstacles.vertical_force(m->obstacles.life.context));
        player = m->player.view(m->player.life.context);
        unsigned passed = m->obstacles.take_passed(m->obstacles.life.context);
        if (passed > 64) { Fail(g, "Too many obstacle rewards in one simulation step."); return; }
        for (unsigned i = 0; i < passed; i++) {
            Reward(g, 10, items.score_multiplier);
            Notify(g, EVENT_OBSTACLE_PASSED, player.position, items.score_multiplier);
        }
        if (g->state.mode == GAME_ERROR) return;
        if (player.touching_floor || m->obstacles.touches_player(m->obstacles.life.context, player)) Damage(g, player);
        if (g->state.mode != GAME_PLAY) return;
        GameEventBuffer events = {0};
        PassageQuery query = m->obstacles.passages(m->obstacles.life.context);
        if (!query.safe_y || !query.is_safe) { Fail(g, "Missing passage query functions."); return; }
        m->items.collect_step(m->items.life.context, &g->state, &f, player, query, &events);
        if (events.overflow || events.count > MORPH_MAX_EVENTS) { Fail(g, "Item event queue overflow."); return; }
        for (size_t i = 0; i < events.count; i++) {
            GameEvent *event = &events.values[i];
            if (event->type == EVENT_FRAGMENT_COLLECTED) Reward(g, 35, event->multiplier);
            else if (event->type != EVENT_BOOST_COLLECTED) { Fail(g, "Unexpected item event."); return; }
            if (g->state.mode == GAME_ERROR) return;
            m->effects.event(m->effects.life.context, event);
        }
        unsigned before = items.collected;
        items = ReadItems(g);
        if (g->state.mode == GAME_ERROR) return;
        if (before < GameEnergyRequired(g->state.world) && items.collected == GameEnergyRequired(g->state.world))
            Notify(g, EVENT_PORTAL_CHARGED, player.position, 1);
        if (items.collected == GameEnergyRequired(g->state.world) && g->state.distance >= g->state.minimum_distance &&
            m->obstacles.path_clear(m->obstacles.life.context, player)) {
            g->state.mode = GAME_PORTAL;
            g->pending_impulse = false;
            m->effects.begin_portal(m->effects.life.context, &g->state, player);
        }
    } else if (g->state.mode == GAME_PORTAL || g->state.mode == GAME_TRANSITION) {
        if (g->state.mode == GAME_PORTAL) {
            f.scroll_delta = g->state.speed * 0.35f;
            g->state.scroll += f.scroll_delta;
            m->obstacles.step(m->obstacles.life.context, &g->state, &f, m->player.view(m->player.life.context), false);
            (void)m->obstacles.take_passed(m->obstacles.life.context); /* no portal bonus for stale obstacles */
        }
        PortalSignal signal = m->effects.step_portal(m->effects.life.context, &g->state, &f);
        if (signal == PORTAL_SIGNAL_ENTERED && g->state.mode == GAME_PORTAL) {
            Reward(g, 100, items.score_multiplier);
            if (g->state.mode == GAME_ERROR) return;
            Notify(g, EVENT_PORTAL_ENTERED, m->player.view(m->player.life.context).position, items.score_multiplier);
            g->state.mode = GAME_TRANSITION;
            g->transition_switched = false;
        } else if (signal == PORTAL_SIGNAL_BLACKOUT && g->state.mode == GAME_TRANSITION && !g->transition_switched) {
            g->transition_switched = true;
            if (g->state.world == MORPH_WORLD_COUNT - 1) { g->state.mode = GAME_WON; return; }
            if (!BeginWorld(g, g->state.world + 1, true)) return;
            items = ReadItems(g);
        } else if (signal == PORTAL_SIGNAL_FADE_DONE && g->state.mode == GAME_TRANSITION && g->transition_switched) {
            g->state.mode = GAME_PLAY;
            g->pending_impulse = false;
        }
        m->player.set_portal_pose(m->player.life.context, m->effects.portal_pose(m->effects.life.context));
    }
    if (g->state.mode == GAME_ERROR) return;
    m->worlds.step(m->worlds.life.context, &g->state, &f);
    m->effects.step_particles(m->effects.life.context, &g->state, &f);
    m->ui.step(m->ui.life.context, &g->state, &f, &items);
}

void GameAdvance(Game *g, double seconds, GameInput input)
{
    if (!g || !g->ready || g->state.mode == GAME_ERROR) return;
    if (!input.focused) { Pause(g); return; }
    UiDecision ui = g->modules.ui.input(g->modules.ui.life.context, &g->state, &input);
    GameCommand command = ui.command != COMMAND_NONE ? ui.command : input.command;
    if (Command(g, command)) return; /* Never simulate the elapsed time before a reset/resume. */
    if (!Active(g->state.mode)) { g->accumulator = 0; g->pending_impulse = false; return; }
    if (g->state.mode == GAME_PLAY && (input.impulse_pressed ||
        (input.pointer_pressed && input.pointer_inside && !ui.consumes_pointer))) g->pending_impulse = true;
    if (!isfinite(seconds) || seconds <= 0) return;
    g->accumulator += fmin(seconds, 0.1);
    unsigned count = 0;
    while (g->accumulator + 1e-10 >= MORPH_STEP_SECONDS && count < MORPH_MAX_CATCHUP_STEPS && Active(g->state.mode)) {
        g->accumulator = fmax(0, g->accumulator - MORPH_STEP_SECONDS);
        GameInput step_input = input;
        step_input.command = COMMAND_NONE;
        step_input.pointer_pressed = false;
        step_input.impulse_pressed = g->pending_impulse;
        g->pending_impulse = false;
        Step(g, step_input);
        count++;
    }
    if (!Active(g->state.mode)) g->accumulator = 0;
    else if (count == MORPH_MAX_CATCHUP_STEPS) g->accumulator = fmod(g->accumulator, MORPH_STEP_SECONDS);
}

void GameDraw(Game *g)
{
    if (!g || !g->ready || g->state.mode == GAME_ERROR) return;
    GameModules *m = &g->modules;
    ItemsView items = ReadItems(g);
    if (g->state.mode == GAME_ERROR) return;
    m->worlds.draw(m->worlds.life.context, &g->state);
    m->obstacles.draw(m->obstacles.life.context, &g->state);
    m->items.draw(m->items.life.context, &g->state);
    m->effects.draw_portal(m->effects.life.context, &g->state);
    m->player.draw(m->player.life.context, &g->state, &items);
    m->effects.draw_particles(m->effects.life.context, &g->state);
    m->ui.draw(m->ui.life.context, &g->state, &items);
    /* The final blackout stops advancing at WON; never cover the victory UI. */
    if (g->state.mode != GAME_WON)
        m->effects.draw_transition(m->effects.life.context, &g->state);
}

const char *GameModeName(GameMode mode)
{
    static const char *names[] = {"INTRO", "PLAY", "PAUSED", "PORTAL", "TRANSITION", "LOST", "WON", "ERROR"};
    return mode >= GAME_INTRO && mode <= GAME_ERROR ? names[mode] : "INVALID";
}
