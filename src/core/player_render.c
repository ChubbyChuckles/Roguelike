#include "core/player_render.h"
#include "graphics/sprite.h"
#include "graphics/font.h"
#include <math.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_player_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return; if(!g_app.player_loaded) return; int tsz=g_app.tile_size; int scale=1; int dir=g_app.player.facing; int sheet_dir=(dir==1||dir==2)?1:dir; int render_state=g_app.player_state; if(g_app.player_combat.phase==ROGUE_ATTACK_WINDUP || g_app.player_combat.phase==ROGUE_ATTACK_STRIKE || g_app.player_combat.phase==ROGUE_ATTACK_RECOVER) render_state=3; const RogueSprite* spr=&g_app.player_frames[render_state][sheet_dir][g_app.player.anim_frame]; if(!spr->sw){ for(int f=0; f<8; f++){ if(g_app.player_frames[render_state][sheet_dir][f].sw){ spr=&g_app.player_frames[render_state][sheet_dir][f]; break; } } }
    if(spr->sw && spr->tex && spr->tex->handle){ int px=(int)(g_app.player.base.pos.x * tsz * scale - g_app.cam_x); int py=(int)(g_app.player.base.pos.y * tsz * scale - g_app.cam_y); if(g_app.levelup_aura_timer_ms>0.0f){ g_app.levelup_aura_timer_ms -= (float)(g_app.dt * 1000.0); float tnorm = g_app.levelup_aura_timer_ms / 2000.0f; if(tnorm<0) tnorm=0; if(tnorm>1) tnorm=1; float pulse=0.5f + 0.5f * (float)sin((2000.0f - g_app.levelup_aura_timer_ms)*0.025f); int radius=(int)(spr->sw*scale * (1.2f + 0.3f*(1.0f-tnorm))); int cx=px + spr->sw*scale/2; int cy=py + spr->sh*scale/2; unsigned char cr=(unsigned char)(120 + 90*pulse); unsigned char cg=(unsigned char)(80 + 120*pulse); unsigned char cb=(unsigned char)(255); unsigned char ca=(unsigned char)(120*tnorm + 60); SDL_SetRenderDrawColor(g_app.renderer,cr,cg,cb,ca); for(int dy=-radius; dy<=radius; ++dy){ int dx_lim=(int)sqrt(radius*radius - dy*dy); SDL_RenderDrawLine(g_app.renderer,cx-dx_lim,cy+dy,cx+dx_lim,cy+dy); } }
        if(dir==1){ SDL_Rect src={spr->sx,spr->sy,spr->sw,spr->sh}; SDL_Rect dst={ (int)(g_app.player.base.pos.x * tsz * scale - g_app.cam_x), (int)(g_app.player.base.pos.y * tsz * scale - g_app.cam_y), spr->sw*scale, spr->sh*scale }; SDL_RenderCopyEx(g_app.renderer,spr->tex->handle,&src,&dst,0.0,NULL,SDL_FLIP_HORIZONTAL);} else { rogue_sprite_draw(spr,(int)(g_app.player.base.pos.x*tsz*scale - g_app.cam_x),(int)(g_app.player.base.pos.y*tsz*scale - g_app.cam_y),scale);} }
    else { SDL_SetRenderDrawColor(g_app.renderer,255,0,255,255); SDL_Rect pr={ (int)(g_app.player.base.pos.x*tsz*scale - g_app.cam_x), (int)(g_app.player.base.pos.y*tsz*scale - g_app.cam_y), g_app.player_frame_size*scale, g_app.player_frame_size*scale }; SDL_RenderFillRect(g_app.renderer,&pr); }
#endif
}
