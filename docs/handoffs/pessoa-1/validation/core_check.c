/* Coordinator integration fixture. NOT a player/items/world implementation.
 * Person 7 may move this to tests/ once the shared build is ready. */
#include "core/game.h"
#include "core/viewport.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Always evaluate checks, including setup calls, in Release builds. */
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); exit(EXIT_FAILURE); } } while (0)

static const WorldConfig configs[6] = {
    {"Ar",4.2f,210,92,12,.019f,1500,440,.065f},
    {"Agua",4.6f,202,86,16,.021f,2100,410,.080f},
    {"Floresta",5,194,80,20,.023f,2700,380,0},
    {"Gelo",5.4f,186,74,24,.025f,3300,350,.090f},
    {"Terra",5.8f,178,68,28,.027f,3900,320,.105f},
    {"Fogo",6.2f,170,62,32,.029f,4500,290,0}
};
typedef struct Fixture {
    PlayerView player;
    ItemsView items;
    PortalSignal signal;
    UiDecision ui;
    GameEventBuffer queued;
    FrameContext last_frame;
    bool collision, clear, boost_after_collection, bad_config, fail_init;
    unsigned passed;
    int init_count, shutdown_count, reset_count, steps, impulses, hits, fail_init_at;
    int timer_steps, collect_steps, portal_begins, portal_steps, fade_switches, notifications;
    int transition_draws;
} Fixture;

static bool Init(void *p) { Fixture *f=p; f->init_count++; return !f->fail_init && f->init_count!=f->fail_init_at; }
static void Shut(void *p) { ((Fixture *)p)->shutdown_count++; }
static void Reset(void *p,const GameState *s,const WorldConfig *c) { (void)s;(void)c;((Fixture *)p)->reset_count++; }
static void ResetPlayer(void *p,const GameState *s,const WorldConfig *c)
{ Fixture *f=p;Reset(p,s,c);f->player=(PlayerView){0};f->player.position=(GameVec2){180,245};f->player.collision_radius=18;f->player.visual_radius=23;f->player.scale=1; }
static void ResetItems(void *p,const GameState *s,const WorldConfig *c)
{ Fixture *f=p;Reset(p,s,c);f->items=(ItemsView){0};f->items.score_multiplier=1;f->items.world_scale=1;f->queued=(GameEventBuffer){0}; }
static void PlayerStep(void *p,const GameState *s,const FrameContext *c,const GameInput *i,float force)
{ (void)s;(void)force;Fixture *f=p;f->steps++;f->impulses+=i->impulse_pressed?1:0;f->last_frame=*c; }
static PlayerView Player(void *p) { return ((Fixture *)p)->player; }
static void Hit(void *p) { Fixture *f=p;f->hits++;f->player.invulnerable=true; }
static void Pose(void *p,PortalPose pose) { (void)p;(void)pose; }
static void DrawPlayer(void *p,const GameState *s,const ItemsView *v) { (void)p;(void)s;(void)v; }
static void StepWorld(void *p,const GameState *s,const FrameContext *c) { (void)p;(void)s;(void)c; }
static void Draw(void *p,const GameState *s) { (void)p;(void)s; }
static void DrawTransition(void *p,const GameState *s) { (void)s;((Fixture *)p)->transition_draws++; }
static const WorldConfig *Config(void *p,int i) { CHECK(i>=0&&i<6);return ((Fixture *)p)->bad_config?NULL:&configs[i]; }
static void ObstaclesStep(void *p,const GameState *s,const FrameContext *c,PlayerView v,bool spawn)
{ (void)p;(void)s;(void)c;(void)v;(void)spawn; }
static float Force(void *p) { (void)p;return 0; }
static bool Touch(void *p,PlayerView v) { (void)v;return ((Fixture *)p)->collision; }
static bool Clear(void *p,PlayerView v) { (void)v;return ((Fixture *)p)->clear; }
static unsigned Passed(void *p) { Fixture *f=p;unsigned n=f->passed;f->passed=0;return n; }
static float SafeY(void *p,float x,float fallback) { (void)p;(void)x;return fallback; }
static bool Safe(void *p,GameVec2 pos,float r) { (void)p;(void)pos;(void)r;return true; }
static PassageQuery Passages(void *p) { return (PassageQuery){p,SafeY,Safe}; }
static void Timers(void *p,const FrameContext *c) { CHECK(c->normal_steps==1);((Fixture *)p)->timer_steps++; }
static ItemsView Items(void *p) { return ((Fixture *)p)->items; }
static bool Shield(void *p) { Fixture *f=p;bool had=f->items.shield;f->items.shield=false;return had; }
static void Collect(void *p,const GameState *s,const FrameContext *c,PlayerView v,PassageQuery q,GameEventBuffer *b)
{ (void)s;(void)c;(void)v;(void)q;Fixture *f=p;f->collect_steps++;*b=f->queued;for(size_t j=0;j<b->count;j++)if(b->values[j].type==EVENT_FRAGMENT_COLLECTED)f->items.collected++;f->queued=(GameEventBuffer){0};if(f->boost_after_collection)f->items.score_multiplier=2; }
static UiDecision Input(void *p,const GameState *s,const GameInput *i) { (void)s;(void)i;return ((Fixture *)p)->ui; }
static void StepUi(void *p,const GameState *s,const FrameContext *c,const ItemsView *v) { (void)p;(void)s;(void)c;(void)v; }
static void Event(void *p,const GameEvent *e) { (void)e;((Fixture *)p)->notifications++; }
static void PortalBegin(void *p,const GameState *s,PlayerView v) { (void)s;(void)v;((Fixture *)p)->portal_begins++; }
static PortalSignal PortalStep(void *p,const GameState *s,const FrameContext *c) { (void)s;(void)c;((Fixture *)p)->portal_steps++;return ((Fixture *)p)->signal; }
static PortalPose PortalPosition(void *p) { (void)p;return (PortalPose){0}; }
static void Switch(void *p,const GameState *s,const WorldConfig *c) { (void)s;(void)c;((Fixture *)p)->fade_switches++; }

static GameModules Modules(Fixture *f)
{
    GameModules m={0};m.contract_version=MORPH_CONTRACT_VERSION;
    ModuleLife life={f,Init,Reset,Shut};
    m.player=(PlayerApi){life,PlayerStep,Player,Hit,Pose,DrawPlayer};m.player.life.reset_world=ResetPlayer;
    m.worlds=(WorldsApi){life,StepWorld,Draw};
    m.obstacles=(ObstaclesApi){life,Config,ObstaclesStep,Force,Touch,Clear,Passed,Passages,Draw};
    m.items=(ItemsApi){life,Timers,Items,Shield,Collect,Draw};m.items.life.reset_world=ResetItems;
    m.ui=(UiApi){life,Input,StepUi,DrawPlayer};
    m.effects=(EffectsApi){life,Event,StepWorld,PortalBegin,PortalStep,PortalPosition,Switch,Draw,Draw,DrawTransition};
    return m;
}
static GameInput Focused(void) { GameInput i={0};i.focused=true;return i; }
static void Tick(Game *g) { GameAdvance(g,MORPH_STEP_SECONDS,Focused()); }
static void Cmd(Game *g,GameCommand command) { GameInput i=Focused();i.command=command;GameAdvance(g,1.0,i); }
static void Start(Game *g,Fixture *f)
{ memset(f,0,sizeof(*f));GameModules m=Modules(f);CHECK(GameInit(g,&m));CHECK(g->ready);Cmd(g,COMMAND_START);CHECK(g->state.mode==GAME_PLAY); }
static void QueueFragment(Fixture *f,unsigned multiplier)
{ GameEvent e={0};e.type=EVENT_FRAGMENT_COLLECTED;e.multiplier=multiplier;CHECK(GameEnqueueEvent(&f->queued,e)); }

static void CheckViewport(void)
{
    GameVec2 p;
    GameViewport v=ViewportFit(1920,1080);CHECK(v.scale==2);CHECK(ViewportToLogical(v,(GameVec2){360,490},&p));CHECK(p.x==180&&p.y==245);
    v=ViewportFit(1000,1000);CHECK(!ViewportToLogical(v,(GameVec2){500,20},&p));CHECK(ViewportToLogical(v,(GameVec2){500,500},&p));CHECK(fabsf(p.x-480)<.01f&&fabsf(p.y-270)<.01f);
    v=ViewportFit(1600,600);CHECK(!ViewportToLogical(v,(GameVec2){10,300},&p));CHECK(!ViewportToLogical(v,(GameVec2){v.x+v.width,300},&p));
    CHECK(!ViewportToLogical(ViewportFit(0,0),(GameVec2){0},&p));CHECK(!ViewportToLogical(v,(GameVec2){NAN,0},&p));
}
static void CheckLifecycle(void)
{
    Game g;GameModules missing={0};missing.contract_version=1;
    CHECK(GameInit(&g,&missing));CHECK(!g.ready&&g.missing_modules==MODULE_ALL);Cmd(&g,COMMAND_START);CHECK(g.state.mode==GAME_INTRO);GameShutdown(&g);
    missing.contract_version=99;CHECK(!GameInit(&g,&missing));CHECK(g.state.mode==GAME_ERROR);
    Fixture f={0};GameModules m=Modules(&f);f.bad_config=true;CHECK(!GameInit(&g,&m));CHECK(f.shutdown_count==6);GameShutdown(&g);CHECK(f.shutdown_count==6);
    f=(Fixture){0};m=Modules(&f);f.fail_init=true;CHECK(!GameInit(&g,&m));CHECK(f.shutdown_count==0);
    f=(Fixture){0};m=Modules(&f);f.fail_init_at=4;CHECK(!GameInit(&g,&m));CHECK(f.init_count==4&&f.shutdown_count==3);
    Start(&g,&f);GameShutdown(&g);GameShutdown(&g);CHECK(f.shutdown_count==6);
}
static void CheckClockAndInput(void)
{
    Game g;Fixture f;Start(&g,&f);
    GameInput input=Focused();input.impulse_pressed=true;
    GameAdvance(&g,MORPH_STEP_SECONDS/2,input);CHECK(f.steps==0);
    GameAdvance(&g,MORPH_STEP_SECONDS/2,Focused());CHECK(f.steps==1&&f.impulses==1);
    GameAdvance(&g,3*MORPH_STEP_SECONDS,Focused());CHECK(f.steps==4&&f.impulses==1);
    f.items.world_scale=.68f;Tick(&g);CHECK(fabsf(f.last_frame.world_steps-.68f)<.0001f);CHECK(f.last_frame.normal_steps==1);CHECK(fabsf(f.last_frame.normal_seconds-1.0f/60)<.0001f);
    int count=f.steps;Cmd(&g,COMMAND_TOGGLE_PAUSE);Tick(&g);CHECK(f.steps==count);CHECK(g.state.mode==GAME_PAUSED);
    Cmd(&g,COMMAND_RESUME);CHECK(f.steps==count);Tick(&g);CHECK(f.steps==count+1);
    input=Focused();input.focused=false;input.impulse_pressed=true;GameAdvance(&g,2,input);CHECK(g.state.mode==GAME_PAUSED);
    Cmd(&g,COMMAND_RESUME);Tick(&g);CHECK(f.impulses==1);
    count=f.steps;GameAdvance(&g,20,Focused());CHECK(f.steps==count+MORPH_MAX_CATCHUP_STEPS);
    count=f.steps;GameAdvance(&g,NAN,Focused());GameAdvance(&g,-2,Focused());CHECK(f.steps==count);
    input=Focused();input.pointer_pressed=true;input.pointer_inside=true;f.ui.consumes_pointer=true;GameAdvance(&g,MORPH_STEP_SECONDS,input);CHECK(f.impulses==1);
    f.ui.consumes_pointer=false;input.pointer_inside=false;GameAdvance(&g,MORPH_STEP_SECONDS,input);CHECK(f.impulses==1);
    input.pointer_inside=true;GameAdvance(&g,MORPH_STEP_SECONDS,input);CHECK(f.impulses==2);
    GameShutdown(&g);
    int expected=-1;
    for(int hz=30;hz<=144;hz+=(hz==30?30:hz==60?60:24)) {
        Start(&g,&f);for(int j=0;j<hz*2;j++)GameAdvance(&g,1.0/hz,Focused());
        if(expected<0) { expected=f.steps; }
        CHECK(f.steps==expected);CHECK(f.steps==120);GameShutdown(&g);
    }
}
static void CheckDamageAndScore(void)
{
    Game g;Fixture f;Start(&g,&f);f.collision=true;f.player.touching_floor=true;f.items.shield=true;
    Tick(&g);CHECK(g.state.lives==3&&f.hits==1&&!f.items.shield);Tick(&g);CHECK(f.hits==1);
    f.player.invulnerable=false;Tick(&g);CHECK(g.state.lives==2&&f.hits==2);
    f.player.invulnerable=false;Tick(&g);CHECK(g.state.lives==1);
    int calls=f.collect_steps;f.player.invulnerable=false;QueueFragment(&f,1);Tick(&g);CHECK(g.state.mode==GAME_LOST&&g.state.lives==0);CHECK(f.collect_steps==calls);
    Cmd(&g,COMMAND_RETRY);CHECK(g.state.world==0&&g.state.lives==3&&g.state.score==0);f.collision=false;
    f.passed=2;f.items.score_multiplier=2;QueueFragment(&f,2);Tick(&g);CHECK(g.state.score==110);
    Tick(&g);CHECK(g.state.score==110);
    f.items.score_multiplier=1;f.boost_after_collection=true;QueueFragment(&f,1);Tick(&g);CHECK(g.state.score==145); /* no retrospective x2 */
    Cmd(&g,COMMAND_RESTART);CHECK(g.state.mode==GAME_INTRO&&g.state.lives==3&&g.state.score==0);
    GameShutdown(&g);
}
static void CheckCampaign(void)
{
    Game g;Fixture f;Start(&g,&f);g.state.lives=1;
    for(int world=0;world<6;world++) {
        CHECK(g.state.world==world);CHECK(g.state.lives==1);CHECK(f.items.collected==0);
        unsigned target=GameEnergyRequired(world);CHECK(target==(unsigned)(world+1)*4);
        for(unsigned k=0;k<target;k++){QueueFragment(&f,1);Tick(&g);CHECK(f.items.collected==k+1);float fraction=GameEnergyFraction(k+1,world);CHECK((k+1==target)?fraction==1:fraction<1);}
        CHECK(g.state.mode==GAME_PLAY);f.clear=true;Tick(&g);CHECK(g.state.mode==GAME_PLAY); /* distance incomplete */
        g.state.distance=g.state.minimum_distance;f.clear=false;Tick(&g);CHECK(g.state.mode==GAME_PLAY);
        f.clear=true;Tick(&g);CHECK(g.state.mode==GAME_PORTAL);
        Cmd(&g,COMMAND_TOGGLE_PAUSE);CHECK(g.state.resume_mode==GAME_PORTAL);int age=f.portal_steps;Tick(&g);CHECK(f.portal_steps==age);Cmd(&g,COMMAND_RESUME);
        uint64_t before=g.state.score;f.items.score_multiplier=2;f.signal=PORTAL_SIGNAL_ENTERED;Tick(&g);CHECK(g.state.mode==GAME_TRANSITION&&g.state.score==before+200);
        Tick(&g);CHECK(g.state.score==before+200); /* duplicate signal cannot pay again */
        Cmd(&g,COMMAND_TOGGLE_PAUSE);CHECK(g.state.resume_mode==GAME_TRANSITION);Cmd(&g,COMMAND_RESUME);
        f.signal=PORTAL_SIGNAL_BLACKOUT;Tick(&g);CHECK(g.state.lives==1);
        if(world==5){
            CHECK(g.state.mode==GAME_WON);CHECK(g.state.world==5);
            int draws=f.transition_draws;
            GameDraw(&g);Tick(&g);GameDraw(&g);
            CHECK(f.transition_draws==draws); /* no frozen blackout above victory */
            break;
        }
        int draws=f.transition_draws;GameDraw(&g);CHECK(f.transition_draws==draws+1);
        Cmd(&g,COMMAND_TOGGLE_PAUSE);GameDraw(&g);CHECK(f.transition_draws==draws+2);
        Cmd(&g,COMMAND_RESUME);
        CHECK(g.state.world==world+1&&g.state.mode==GAME_TRANSITION);Tick(&g);CHECK(g.state.world==world+1);
        f.signal=PORTAL_SIGNAL_FADE_DONE;Tick(&g);CHECK(g.state.mode==GAME_PLAY);f.signal=PORTAL_SIGNAL_NONE;f.clear=false;
    }
    CHECK(f.fade_switches==5);GameShutdown(&g);
    CHECK(GameEnergyRequired(-1)==0&&GameEnergyRequired(6)==0);
}
static void CheckFailureReporting(void)
{
    Game g;Fixture f;Start(&g,&f);f.queued.overflow=true;Tick(&g);CHECK(g.state.mode==GAME_ERROR);CHECK(strstr(g.error,"overflow"));GameShutdown(&g);
    Start(&g,&f);f.items.collected=25;Tick(&g);CHECK(g.state.mode==GAME_ERROR);GameShutdown(&g);
    GameEventBuffer buffer={0};GameEvent e={0};for(int i=0;i<MORPH_MAX_EVENTS;i++)CHECK(GameEnqueueEvent(&buffer,e));CHECK(!GameEnqueueEvent(&buffer,e)&&buffer.overflow);
}
int main(void)
{
    CheckViewport();CheckLifecycle();CheckClockAndInput();CheckDamageAndScore();CheckCampaign();CheckFailureReporting();
    puts("PASS: viewport, lifecycle, fixed clock, focus, queued input, pause, damage, score, six-world coordination and contract errors.");
    puts("These are coordinator tests with explicit fixtures, NOT a completed raylib game or full gameplay validation.");
    return 0;
}
