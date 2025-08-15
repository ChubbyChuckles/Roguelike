/* Runtime integration layer for experimental UI skill graph (Phase 5) */
#include "core/app_state.h"
#include "core/skills.h"
#include "ui/core/ui_context.h"
#include <stdio.h>
#include <math.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
extern SDL_Renderer* g_internal_sdl_renderer_ref; /* provided by renderer.c */
extern void rogue_font_draw_text(int x,int y,const char* text,int small, RogueColor color);
#endif

/* Local static UI context (independent of existing legacy HUD rendering). */
static RogueUIContext g_skill_ui;
static int g_skill_ui_inited = 0;

void rogue_skillgraph_runtime_init(void){
    if(g_skill_ui_inited) return;
    RogueUIContextConfig cfg = { .max_nodes = 4096, .seed = 1337u, .arena_size = 128*1024 };
    if(!rogue_ui_init(&g_skill_ui, &cfg)) return;
    g_skill_ui_inited = 1;
}

/* Build graph from actual registered skills; arrange in radial layers by tag groups. */
static void build_live_graph(RogueUIContext* ui){
    if(g_app.skill_count<=0) return;
    float view_w = (float)g_app.viewport_w * 0.6f;
    float view_h = (float)g_app.viewport_h * 0.6f;
    float view_x = -view_w*0.1f; float view_y = -view_h*0.1f; /* slight offset */
    float zoom = 1.0f;
    rogue_ui_skillgraph_begin(ui, view_x, view_y, view_w, view_h, zoom);
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

void rogue_skillgraph_runtime_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.show_skill_graph) return;
    if(!g_skill_ui_inited) rogue_skillgraph_runtime_init();
    if(!g_skill_ui_inited) return;
    rogue_ui_begin(&g_skill_ui, g_app.dt * 1000.0);
    build_live_graph(&g_skill_ui);
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
