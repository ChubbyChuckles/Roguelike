#include "core/enemy_render.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_enemy_render(void){
#ifdef ROGUE_HAVE_SDL
    int tsz = g_app.tile_size;
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++) if(g_app.enemies[i].alive){
        RogueEnemy* e=&g_app.enemies[i]; RogueEnemyTypeDef* t=&g_app.enemy_types[e->type_index];
        int ex=(int)(e->base.pos.x*tsz - g_app.cam_x);
        int ey=(int)(e->base.pos.y*tsz - g_app.cam_y);
        RogueSprite* frames=NULL; int fcount=0;
        if(e->ai_state==ROGUE_ENEMY_AI_AGGRO){ if(t->run_count>0){ frames=t->run_frames; fcount=t->run_count; } else { frames=t->idle_frames; fcount=t->idle_count; } }
        else if(e->ai_state==ROGUE_ENEMY_AI_PATROL){ frames=t->idle_frames; fcount=t->idle_count; }
        else { frames=t->death_frames; fcount=t->death_count; }
        RogueSprite* spr = (frames && fcount)? &frames[e->anim_frame % fcount] : NULL;
        if(spr && spr->tex && spr->tex->handle && spr->sw){
            Uint8 mr=(Uint8)(e->tint_r<0?0:(e->tint_r>255?255:e->tint_r));
            Uint8 mg=(Uint8)(e->tint_g<0?0:(e->tint_g>255?255:e->tint_g));
            Uint8 mb=(Uint8)(e->tint_b<0?0:(e->tint_b>255?255:e->tint_b));
            Uint8 ma=255; if(e->ai_state==ROGUE_ENEMY_AI_DEAD){ ma=(Uint8)(e->death_fade<0?0:(e->death_fade>1.0f?255:(e->death_fade*255.0f))); }
            SDL_SetTextureColorMod(spr->tex->handle,mr,mg,mb);
            SDL_SetTextureAlphaMod(spr->tex->handle,ma);
            SDL_Rect src={spr->sx,spr->sy,spr->sw,spr->sh};
            SDL_Rect dst={ex - spr->sw/2, ey - spr->sh/2, spr->sw, spr->sh};
            SDL_RenderCopyEx(g_app.renderer,spr->tex->handle,&src,&dst,0.0,NULL,(e->facing==1)? SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE);
            SDL_SetTextureColorMod(spr->tex->handle,255,255,255);
            SDL_SetTextureAlphaMod(spr->tex->handle,255);
        } else {
            unsigned char r=(unsigned char)e->tint_r,g=(unsigned char)e->tint_g,b=(unsigned char)e->tint_b,a=255; if(e->ai_state==ROGUE_ENEMY_AI_DEAD) a=(unsigned char)(e->death_fade*255.0f);
            SDL_SetRenderDrawColor(g_app.renderer,r,g,b,a); SDL_Rect er={ex-4,ey-4,8,8}; SDL_RenderFillRect(g_app.renderer,&er);
        }
        int maxhp = e->max_health>0? e->max_health:1; float ratio = (e->health>0)? (float)e->health/(float)maxhp : 0.0f; if(ratio<0) ratio=0; if(ratio>1) ratio=1;
        int barw=20, barh=3; int bx=ex-barw/2; int by=ey - (spr? spr->sh/2 : 6) - 6;
        SDL_SetRenderDrawColor(g_app.renderer,25,8,8,200); SDL_Rect bg={bx-1,by-1,barw+2,barh+2}; SDL_RenderFillRect(g_app.renderer,&bg);
        SDL_SetRenderDrawColor(g_app.renderer,120,0,0,255); SDL_Rect fg1={bx,by,(int)(barw*ratio),barh}; SDL_RenderFillRect(g_app.renderer,&fg1);
        SDL_SetRenderDrawColor(g_app.renderer,220,30,30,255); SDL_Rect fg2={bx,by,(int)(barw*ratio*0.55f),barh}; SDL_RenderFillRect(g_app.renderer,&fg2);
        g_app.frame_draw_calls++;
    }
#endif
}
