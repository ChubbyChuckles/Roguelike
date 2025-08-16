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
    float drag_w, drag_h; /* size of dragged icon */
    /* Hover tooltip timing */
    int hover_skill_id; /* currently hovered skill id (sprite under cursor) */
    double hover_start_ms; /* when cursor first moved onto this skill */
    /* Per-frame centering offset applied at render + hit-test so graph stays on screen */
    float render_offset_x;
    float render_offset_y;
    /* Debug bbox of sprites before offset */
    float bbox_minx, bbox_miny, bbox_maxx, bbox_maxy;
    int auto_fit_active; /* when set, nodes already centered & scaled */
} RuntimeSkillGraphState;
static RuntimeSkillGraphState g_rt = {0};
static RogueSkillMaze g_maze; static int g_maze_built=0;

void rogue_skillgraph_runtime_init(void){
    if(g_skill_ui_inited) return;
    RogueUIContextConfig cfg = { .max_nodes = 4096, .seed = 1337u, .arena_size = 128*1024 };
    if(!rogue_ui_init(&g_skill_ui, &cfg)) return;
    g_skill_ui_inited = 1;
    /* Disable synergy panel for minimalist skill graph */
    rogue_ui_skillgraph_enable_synergy_panel(&g_skill_ui,0);
    g_rt.initialized=1; g_rt.view_x=0.0f; g_rt.view_y=0.0f; g_rt.zoom=1.0f; g_rt.filter_mask=0; g_rt.undo_count=0; g_rt.last_mouse_down=0; g_rt.drag_w=32.0f; g_rt.drag_h=32.0f; g_rt.hover_skill_id=-1; g_rt.hover_start_ms=0.0; g_rt.render_offset_x=0.0f; g_rt.render_offset_y=0.0f;
}

/* Build graph from actual registered skills; arrange in radial layers by tag groups. */
static void build_live_graph(RogueUIContext* ui){
    if(g_app.skill_count<=0) return;
    /* Clear previously emitted panels/sprites so repeated builds in same frame don't layer lines above icons. */
    ui->node_count = 0;
    /* Start with viewport dimensions; we'll compute auto-fit zoom */
    float view_w = (float)g_app.viewport_w;
    float view_h = (float)g_app.viewport_h;
    rogue_ui_skillgraph_begin(ui, 0.0f, 0.0f, view_w, view_h, 1.0f);
    if(g_rt.filter_mask) rogue_ui_skillgraph_set_filter_tags(ui, g_rt.filter_mask); else rogue_ui_skillgraph_set_filter_tags(ui, 0);
    /* Build maze once (could add hot-reload on config changes) */
    if(!g_maze_built){ if(rogue_skill_maze_generate("assets/skill_maze_config.json", &g_maze)) g_maze_built=1; }
    int count = g_app.skill_count;
    if(g_maze_built){
    /* Compute raw maze extents (node coordinates) */
    float min_raw_x=1e9f,max_raw_x=-1e9f,min_raw_y=1e9f,max_raw_y=-1e9f;
    for(int n=0;n<g_maze.node_count;n++){ float x=g_maze.nodes[n].x; float y=g_maze.nodes[n].y; if(x<min_raw_x)min_raw_x=x; if(x>max_raw_x)max_raw_x=x; if(y<min_raw_y)min_raw_y=y; if(y>max_raw_y)max_raw_y=y; }
    /* Assume icon size 32 and edge thickness 4 add padding to raw extents */
    const float ICON_SZ = 32.0f; const float EDGE_THICK = 4.0f; const float PAD = ICON_SZ*0.5f + EDGE_THICK*0.5f;
    float raw_w = (max_raw_x - min_raw_x) + PAD*2.0f;
    float raw_h = (max_raw_y - min_raw_y) + PAD*2.0f;
    /* Available size with 20px margin on each side */
    float avail_w = (float)g_app.viewport_w - 40.0f;
    float avail_h = (float)g_app.viewport_h - 40.0f;
    if(avail_w < 40.0f) avail_w = 40.0f; if(avail_h < 40.0f) avail_h = 40.0f;
    float fit_z_w = avail_w / raw_w;
    float fit_z_h = avail_h / raw_h;
    float fit_z = fit_z_w < fit_z_h ? fit_z_w : fit_z_h;
    if(fit_z <= 0.0f) fit_z = 1.0f;
    g_rt.zoom = fit_z; /* override manual zoom: always fit */
    g_rt.auto_fit_active = 1;
    /* Re-begin so internal view uses current viewport (no oversized virtual space needed) */
    rogue_ui_skillgraph_begin(ui, 0.0f, 0.0f, (float)g_app.viewport_w, (float)g_app.viewport_h, 1.0f);
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
        /* Draw edges as dotted panels (screen-space) */
        float z = g_rt.zoom; float center_x = (float)g_app.viewport_w*0.5f; float center_y = (float)g_app.viewport_h*0.5f;
        for(int e=0;e<g_maze.edge_count;e++){
            int a=g_maze.edges[e].from; int b=g_maze.edges[e].to; if((unsigned)a>=(unsigned)g_maze.node_count||(unsigned)b>=(unsigned)g_maze.node_count) continue;
            float ax=center_x + g_maze.nodes[a].x * z; float ay=center_y + g_maze.nodes[a].y * z; float bx=center_x + g_maze.nodes[b].x * z; float by=center_y + g_maze.nodes[b].y * z;
            float dx=bx-ax, dy=by-ay; float len=sqrtf(dx*dx+dy*dy); if(len<2) continue; int steps=(int)(len/3.0f); if(steps<1) steps=1; float inv=1.0f/(float)steps; float thickness=4.0f; float half=thickness*0.5f;
            for(int s=0;s<=steps;s++){ float t=(float)s*inv; float cx=ax+dx*t; float cy=ay+dy*t; rogue_ui_panel(ui,(RogueUIRect){cx-half,cy-half,thickness,thickness},0x303030D0u);} }
        /* Emit one sprite per node (duplicate loop removed) */
        for(int n=0;n<g_maze.node_count;n++){
            int sid=assigned[n]; float cx=view_w*0.5f + g_maze.nodes[n].x * z; float cy=view_h*0.5f + g_maze.nodes[n].y * z; if(sid>=0){
                if(sid < g_app.skill_count){ const RogueSkillDef* def=rogue_skill_get_def(sid); const RogueSkillState* st=rogue_skill_get_state(sid); if(def&&st){ unsigned int tags=(unsigned int)def->tags; int synergy=def->is_passive && def->synergy_id>=0; rogue_ui_skillgraph_add(ui,cx,cy,sid,st->rank,def->max_rank,synergy,tags);} else { rogue_ui_panel(ui,(RogueUIRect){cx-4,cy-4,8,8},0xFF0000FFu);} } else { /* sid out of range: emit debug panel */ rogue_ui_panel(ui,(RogueUIRect){cx-4,cy-4,8,8},0xFFA000FFu); }
            } else {
                rogue_ui_panel(ui,(RogueUIRect){cx-4,cy-4,8,8},0xFF00FF80u);
            }
        }
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
    /* Debug footer removed */
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
    /* Tag filtering disabled (was bound to number keys and caused icons to vanish) */
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
                float x=n->rect.x + g_rt.render_offset_x; float y=n->rect.y + g_rt.render_offset_y; float w=n->rect.w; float h=n->rect.h;
                if(mx>=x && my>=y && mx<=x+w && my<=y+h){
                    g_rt.drag_active=1; g_rt.drag_skill_id=n->data_i0; g_rt.drag_start_x=(float)mx; g_rt.drag_start_y=(float)my; g_rt.drag_cur_x=(float)mx; g_rt.drag_cur_y=(float)my; g_rt.drag_started_move=0; g_rt.drag_w=w; g_rt.drag_h=h; break;
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
    /* When auto-fit active we already placed maze centered; skip dynamic offset adjustment */
    int pre_cnt=0; const RogueUINode* pre_nodes = rogue_ui_nodes(&g_skill_ui,&pre_cnt);
    if(!g_rt.auto_fit_active){
        float minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f; int have_sprite=0;
        for(int i=0;i<pre_cnt;i++){ const RogueUINode* n=&pre_nodes[i]; if(n->kind==3){ have_sprite=1; if(n->rect.x<minx)minx=n->rect.x; if(n->rect.y<miny)miny=n->rect.y; if(n->rect.x+n->rect.w>maxx)maxx=n->rect.x+n->rect.w; if(n->rect.y+n->rect.h>maxy)maxy=n->rect.y+n->rect.h; }}
        if(have_sprite){ float bw = maxx-minx; float bh = maxy-miny; float desired_cx = g_app.viewport_w*0.5f; float desired_cy = g_app.viewport_h*0.5f; float cur_cx = minx + bw*0.5f; float cur_cy = miny + bh*0.5f; g_rt.render_offset_x = desired_cx - cur_cx; g_rt.render_offset_y = desired_cy - cur_cy; g_rt.bbox_minx=minx; g_rt.bbox_miny=miny; g_rt.bbox_maxx=maxx; g_rt.bbox_maxy=maxy; } else { g_rt.render_offset_x=0.0f; g_rt.render_offset_y=0.0f; g_rt.bbox_minx=g_rt.bbox_miny=0.0f; g_rt.bbox_maxx=g_rt.bbox_maxy=0.0f; }
    } else {
        g_rt.render_offset_x = 0.0f; g_rt.render_offset_y = 0.0f; /* preserve bbox for debug */
    }
    runtime_skillgraph_handle_input();
    /* Rebuild after potential rank changes to update ranks in UI */
    build_live_graph(&g_skill_ui);
    if(!g_rt.auto_fit_active){
        /* Recompute offset after rebuild (in case layout changed due to rank visuals) */
        pre_cnt=0; pre_nodes = rogue_ui_nodes(&g_skill_ui,&pre_cnt); float minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f; int have_sprite=0;
        for(int i=0;i<pre_cnt;i++){ const RogueUINode* n=&pre_nodes[i]; if(n->kind==3){ have_sprite=1; if(n->rect.x<minx)minx=n->rect.x; if(n->rect.y<miny)miny=n->rect.y; if(n->rect.x+n->rect.w>maxx)maxx=n->rect.x+n->rect.w; if(n->rect.y+n->rect.h>maxy)maxy=n->rect.y+n->rect.h; }}
        if(have_sprite){ float bw = maxx-minx; float bh = maxy-miny; float desired_cx = g_app.viewport_w*0.5f; float desired_cy = g_app.viewport_h*0.5f; float cur_cx = minx + bw*0.5f; float cur_cy = miny + bh*0.5f; g_rt.render_offset_x = desired_cx - cur_cx; g_rt.render_offset_y = desired_cy - cur_cy; g_rt.bbox_minx=minx; g_rt.bbox_miny=miny; g_rt.bbox_maxx=maxx; g_rt.bbox_maxy=maxy; } else { g_rt.render_offset_x=0.0f; g_rt.render_offset_y=0.0f; g_rt.bbox_minx=g_rt.bbox_miny=0.0f; g_rt.bbox_maxx=g_rt.bbox_maxy=0.0f; }
    } else {
        g_rt.render_offset_x=0.0f; g_rt.render_offset_y=0.0f;
    }
    /* Synergy/info panel removed for cleaner presentation */
    rogue_ui_end(&g_skill_ui);
    int count=0; const RogueUINode* nodes = rogue_ui_nodes(&g_skill_ui,&count);
    for(int i=0;i<count;i++){
        const RogueUINode* n=&nodes[i];
    if(n->kind==0){ /* panel */
            Uint8 r=(Uint8)(n->color>>24); Uint8 g=(Uint8)(n->color>>16); Uint8 b=(Uint8)(n->color>>8); Uint8 a=(Uint8)(n->color);
            SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,r,g,b,a);
        SDL_Rect rc={(int)(n->rect.x + g_rt.render_offset_x),(int)(n->rect.y + g_rt.render_offset_y),(int)n->rect.w,(int)n->rect.h};
            SDL_RenderFillRect(g_internal_sdl_renderer_ref,&rc);
        } else if(n->kind==1 && n->text){ /* text */
        rogue_font_draw_text((int)(n->rect.x + g_rt.render_offset_x),(int)(n->rect.y + g_rt.render_offset_y),n->text,1,(RogueColor){255,255,255,255});
        } else if(n->kind==3){ /* sprite (skill icon) */
            int skill_id = n->data_i0; /* sheet id we overloaded with skill index */
            if(g_app.skill_icon_textures && skill_id>=0 && skill_id<g_app.skill_count){
                RogueTexture* tex = &g_app.skill_icon_textures[skill_id];
                if(tex->handle){
            SDL_Rect dst={(int)(n->rect.x + g_rt.render_offset_x),(int)(n->rect.y + g_rt.render_offset_y),(int)n->rect.w,(int)n->rect.h};
                    SDL_RenderCopy(g_internal_sdl_renderer_ref, tex->handle, NULL, &dst);
                }
            }
        }
    }

    /* Debug overlay: show offsets & bbox; draw bbox rectangle */
    char dbg[160];
    int ox=(int)g_rt.render_offset_x; int oy=(int)g_rt.render_offset_y;
    SDL_snprintf(dbg,sizeof(dbg),"SkillGraph Center off=(%d,%d) bbox=(%.0f,%.0f)-(%.0f,%.0f)",ox,oy,g_rt.bbox_minx,g_rt.bbox_miny,g_rt.bbox_maxx,g_rt.bbox_maxy);
    rogue_font_draw_text(8,8,dbg,1,(RogueColor){255,255,0,255});
    if(g_rt.bbox_maxx>g_rt.bbox_minx){
        SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,255,255,0,100);
        SDL_Rect bb={(int)(g_rt.bbox_minx+g_rt.render_offset_x),(int)(g_rt.bbox_miny+g_rt.render_offset_y),(int)(g_rt.bbox_maxx-g_rt.bbox_minx),(int)(g_rt.bbox_maxy-g_rt.bbox_miny)};
        SDL_RenderDrawRect(g_internal_sdl_renderer_ref,&bb);
    }
    /* Hover tooltip with delay */
    int mx_hover, my_hover; SDL_GetMouseState(&mx_hover,&my_hover);
    int hover_sid=-1; float hx=0,hy=0,hw=0,hh=0;
    for(int i=count-1;i>=0;i--){ const RogueUINode* n=&nodes[i]; if(n->kind==3){ float x=n->rect.x + g_rt.render_offset_x; float y=n->rect.y + g_rt.render_offset_y; float w=n->rect.w; float h=n->rect.h; if(mx_hover>=x && my_hover>=y && mx_hover<=x+w && my_hover<=y+h){ hover_sid=n->data_i0; hx=x; hy=y; hw=w; hh=h; break; } } }
    double now_ms = g_app.game_time_ms;
    if(hover_sid != g_rt.hover_skill_id){ g_rt.hover_skill_id = hover_sid; g_rt.hover_start_ms = now_ms; }
    const double HOVER_DELAY_MS = 220.0;
    if(hover_sid>=0 && (now_ms - g_rt.hover_start_ms) >= HOVER_DELAY_MS){
        const RogueSkillDef* def = rogue_skill_get_def(hover_sid);
        const RogueSkillState* st = rogue_skill_get_state(hover_sid);
        if(def && st){
            /* Build multiline tooltip lines */
            char line1[96]; snprintf(line1,sizeof line1,"%s  (Rank %d/%d)", def->name?def->name:"Skill", st->rank, def->max_rank);
            char line2[96];
            float cd_total = def->base_cooldown_ms - (st->rank-1)*def->cooldown_reduction_ms_per_rank; if(cd_total < 0) cd_total = 0;
            if(def->is_passive){ snprintf(line2,sizeof line2,"Passive"); }
            else snprintf(line2,sizeof line2,"Cooldown: %.1fs", cd_total/1000.0f);
            char line3[96];
            int any_cost = 0; char costbuf[64]=""; if(def->resource_cost_mana>0){ snprintf(costbuf,sizeof costbuf,"Mana %d", def->resource_cost_mana); any_cost=1; }
            if(def->action_point_cost>0){ char apb[32]; snprintf(apb,sizeof apb, any_cost?", AP %d":"AP %d", def->action_point_cost); size_t have=strlen(costbuf); size_t left = (have<sizeof(costbuf))? sizeof(costbuf)-have-1:0; if(left>0){ size_t j=0; while(j<left && apb[j]){ costbuf[have+j]=apb[j]; j++; } costbuf[have+j]=0; } any_cost=1; }
            if(!any_cost) snprintf(costbuf,sizeof costbuf,"No cost");
            snprintf(line3,sizeof line3,"%s", costbuf);
            /* Determine panel width from longest line */
            int w1=(int)strlen(line1), w2=(int)strlen(line2), w3=(int)strlen(line3); int maxw=w1; if(w2>maxw) maxw=w2; if(w3>maxw) maxw=w3; int panel_w = maxw*6 + 10; int panel_h = 3*14 + 8; /* 3 lines */
            int tx = (int)(hx + hw + 6); int ty = (int)(hy - 6); if(ty<4) ty=4; if(tx + panel_w > g_app.viewport_w) tx = (int)(hx - 6 - panel_w);
            SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,18,18,30,235); SDL_Rect tip={tx,ty,panel_w,panel_h}; SDL_RenderFillRect(g_internal_sdl_renderer_ref,&tip);
            SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,100,100,150,255); SDL_Rect top={tip.x,tip.y,tip.w,2}; SDL_RenderFillRect(g_internal_sdl_renderer_ref,&top);
            rogue_font_draw_text(tip.x+4, tip.y+4, line1,1,(RogueColor){255,255,210,255});
            rogue_font_draw_text(tip.x+4, tip.y+4+14, line2,1,(RogueColor){210,230,255,255});
            rogue_font_draw_text(tip.x+4, tip.y+4+28, line3,1,(RogueColor){200,255,210,255});
        }
    }
    /* Render drag ghost & slot highlight */
    if(g_rt.drag_active){
        int bar_w = 10*34 + 8; int bar_h = 46; int bar_x = 4; int bar_y = g_app.viewport_h - bar_h - 4;
        int mx=(int)g_rt.drag_cur_x, my=(int)g_rt.drag_cur_y;
        /* Ghost: draw actual skill icon (semi-transparent) */
        SDL_Rect ghost={mx-(int)(g_rt.drag_w*0.5f), my-(int)(g_rt.drag_h*0.5f), (int)g_rt.drag_w, (int)g_rt.drag_h};
        int sid = g_rt.drag_skill_id;
        int drew_icon=0;
        if(sid>=0 && sid<g_app.skill_count && g_app.skill_icon_textures){ RogueTexture* tex=&g_app.skill_icon_textures[sid]; if(tex->handle){ SDL_SetTextureAlphaMod(tex->handle, 180); SDL_RenderCopy(g_internal_sdl_renderer_ref, tex->handle, NULL, &ghost); SDL_SetTextureAlphaMod(tex->handle, 255); drew_icon=1; }}
        if(!drew_icon){ SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,200,200,255,160); SDL_RenderFillRect(g_internal_sdl_renderer_ref,&ghost); }
        /* Highlight slot under cursor */
        if(mx>=bar_x && mx<bar_x+bar_w && my>=bar_y && my<bar_y+bar_h){ int local_x = mx - (bar_x + 6); if(local_x>=0){ int slot=local_x/34; if(slot>=0 && slot<10){ int slot_x = bar_x + 6 + slot*34; SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,255,200,60,180); SDL_Rect hi={slot_x,bar_y+6,32,32}; SDL_RenderDrawRect(g_internal_sdl_renderer_ref,&hi); }} }
    }
#endif
}
