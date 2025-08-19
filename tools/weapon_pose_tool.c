#define SDL_MAIN_HANDLED
#include <SDL.h>
#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Reworked tool (Phase 5A update):
    - Single weapon image (BMP) overlaid on 8-frame player slash animation frames extracted from a player sheet (PNG or BMP).
    - Player sheet contains 8 frames laid out horizontally for one direction (down/up/side). Left direction is derived by mirroring side-right in game.
    - This tool lets you tune per-frame dx, dy, angle, scale, pivot for that direction.
*/

#define FRAME_COUNT 8

typedef struct FramePose { float dx, dy, angle, scale, pivot_x, pivot_y; } FramePose;
static FramePose poses[FRAME_COUNT];
static int frame_index = 0;
static int dirty_flag = 0;
static SDL_Texture* weapon_tex_single = NULL; /* single weapon image reused for all frames */
static int weapon_w=0, weapon_h=0;
static SDL_Texture* player_sheet_tex = NULL; /* full animation sheet */
static int player_sheet_w=0, player_sheet_h=0;
static int frame_size = 64; /* width & height per frame (square) */
static const char* weapon_id = "weapon";
static const char* out_path = NULL;
static const char* weapon_image_path = NULL; /* optional explicit weapon image */
static const char* direction_label = "down"; /* user provided direction hint */

static void usage(void){
    printf("weapon_pose_tool --weapon <id> --player-sheet <path_to_sheet_png> [--weapon-image <bmp>] [--frame-size N] [--direction <down|up|side>] [--out path]\n");
}

#ifdef _WIN32
/* Minimal WIC PNG loader (subset) – returns SDL_Texture* */
static SDL_Texture* load_png_wic(SDL_Renderer* r, const char* path, int* w, int* h){
    HRESULT hr; static int com_init=0; if(!com_init){ CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); com_init=1; }
    IWICImagingFactory* factory=NULL; hr = CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(LPVOID*)&factory); if(FAILED(hr)||!factory) return NULL;
    int wlen = MultiByteToWideChar(CP_UTF8,0,path,-1,NULL,0); wchar_t* wpath=(wchar_t*)malloc(sizeof(wchar_t)*wlen); MultiByteToWideChar(CP_UTF8,0,path,-1,wpath,wlen);
    IWICBitmapDecoder* decoder=NULL; hr = factory->lpVtbl->CreateDecoderFromFilename(factory,wpath,NULL,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder); free(wpath); if(FAILED(hr)||!decoder){ factory->lpVtbl->Release(factory); return NULL; }
    IWICBitmapFrameDecode* frame=NULL; hr = decoder->lpVtbl->GetFrame(decoder,0,&frame); if(FAILED(hr)||!frame){ decoder->lpVtbl->Release(decoder); factory->lpVtbl->Release(factory); return NULL; }
    IWICFormatConverter* conv=NULL; hr = factory->lpVtbl->CreateFormatConverter(factory,&conv); if(FAILED(hr)||!conv){ frame->lpVtbl->Release(frame); decoder->lpVtbl->Release(decoder); factory->lpVtbl->Release(factory); return NULL; }
    hr = conv->lpVtbl->Initialize(conv,(IWICBitmapSource*)frame,&GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,NULL,0.0,WICBitmapPaletteTypeCustom); if(FAILED(hr)){ conv->lpVtbl->Release(conv); frame->lpVtbl->Release(frame); decoder->lpVtbl->Release(decoder); factory->lpVtbl->Release(factory); return NULL; }
    UINT pw=0,ph=0; conv->lpVtbl->GetSize(conv,&pw,&ph); if(pw==0||ph==0){ conv->lpVtbl->Release(conv); frame->lpVtbl->Release(frame); decoder->lpVtbl->Release(decoder); factory->lpVtbl->Release(factory); return NULL; }
    size_t stride = pw*4ULL; size_t total = stride*ph; unsigned char* pixels=(unsigned char*)malloc(total); if(!pixels){ conv->lpVtbl->Release(conv); frame->lpVtbl->Release(frame); decoder->lpVtbl->Release(decoder); factory->lpVtbl->Release(factory); return NULL; }
    hr = conv->lpVtbl->CopyPixels(conv,NULL,(UINT)stride,(UINT)total,pixels); if(FAILED(hr)){ free(pixels); conv->lpVtbl->Release(conv); frame->lpVtbl->Release(frame); decoder->lpVtbl->Release(decoder); factory->lpVtbl->Release(factory); return NULL; }
    conv->lpVtbl->Release(conv); frame->lpVtbl->Release(frame); decoder->lpVtbl->Release(decoder); factory->lpVtbl->Release(factory);
    Uint32 rmask=0x000000FFu, gmask=0x0000FF00u, bmask=0x00FF0000u, amask=0xFF000000u;
    SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(pixels,(int)pw,(int)ph,32,(int)stride,rmask,gmask,bmask,amask);
    if(!surf){ free(pixels); return NULL; }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r,surf); if(!tex){ SDL_FreeSurface(surf); free(pixels); return NULL; }
    /* SDL will own no copy of pixels; free after texture upload */
    SDL_FreeSurface(surf); free(pixels); if(w)*w=(int)pw; if(h)*h=(int)ph; return tex;
}
#endif

static int load_texture_auto(SDL_Renderer* r, const char* path, SDL_Texture** out_tex, int* w, int* h){
    size_t len = strlen(path);
    if(len>=4 && (strcmp(path+len-4,".png")==0 || strcmp(path+len-4,".PNG")==0)){
#ifdef _WIN32
        SDL_Texture* t = load_png_wic(r,path,w,h); if(!t){ fprintf(stderr,"WARN: png load failed %s\n", path); return 0; } *out_tex=t; return 1;
#else
        fprintf(stderr,"ERR: PNG unsupported on this platform build for tool (%s)\n", path); return 0;
#endif
    } else {
        SDL_Surface* s = SDL_LoadBMP(path); if(!s){ fprintf(stderr,"WARN: bmp load fail %s (%s)\n", path, SDL_GetError()); return 0; }
        SDL_Texture* t = SDL_CreateTextureFromSurface(r,s); if(!t){ fprintf(stderr,"WARN: texture create fail %s (%s)\n", path, SDL_GetError()); SDL_FreeSurface(s); return 0; }
        *w=s->w; *h=s->h; *out_tex=t; SDL_FreeSurface(s); return 1;
    }
}

static void init_default_poses(void){
    for(int i=0;i<FRAME_COUNT;i++){ if(poses[i].scale==0) poses[i].scale=1.0f; if(poses[i].pivot_x==0 && poses[i].pivot_y==0){ poses[i].pivot_x=0.5f; poses[i].pivot_y=0.5f; } }
}

static void save_json(const char* path){
    if(!path) return; FILE* f=NULL; fopen_s(&f,path,"wb"); if(!f){ fprintf(stderr,"ERR: save fail %s\n", path); return; }
    fprintf(f,"{\n  \"weapon_id\": \"%s\",\n  \"direction\": \"%s\",\n  \"frame_size\": %d,\n  \"frames\": [\n", weapon_id, direction_label, frame_size);
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
    const char* player_sheet_path = NULL; out_path = NULL;
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--weapon")==0 && i+1<argc){ weapon_id=argv[++i]; }
        else if(strcmp(argv[i],"--player-sheet")==0 && i+1<argc){ player_sheet_path=argv[++i]; }
        else if(strcmp(argv[i],"--out")==0 && i+1<argc){ out_path=argv[++i]; }
        else if(strcmp(argv[i],"--weapon-image")==0 && i+1<argc){ weapon_image_path=argv[++i]; }
        else if(strcmp(argv[i],"--frame-size")==0 && i+1<argc){ frame_size=atoi(argv[++i]); if(frame_size<=0) frame_size=64; }
        else if(strcmp(argv[i],"--direction")==0 && i+1<argc){ direction_label=argv[++i]; }
        else if(strcmp(argv[i],"--help")==0){ usage(); return 0; }
    }
    if(!player_sheet_path){ fprintf(stderr,"ERR: --player-sheet required.\n"); usage(); return 1; }
    if(SDL_Init(SDL_INIT_VIDEO)!=0){ fprintf(stderr,"SDL init fail: %s\n", SDL_GetError()); return 1; }
    SDL_Window* win = SDL_CreateWindow("Weapon Pose Tool", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* ren = SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!ren){ fprintf(stderr,"Renderer fail %s\n", SDL_GetError()); return 1; }
    /* Load player sheet (PNG or BMP) */
    if(!load_texture_auto(ren, player_sheet_path, &player_sheet_tex, &player_sheet_w, &player_sheet_h)){
        fprintf(stderr,"ERR: failed to load player sheet %s\n", player_sheet_path); return 1; }
    if(player_sheet_w < frame_size * FRAME_COUNT){ fprintf(stderr,"WARN: sheet width (%d) < expected %d; frames may crop.\n", player_sheet_w, frame_size*FRAME_COUNT); }
    /* Load single weapon image */
    char inferred_path[256]; if(!weapon_image_path){ snprintf(inferred_path,sizeof inferred_path,"assets/weapons/weapon_%s.bmp", weapon_id); weapon_image_path=inferred_path; }
    if(!load_texture_auto(ren, weapon_image_path, &weapon_tex_single, &weapon_w, &weapon_h)){
        fprintf(stderr,"ERR: failed load weapon image %s\n", weapon_image_path); return 1; }
    init_default_poses();
    if(!out_path){ static char auto_out[256]; snprintf(auto_out,sizeof auto_out,"assets/weapons/weapon_%s_%s_pose.json", weapon_id, direction_label); out_path=auto_out; }
    int running=1; while(running){
        SDL_Event e; while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) running=0; else if(e.type==SDL_KEYDOWN){ if(e.key.keysym.sym==SDLK_ESCAPE) running=0; else handle_key(&e); }
        }
        int w,h; SDL_GetRendererOutputSize(ren,&w,&h);
        SDL_SetRenderDrawColor(ren,18,18,20,255); SDL_RenderClear(ren);
        /* Draw player reference centered */
        int origin_x = w/2; int origin_y = h/2;
        /* Slice current frame from sheet */
        SDL_Rect src = { frame_index * frame_size, 0, frame_size, frame_size };
        SDL_Rect pdst = { origin_x - frame_size/2, origin_y - frame_size/2, frame_size, frame_size };
        SDL_RenderCopy(ren, player_sheet_tex, &src, &pdst);
        /* Draw weapon overlay */
        FramePose* p=&poses[frame_index]; SDL_FRect dst; dst.w = weapon_w * p->scale; dst.h = weapon_h * p->scale; dst.x = origin_x + p->dx - dst.w * p->pivot_x; dst.y = origin_y + p->dy - dst.h * p->pivot_y; SDL_FPoint center = { dst.w * p->pivot_x, dst.h * p->pivot_y }; if(weapon_tex_single){ SDL_RenderCopyExF(ren, weapon_tex_single, NULL, &dst, p->angle, &center, SDL_FLIP_NONE); }
        /* Crosshair */
        SDL_SetRenderDrawColor(ren,255,255,0,255); SDL_RenderDrawLine(ren, origin_x-10, origin_y, origin_x+10, origin_y); SDL_RenderDrawLine(ren, origin_x, origin_y-10, origin_y, origin_y+10);
        /* HUD text minimal */
        char info[256]; snprintf(info,sizeof info,"%s dir=%s frame=%d dx=%.1f dy=%.1f ang=%.1f scale=%.2f piv=(%.2f,%.2f)%s", weapon_id, direction_label, frame_index, p->dx, p->dy, p->angle, p->scale, p->pivot_x, p->pivot_y, dirty_flag?" *":"");
        SDL_SetWindowTitle(win, info);
        SDL_RenderPresent(ren);
    }
    SDL_Quit();
    return 0;
}
