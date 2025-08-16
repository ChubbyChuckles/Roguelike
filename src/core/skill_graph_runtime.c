/* Runtime integration layer for experimental UI skill graph (Phase 5)
    Wiring of advanced features: persistent pan/zoom, filtering, allocation + undo,
    pulses, spend flyouts, synergy panel, tag filters. */
#include "core/app_state.h"
#include "core/skills.h"
#include "ui/core/ui_context.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* Forward declarations (not exposed in public headers) */
void rogue_skills_recompute_synergies(void);
void rogue_persistence_save_player_stats(void);
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
extern SDL_Renderer* g_internal_sdl_renderer_ref; /* provided by renderer.c */
extern void rogue_font_draw_text(int x,int y,const char* text,int small, RogueColor color);
#endif

/* Local static UI context (independent of existing legacy HUD rendering). */
static RogueUIContext g_skill_ui;
static int g_skill_ui_inited = 0;

/* Persistent runtime state for interaction */
typedef struct RuntimeSkillGraphState {
    int initialized;
    float view_x, view_y; /* world top-left */
    float zoom;
    unsigned int filter_mask; /* tag filter */
    struct { int skill_id; int prev_rank; } undo[64];
    int undo_count;
    int last_mouse_down; /* previous frame left button */
} RuntimeSkillGraphState;
static RuntimeSkillGraphState g_rt = {0};

void rogue_skillgraph_runtime_init(void){
    if(g_skill_ui_inited) return;
    RogueUIContextConfig cfg = { .max_nodes = 4096, .seed = 1337u, .arena_size = 128*1024 };
    if(!rogue_ui_init(&g_skill_ui, &cfg)) return;
    g_skill_ui_inited = 1;
    rogue_ui_skillgraph_enable_synergy_panel(&g_skill_ui,1);
    g_rt.initialized=1; g_rt.view_x=0.0f; g_rt.view_y=0.0f; g_rt.zoom=1.0f; g_rt.filter_mask=0; g_rt.undo_count=0; g_rt.last_mouse_down=0;
}

/* Build graph from actual registered skills; arrange in radial layers by tag groups. */
static void build_live_graph(RogueUIContext* ui){
    if(g_app.skill_count<=0) return;
    float view_w = (float)g_app.viewport_w * 0.70f;
    float view_h = (float)g_app.viewport_h * 0.70f;
    /* Use persistent pan/zoom state */
    if(g_rt.zoom < 0.25f) g_rt.zoom = 0.25f; if(g_rt.zoom>3.0f) g_rt.zoom=3.0f;
    rogue_ui_skillgraph_begin(ui, g_rt.view_x, g_rt.view_y, view_w/g_rt.zoom, view_h/g_rt.zoom, g_rt.zoom);
    if(g_rt.filter_mask) rogue_ui_skillgraph_set_filter_tags(ui, g_rt.filter_mask); else rogue_ui_skillgraph_set_filter_tags(ui, 0);
    int layers = 3; /* simple layering by tag buckets */
    float base_r = 60.0f;
    int count = g_app.skill_count;
    for(int i=0;i<count;i++){
        const RogueSkillDef* def = rogue_skill_get_def(i);
        const RogueSkillState* st = rogue_skill_get_state(i);
        if(!def||!st) continue;
        unsigned int tags = (unsigned int)def->tags;
        int synergy = def->is_passive && def->synergy_id>=0;
        /* Determine ring radius based on primary tag bit (first set bit) */
        int layer = 0;
        unsigned int tag_bits = tags?tags:1u; /* ensure non-zero */
        while((tag_bits & 1u)==0){ tag_bits >>=1; layer++; if(layer>=layers) break; }
        if(layer>=layers) layer = layers-1;
        float radius = base_r + layer * 55.0f;
        float ang = ((float)i / (float)count) * 6.2831853f;
        float cx = view_w*0.5f + cosf(ang)*radius;
        float cy = view_h*0.5f + sinf(ang)*radius;
        rogue_ui_skillgraph_add(ui, cx, cy, i, st->rank, def->max_rank, synergy, tags);
    }
    rogue_ui_skillgraph_build(ui);
}

/* Handle input for pan/zoom/filter/allocate/undo. Called once per frame before build. */
static void runtime_skillgraph_handle_input(void){
#ifdef ROGUE_HAVE_SDL
    int mx,my; Uint32 mstate = SDL_GetMouseState(&mx,&my);
    const Uint8* kb = SDL_GetKeyboardState(NULL);
    /* Zoom with + / - keys */
    float dtf = (float)g_app.dt;
    if(kb[SDL_SCANCODE_EQUALS]||kb[SDL_SCANCODE_KP_PLUS]) g_rt.zoom *= 1.0f + dtf * 1.5f;
    if(kb[SDL_SCANCODE_MINUS] || kb[SDL_SCANCODE_KP_MINUS]) g_rt.zoom *= 1.0f - dtf * 1.5f;
    /* Pan with WASD / arrows while holding right mouse or space */
    float pan_speed = 400.0f * dtf / g_rt.zoom;
    if(kb[SDL_SCANCODE_LEFT] || kb[SDL_SCANCODE_A]) g_rt.view_x -= pan_speed;
    if(kb[SDL_SCANCODE_RIGHT]|| kb[SDL_SCANCODE_D]) g_rt.view_x += pan_speed;
    if(kb[SDL_SCANCODE_UP] || kb[SDL_SCANCODE_W]) g_rt.view_y -= pan_speed;
    if(kb[SDL_SCANCODE_DOWN] || kb[SDL_SCANCODE_S]) g_rt.view_y += pan_speed;
    /* Tag filtering: number keys 1..9 set single-bit filter; 0 clears */
    for(int d=1; d<=9; ++d){ if(kb[SDL_SCANCODE_1 + (d-1)]){ g_rt.filter_mask = 1u << (d-1); break; } }
    if(kb[SDL_SCANCODE_0]) g_rt.filter_mask = 0;
    /* Undo (Ctrl+Z) */
    if( (kb[SDL_SCANCODE_LCTRL]||kb[SDL_SCANCODE_RCTRL]) && kb[SDL_SCANCODE_Z]){
        static int undo_consumed=0; /* simple edge guard */
        if(!undo_consumed && g_rt.undo_count>0){
            g_rt.undo_count--;
            int sid = g_rt.undo[g_rt.undo_count].skill_id;
            int prev = g_rt.undo[g_rt.undo_count].prev_rank;
            struct RogueSkillState* st = (struct RogueSkillState*)rogue_skill_get_state(sid);
            if(st){ int delta = st->rank - prev; if(delta>0){ st->rank = prev; g_app.talent_points += delta; rogue_skills_recompute_synergies(); rogue_persistence_save_player_stats(); } }
        }
        undo_consumed=1;
    } else {
        /* reset edge */
    }
    /* Mouse click allocation (left click) */
    int left_down = (mstate & SDL_BUTTON(SDL_BUTTON_LEFT))?1:0;
    if(left_down && !g_rt.last_mouse_down){
        /* Hit-test existing sprite nodes (kind==3) for click */
        int node_count=0; const RogueUINode* nodes = rogue_ui_nodes(&g_skill_ui,&node_count);
        for(int i=0;i<node_count;i++){
            const RogueUINode* n=&nodes[i]; if(n->kind==3){ /* sprite */
                float x=n->rect.x, y=n->rect.y, w=n->rect.w, h=n->rect.h;
                if(mx>=x && my>=y && mx<=x+w && my<=y+h){
                    int sid = n->data_i0;
                    /* Attempt rank up */
                    int prev_rank = -1; const RogueSkillState* st_before = rogue_skill_get_state(sid); if(st_before) prev_rank = st_before->rank;
                    int new_rank = rogue_skill_rank_up(sid);
                    if(new_rank>=0 && prev_rank>=0 && new_rank>prev_rank){
                        if(g_rt.undo_count < (int)(sizeof(g_rt.undo)/sizeof(g_rt.undo[0]))){ g_rt.undo[g_rt.undo_count].skill_id=sid; g_rt.undo[g_rt.undo_count].prev_rank=prev_rank; g_rt.undo_count++; }
                        rogue_ui_skillgraph_pulse(&g_skill_ui,sid);
                        rogue_ui_skillgraph_spend_flyout(&g_skill_ui,sid,1);
                    }
                    break; /* only one */
                }
            }
        }
    }
    g_rt.last_mouse_down = left_down;
#endif
}

void rogue_skillgraph_runtime_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.show_skill_graph) return;
    if(!g_skill_ui_inited) rogue_skillgraph_runtime_init();
    if(!g_skill_ui_inited) return;
    rogue_ui_begin(&g_skill_ui, g_app.dt * 1000.0);
    /* Build first so we have nodes for click hit-testing; we will process input then rebuild to reflect changes */
    build_live_graph(&g_skill_ui);
    runtime_skillgraph_handle_input();
    /* Rebuild after potential rank changes to update ranks in UI */
    build_live_graph(&g_skill_ui);
    /* Synergy panel & info (constructed while frame active) */
    if(g_skill_ui.skillgraph_synergy_panel_enabled){
        /* Simple aggregate list (first 8 synergy ids) */
        int x=15, y=15; int w=180; int h=14*10 + 40; /* rough */
        rogue_ui_panel(&g_skill_ui,(RogueUIRect){(float)x,(float)y,(float)w,(float)h},0x202028E0u);
        char line[96]; int ly=y+4;
        rogue_ui_text_dup(&g_skill_ui,(RogueUIRect){(float)(x+6),(float)ly,(float)(w-12),12},"Synergies",0xFFFFFFFFu); ly+=14;
        for(int s=0;s<8;s++){ int total = rogue_skill_synergy_total(s); if(total){ snprintf(line,sizeof line,"S%d: %d",s,total); rogue_ui_text_dup(&g_skill_ui,(RogueUIRect){(float)(x+6),(float)ly,(float)(w-12),12},line,0x90E0FFFFu); ly+=12; } }
        ly+=4;
        snprintf(line,sizeof line,"Talent Pts: %d", g_app.talent_points); rogue_ui_text_dup(&g_skill_ui,(RogueUIRect){(float)(x+6),(float)ly,(float)(w-12),12},line,0xFFFFFFFFu); ly+=12;
        unsigned int fm = g_rt.filter_mask; if(fm){ snprintf(line,sizeof line,"Filter bit: %u", (unsigned) (log2((double)fm)+0.5)); } else { snprintf(line,sizeof line,"Filter: (none)"); }
        rogue_ui_text_dup(&g_skill_ui,(RogueUIRect){(float)(x+6),(float)ly,(float)(w-12),12},line,0xFFFFFFFFu); ly+=12;
        rogue_ui_text_dup(&g_skill_ui,(RogueUIRect){(float)(x+6),(float)ly,(float)(w-12),12},"1-9=Filter 0=Clear",0x8080FFFFu); ly+=12;
        rogue_ui_text_dup(&g_skill_ui,(RogueUIRect){(float)(x+6),(float)ly,(float)(w-12),12},"+=Zoom - =Zoom",0x8080FFFFu); ly+=12;
        rogue_ui_text_dup(&g_skill_ui,(RogueUIRect){(float)(x+6),(float)ly,(float)(w-12),12},"LClick=Rank Up",0x8080FFFFu); ly+=12;
        rogue_ui_text_dup(&g_skill_ui,(RogueUIRect){(float)(x+6),(float)ly,(float)(w-12),12},"Ctrl+Z=Undo",0x8080FFFFu);
    }
    rogue_ui_end(&g_skill_ui);
    int count=0; const RogueUINode* nodes = rogue_ui_nodes(&g_skill_ui,&count);
    for(int i=0;i<count;i++){
        const RogueUINode* n=&nodes[i];
        if(n->kind==0){ /* panel */
            Uint8 r=(Uint8)(n->color>>24); Uint8 g=(Uint8)(n->color>>16); Uint8 b=(Uint8)(n->color>>8); Uint8 a=(Uint8)(n->color);
            SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,r,g,b,a);
            SDL_Rect rc={(int)n->rect.x,(int)n->rect.y,(int)n->rect.w,(int)n->rect.h};
            SDL_RenderFillRect(g_internal_sdl_renderer_ref,&rc);
        } else if(n->kind==1 && n->text){ /* text */
            rogue_font_draw_text((int)n->rect.x,(int)n->rect.y,n->text,1,(RogueColor){255,255,255,255});
        } else if(n->kind==3){ /* sprite (skill icon) */
            int skill_id = n->data_i0; /* sheet id we overloaded with skill index */
            if(g_app.skill_icon_textures && skill_id>=0 && skill_id<g_app.skill_count){
                RogueTexture* tex = &g_app.skill_icon_textures[skill_id];
                if(tex->handle){
                    SDL_Rect dst={(int)n->rect.x,(int)n->rect.y,(int)n->rect.w,(int)n->rect.h};
                    SDL_RenderCopy(g_internal_sdl_renderer_ref, tex->handle, NULL, &dst);
                }
            }
        }
    }
#endif
}
