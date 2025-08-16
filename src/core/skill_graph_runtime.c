/* Runtime integration layer for experimental UI skill graph (Phase 5)
    Wiring of advanced features: persistent pan/zoom, filtering, allocation + undo,
    pulses, spend flyouts, synergy panel, tag filters. */
#include "core/app_state.h"
#include "core/skills.h"
#include "core/skill_maze.h"
#include "ui/core/ui_context.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "core/skill_bar.h"
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
    /* Drag & drop (skill -> skill bar) */
    int drag_active;
    int drag_skill_id;
    float drag_start_x, drag_start_y;
    float drag_cur_x, drag_cur_y;
    int drag_started_move; /* movement threshold passed */
} RuntimeSkillGraphState;
static RuntimeSkillGraphState g_rt = {0};
static RogueSkillMaze g_maze; static int g_maze_built=0;

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
    /* Clear previously emitted panels/sprites so repeated builds in same frame don't layer lines above icons. */
    ui->node_count = 0;
    float view_w = (float)g_app.viewport_w * 0.70f;
    float view_h = (float)g_app.viewport_h * 0.70f;
    /* Freeze to screen center: neutral view; manual zoom applied to coordinates */
    if(g_rt.zoom < 0.5f) g_rt.zoom = 0.5f; if(g_rt.zoom>2.5f) g_rt.zoom=2.5f;
    rogue_ui_skillgraph_begin(ui, 0.0f, 0.0f, view_w, view_h, 1.0f);
    if(g_rt.filter_mask) rogue_ui_skillgraph_set_filter_tags(ui, g_rt.filter_mask); else rogue_ui_skillgraph_set_filter_tags(ui, 0);
    /* Build maze once (could add hot-reload on config changes) */
    if(!g_maze_built){ if(rogue_skill_maze_generate("assets/skill_maze_config.json", &g_maze)) g_maze_built=1; }
    int count = g_app.skill_count;
    if(g_maze_built){
        /* Ensure the view region fully encloses the maze so skill nodes on outer rings are not frustum-culled.
           Coordinate scheme offsets node positions by view_* / 2 for centering, so ensure view_* >= 2*max_abs + margin. */
        float max_abs_x = 0.0f, max_abs_y = 0.0f;
        for(int n=0;n<g_maze.node_count;n++){ float ax=fabsf(g_maze.nodes[n].x); float ay=fabsf(g_maze.nodes[n].y); if(ax>max_abs_x) max_abs_x=ax; if(ay>max_abs_y) max_abs_y=ay; }
    float needed_w = max_abs_x*2.0f + 32.0f; if(view_w < needed_w) view_w = needed_w;
    float needed_h = max_abs_y*2.0f + 32.0f; if(view_h < needed_h) view_h = needed_h;
    /* Aggressively expand view rectangle to avoid any culling misses (temporary until culling coordinate space refactor). */
    view_w *= 2.5f; view_h *= 2.5f;
        /* Re-begin (resize) if dimensions expanded */
        rogue_ui_skillgraph_begin(ui, 0.0f, 0.0f, view_w, view_h, 1.0f);
        /* Full-population assignment with ring-aware multi-pass */
        int* assigned = (int*)malloc(sizeof(int)*g_maze.node_count); if(!assigned) return; for(int i=0;i<g_maze.node_count;i++) assigned[i]=-1; int filled=0; int count_safe = count>0?count:1; int cursor=0;
        for(int pass=0; pass<3 && filled<g_maze.node_count; ++pass){
            for(int n=0;n<g_maze.node_count && filled<g_maze.node_count; ++n){ if(assigned[n]>=0) continue; int ring=g_maze.nodes[n].ring; for(int tries=0; tries<count; ++tries){ int sid=(cursor+tries)%count_safe; const RogueSkillDef* def=rogue_skill_get_def(sid); if(!def) continue; int trg=def->skill_strength; int ok=0; if(trg==0) ok=1; else if(trg==ring) ok=1; else if(trg>g_maze.rings && ring==g_maze.rings) ok=1; else if(pass>=1){ if(pass==1 && (trg==ring-1 || trg==ring+1)) ok=1; else if(pass==2) ok=1; }
                    if(ok){ assigned[n]=sid; filled++; cursor=(sid+1)%count_safe; break; }
                }
                if(assigned[n]<0 && count>0){ assigned[n]=(n+cursor)%count_safe; filled++; }
            }
        }
        for(int n=0;n<g_maze.node_count;n++){ if(assigned[n]<0) assigned[n]= n % count_safe; }
        /* Connected line rendering already provided by reverted repo: keep algorithm but anchor to screen center independent of player. */
        float z = g_rt.zoom; float center_x = view_w*0.5f; float center_y = view_h*0.5f;
        for(int e=0;e<g_maze.edge_count;e++){ int a=g_maze.edges[e].from; int b=g_maze.edges[e].to; if((unsigned)a>=(unsigned)g_maze.node_count||(unsigned)b>=(unsigned)g_maze.node_count) continue; float ax=center_x + g_maze.nodes[a].x * z; float ay=center_y + g_maze.nodes[a].y * z; float bx=center_x + g_maze.nodes[b].x * z; float by=center_y + g_maze.nodes[b].y * z; float dx=bx-ax, dy=by-ay; float len=sqrtf(dx*dx+dy*dy); if(len<2) continue; int steps=(int)(len/3.0f); if(steps<1) steps=1; float inv=1.0f/(float)steps; float thickness=4.0f; float half=thickness*0.5f; for(int s=0;s<=steps;s++){ float t=(float)s*inv; float cx=ax+dx*t; float cy=ay+dy*t; rogue_ui_panel(ui,(RogueUIRect){cx-half,cy-half,thickness,thickness},0x303030D0u);} }
        for(int n=0;n<g_maze.node_count;n++){ int sid=assigned[n]; float cx=view_w*0.5f + g_maze.nodes[n].x * z; float cy=view_h*0.5f + g_maze.nodes[n].y * z; if(sid>=0){ const RogueSkillDef* def=rogue_skill_get_def(sid); const RogueSkillState* st=rogue_skill_get_state(sid); if(def&&st){ unsigned int tags=(unsigned int)def->tags; int synergy=def->is_passive && def->synergy_id>=0; rogue_ui_skillgraph_add(ui,cx,cy,sid,st->rank,def->max_rank,synergy,tags);} else { rogue_ui_panel(ui,(RogueUIRect){cx-4,cy-4,8,8},0xFF0000FFu);} } else { rogue_ui_panel(ui,(RogueUIRect){cx-4,cy-4,8,8},0xFF00FF80u);} }
        for(int n=0;n<g_maze.node_count;n++){ int sid=assigned[n]; float cx=view_w*0.5f + g_maze.nodes[n].x * z; float cy=view_h*0.5f + g_maze.nodes[n].y * z; if(sid>=0){ const RogueSkillDef* def=rogue_skill_get_def(sid); const RogueSkillState* st=rogue_skill_get_state(sid); if(def&&st){ unsigned int tags=(unsigned int)def->tags; int synergy=def->is_passive && def->synergy_id>=0; /* Larger icon scale: apply extra zoom factor */ rogue_ui_skillgraph_add(ui,cx,cy,sid,st->rank,def->max_rank,synergy,tags);} else { rogue_ui_panel(ui,(RogueUIRect){cx-4,cy-4,8,8},0xFF0000FFu);} } else { rogue_ui_panel(ui,(RogueUIRect){cx-4,cy-4,8,8},0xFF00FF80u);} }
        int maze_nodes_debug = g_maze.node_count;
        free(assigned);
        /* Build skill graph visible nodes (culling) */
    int visible = rogue_ui_skillgraph_build(ui);
        /* Instrumentation overlay (throttled logging) */
        static double last_log_ms = 0.0;
        if(g_app.game_time_ms - last_log_ms > 1000.0){
            fprintf(stderr,"SKILLGRAPH DBG maze_nodes=%d skills_total=%d visible=%d filter_mask=0x%X view_w=%.1f view_h=%.1f zoom=%.2f\n",
                maze_nodes_debug, g_app.skill_count, visible, g_rt.filter_mask, view_w, view_h, g_rt.zoom);
            last_log_ms = g_app.game_time_ms;
        }
        char dbg[96]; snprintf(dbg,sizeof dbg,"Skills: %d/%d vis zoom=%.2f", visible, maze_nodes_debug, g_rt.zoom);
        rogue_ui_panel(ui,(RogueUIRect){8,(float)g_app.viewport_h - 140.0f,140,24},0x101018C0u);
        rogue_ui_text_dup(ui,(RogueUIRect){12,(float)g_app.viewport_h - 132.0f,132,14},dbg,0xFFFFFFFFu);
        return; /* early since we've already built UI skill graph */
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
    /* Hot-reload maze (R) */
    {
        static int reload_consumed=0;
        if(kb[SDL_SCANCODE_R]){
            if(!reload_consumed){ if(g_maze_built){ rogue_skill_maze_free(&g_maze); g_maze_built=0; fprintf(stderr,"SKILLGRAPH reload requested (R)\n"); } reload_consumed=1; }
        } else reload_consumed=0;
    }
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
    /* Drag begin */
    if(left_down && !g_rt.last_mouse_down && !g_rt.drag_active){
        int node_count=0; const RogueUINode* nodes = rogue_ui_nodes(&g_skill_ui,&node_count);
        for(int i=0;i<node_count;i++){
            const RogueUINode* n=&nodes[i]; if(n->kind==3){ /* sprite */
                float x=n->rect.x, y=n->rect.y, w=n->rect.w, h=n->rect.h;
                if(mx>=x && my>=y && mx<=x+w && my<=y+h){
                    g_rt.drag_active=1; g_rt.drag_skill_id=n->data_i0; g_rt.drag_start_x=(float)mx; g_rt.drag_start_y=(float)my; g_rt.drag_cur_x=(float)mx; g_rt.drag_cur_y=(float)my; g_rt.drag_started_move=0; break;
                }
            }
        }
    }
    /* Update drag */
    if(g_rt.drag_active && left_down){ g_rt.drag_cur_x=(float)mx; g_rt.drag_cur_y=(float)my; float dx=g_rt.drag_cur_x-g_rt.drag_start_x; float dy=g_rt.drag_cur_y-g_rt.drag_start_y; if(!g_rt.drag_started_move && (dx*dx+dy*dy)>16.0f) g_rt.drag_started_move=1; }
    /* Drag end / click rank up */
    if(!left_down && g_rt.last_mouse_down && g_rt.drag_active){
        int performed_action=0;
        /* Determine if release over skill bar */
        int bar_w = 10*34 + 8; int bar_h = 46; int bar_x = 4; int bar_y = g_app.viewport_h - bar_h - 4;
        if(g_rt.drag_started_move && mx>=bar_x && mx<bar_x+bar_w && my>=bar_y && my<bar_y+bar_h){
            int local_x = mx - (bar_x + 6); if(local_x>=0){ int slot = local_x / 34; if(slot>=0 && slot<10){ rogue_skill_bar_set_slot(slot, g_rt.drag_skill_id); rogue_skill_bar_flash(slot); performed_action=1; }}
        }
        if(!performed_action && !g_rt.drag_started_move){
            /* Treat as rank up click */
            int sid = g_rt.drag_skill_id; int prev_rank = -1; const RogueSkillState* st_before = rogue_skill_get_state(sid); if(st_before) prev_rank = st_before->rank; int new_rank = rogue_skill_rank_up(sid);
            if(new_rank>=0 && prev_rank>=0 && new_rank>prev_rank){ if(g_rt.undo_count < (int)(sizeof(g_rt.undo)/sizeof(g_rt.undo[0]))){ g_rt.undo[g_rt.undo_count].skill_id=sid; g_rt.undo[g_rt.undo_count].prev_rank=prev_rank; g_rt.undo_count++; } rogue_ui_skillgraph_pulse(&g_skill_ui,sid); rogue_ui_skillgraph_spend_flyout(&g_skill_ui,sid,1); }
        }
        g_rt.drag_active=0; g_rt.drag_skill_id=-1;
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
    /* Render drag ghost & slot highlight */
    if(g_rt.drag_active){
        int bar_w = 10*34 + 8; int bar_h = 46; int bar_x = 4; int bar_y = g_app.viewport_h - bar_h - 4;
        int mx=(int)g_rt.drag_cur_x, my=(int)g_rt.drag_cur_y;
        /* Ghost */
        SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,200,200,255,160);
        SDL_Rect ghost={mx-16,my-16,32,32}; SDL_RenderFillRect(g_internal_sdl_renderer_ref,&ghost);
        /* Highlight slot under cursor */
        if(mx>=bar_x && mx<bar_x+bar_w && my>=bar_y && my<bar_y+bar_h){ int local_x = mx - (bar_x + 6); if(local_x>=0){ int slot=local_x/34; if(slot>=0 && slot<10){ int slot_x = bar_x + 6 + slot*34; SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,255,200,60,180); SDL_Rect hi={slot_x,bar_y+6,32,32}; SDL_RenderDrawRect(g_internal_sdl_renderer_ref,&hi); }} }
    }
#endif
}
