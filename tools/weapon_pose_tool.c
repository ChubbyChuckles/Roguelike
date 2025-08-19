#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define FRAME_COUNT 8

typedef struct FramePose { float dx, dy, angle, scale, pivot_x, pivot_y; } FramePose;
static FramePose poses[FRAME_COUNT];
static int frame_index = 0;
static int dirty_flag = 0;
static SDL_Texture* weapon_tex[FRAME_COUNT];
static int weapon_w[FRAME_COUNT], weapon_h[FRAME_COUNT];
static SDL_Texture* player_ref = NULL; /* optional */
static int player_w=0, player_h=0;
static const char* weapon_id = "weapon";
static const char* out_path = NULL;

static void usage(void){
    printf("weapon_pose_tool --weapon <id> [--player-sheet path] [--out path]\n");
}

static int load_texture(SDL_Renderer* r, const char* path, SDL_Texture** out_tex, int* w, int* h){
    SDL_Surface* s = SDL_LoadBMP(path); /* fallback: allow simple BMP; PNG requires SDL_image */
    if(!s){ fprintf(stderr, "WARN: load fail %s (%s)\n", path, SDL_GetError()); return 0; }
    SDL_Texture* t = SDL_CreateTextureFromSurface(r,s);
    if(!t){ fprintf(stderr, "WARN: texture create fail %s (%s)\n", path, SDL_GetError()); SDL_FreeSurface(s); return 0; }
    *w = s->w; *h = s->h; *out_tex = t; SDL_FreeSurface(s); return 1;
}

static void ensure_frames(SDL_Renderer* r){
    for(int i=0;i<FRAME_COUNT;i++){
        if(!weapon_tex[i]){
            char path[256]; snprintf(path,sizeof path,"assets/weapons/%s_f%d.bmp", weapon_id, i);
            load_texture(r,path,&weapon_tex[i],&weapon_w[i],&weapon_h[i]);
            if(!weapon_tex[i] && i>0){ /* fallback */ weapon_tex[i]=weapon_tex[i-1]; weapon_w[i]=weapon_w[i-1]; weapon_h[i]=weapon_h[i-1]; }
        }
        if(poses[i].scale==0) poses[i].scale = 1.0f;
        if(poses[i].pivot_x==0 && poses[i].pivot_y==0){ poses[i].pivot_x=0.5f; poses[i].pivot_y=0.5f; }
    }
}

static void save_json(const char* path){
    if(!path) return; FILE* f=NULL; fopen_s(&f,path,"wb"); if(!f){ fprintf(stderr,"ERR: save fail %s\n", path); return; }
    fprintf(f,"{\n  \"weapon_id\": \"%s\",\n  \"frames\": [\n", weapon_id);
    for(int i=0;i<FRAME_COUNT;i++){
        FramePose* p=&poses[i];
        fprintf(f,"    { \"dx\": %.4f, \"dy\": %.4f, \"angle\": %.4f, \"scale\": %.4f, \"pivot_x\": %.4f, \"pivot_y\": %.4f }%s\n",
            p->dx, p->dy, p->angle, p->scale, p->pivot_x, p->pivot_y, (i==FRAME_COUNT-1)?"":",");
    }
    fprintf(f,"  ]\n}\n"); fclose(f); dirty_flag = 0; printf("Saved %s\n", path);
}

static void handle_key(const SDL_Event* e){
    int shift = (SDL_GetModState() & KMOD_SHIFT)!=0;
    int ctrl = (SDL_GetModState() & KMOD_CTRL)!=0;
    FramePose* p = &poses[frame_index];
    float move = shift?5.0f:1.0f; float rot = shift?10.0f:2.0f; float scl = 0.05f;
    switch(e->key.keysym.sym){
        case SDLK_PAGEUP: frame_index=(frame_index+FRAME_COUNT-1)%FRAME_COUNT; break;
        case SDLK_PAGEDOWN: frame_index=(frame_index+1)%FRAME_COUNT; break;
        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8:
            frame_index = e->key.keysym.sym - SDLK_1; break;
        case SDLK_LEFT:
            if(ctrl) { p->pivot_x -= 0.01f; if(p->pivot_x<0) p->pivot_x=0; }
            else { p->dx -= move; }
            dirty_flag=1; break;
        case SDLK_RIGHT:
            if(ctrl) { p->pivot_x += 0.01f; if(p->pivot_x>1) p->pivot_x=1; }
            else { p->dx += move; }
            dirty_flag=1; break;
        case SDLK_UP:
            if(ctrl){ p->pivot_y -= 0.01f; if(p->pivot_y<0) p->pivot_y=0; }
            else { p->dy -= move; }
            dirty_flag=1; break;
        case SDLK_DOWN:
            if(ctrl){ p->pivot_y += 0.01f; if(p->pivot_y>1) p->pivot_y=1; }
            else { p->dy += move; }
            dirty_flag=1; break;
        case SDLK_q: p->angle -= rot; dirty_flag=1; break;
        case SDLK_e: p->angle += rot; dirty_flag=1; break;
        case SDLK_z: p->scale -= scl; if(p->scale<0.05f)p->scale=0.05f; dirty_flag=1; break;
        case SDLK_x: p->scale += scl; dirty_flag=1; break;
        case SDLK_s: save_json(out_path?out_path:"pose_out.json"); break;
        default: break;
    }
}

int main(int argc, char** argv){
    SDL_SetMainReady();
    const char* player_sheet_path = NULL; out_path = "pose_out.json";
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--weapon")==0 && i+1<argc){ weapon_id=argv[++i]; }
        else if(strcmp(argv[i],"--player-sheet")==0 && i+1<argc){ player_sheet_path=argv[++i]; }
        else if(strcmp(argv[i],"--out")==0 && i+1<argc){ out_path=argv[++i]; }
        else if(strcmp(argv[i],"--help")==0){ usage(); return 0; }
    }
    if(SDL_Init(SDL_INIT_VIDEO)!=0){ fprintf(stderr,"SDL init fail: %s\n", SDL_GetError()); return 1; }
    SDL_Window* win = SDL_CreateWindow("Weapon Pose Tool", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* ren = SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!ren){ fprintf(stderr,"Renderer fail %s\n", SDL_GetError()); return 1; }
    if(player_sheet_path){ load_texture(ren, player_sheet_path, &player_ref, &player_w, &player_h); }
    ensure_frames(ren);
    int running=1; while(running){
        SDL_Event e; while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) running=0; else if(e.type==SDL_KEYDOWN){ if(e.key.keysym.sym==SDLK_ESCAPE) running=0; else handle_key(&e); }
        }
        int w,h; SDL_GetRendererOutputSize(ren,&w,&h);
        SDL_SetRenderDrawColor(ren,18,18,20,255); SDL_RenderClear(ren);
        /* Draw player reference centered */
        int origin_x = w/2; int origin_y = h/2;
        if(player_ref){ SDL_Rect dst={origin_x-player_w/2, origin_y-player_h/2, player_w, player_h}; SDL_RenderCopy(ren,player_ref,NULL,&dst); }
        /* Draw weapon frame */
        FramePose* p=&poses[frame_index]; SDL_Texture* wt = weapon_tex[frame_index]; int ww=weapon_w[frame_index]; int wh=weapon_h[frame_index];
        SDL_FRect dst; dst.w = ww * p->scale; dst.h = wh * p->scale; dst.x = origin_x + p->dx - dst.w * p->pivot_x; dst.y = origin_y + p->dy - dst.h * p->pivot_y;
        SDL_FPoint center = { dst.w * p->pivot_x, dst.h * p->pivot_y };
        if(wt){ SDL_RenderCopyExF(ren, wt, NULL, &dst, p->angle, &center, SDL_FLIP_NONE); }
        /* Crosshair */
        SDL_SetRenderDrawColor(ren,255,255,0,255); SDL_RenderDrawLine(ren, origin_x-10, origin_y, origin_x+10, origin_y); SDL_RenderDrawLine(ren, origin_x, origin_y-10, origin_y, origin_y+10);
        SDL_RenderPresent(ren);
    }
    SDL_Quit();
    return 0;
}
