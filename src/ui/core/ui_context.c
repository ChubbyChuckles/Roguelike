#include "ui/core/ui_context.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

static unsigned int xorshift32(unsigned int* s){ unsigned int x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }

int rogue_ui_init(RogueUIContext* ctx, const RogueUIContextConfig* cfg){
    if(!ctx||!cfg) return 0;
    memset(ctx,0,sizeof *ctx);
    int cap = cfg->max_nodes>0? cfg->max_nodes:128;
    ctx->nodes = (RogueUINode*)calloc((size_t)cap,sizeof(RogueUINode));
    if(!ctx->nodes) return 0;
    ctx->node_capacity = cap;
    ctx->rng_state = cfg->seed? cfg->seed: 0xC0FFEEu;
    ctx->theme.panel_bg_color = 0x202028FFu;
    ctx->theme.text_color = 0xFFFFFFFFu;
    size_t arena_size = cfg->arena_size? cfg->arena_size: (size_t) (32*1024);
    ctx->arena = (unsigned char*)malloc(arena_size);
    if(!ctx->arena){ free(ctx->nodes); ctx->nodes=NULL; return 0; }
    ctx->arena_size = arena_size;
    return 1;
}

void rogue_ui_shutdown(RogueUIContext* ctx){ if(!ctx) return; free(ctx->nodes); ctx->nodes=NULL; ctx->node_capacity=0; ctx->node_count=0; free(ctx->arena); ctx->arena=NULL; ctx->arena_size=ctx->arena_offset=0; }

void rogue_ui_begin(RogueUIContext* ctx, double delta_time_ms){ (void)delta_time_ms; if(!ctx) return; ctx->node_count=0; ctx->stats.draw_calls=0; ctx->frame_active=1; ctx->arena_offset=0; }
void rogue_ui_end(RogueUIContext* ctx){ if(!ctx) return; ctx->frame_active=0; }

static int push_node(RogueUIContext* ctx, RogueUINode n){ if(ctx->node_count>=ctx->node_capacity) return -1; ctx->nodes[ctx->node_count]=n; ctx->node_count++; ctx->stats.node_count=ctx->node_count; return ctx->node_count-1; }

int rogue_ui_panel(RogueUIContext* ctx, RogueUIRect r, uint32_t color){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.color=color; n.kind=0; return push_node(ctx,n); }
int rogue_ui_text(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.text=text; n.color=color; n.kind=1; return push_node(ctx,n); }
int rogue_ui_image(RogueUIContext* ctx, RogueUIRect r, const char* path, uint32_t tint){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.text=path; n.color=tint; n.kind=2; return push_node(ctx,n); }
int rogue_ui_sprite(RogueUIContext* ctx, RogueUIRect r, int sheet_id, int frame, uint32_t tint){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.color=tint; n.data_i0=sheet_id; n.data_i1=frame; n.kind=3; return push_node(ctx,n); }
int rogue_ui_progress_bar(RogueUIContext* ctx, RogueUIRect r, float value, float max_value, uint32_t bg_color, uint32_t fill_color, int orientation){ if(!ctx||!ctx->frame_active) return -1; if(max_value<=0) max_value=1.0f; if(value<0) value=0; if(value>max_value) value=max_value; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.color=bg_color; n.aux_color=fill_color; n.value=value; n.value_max=max_value; n.data_i0=orientation; n.kind=4; return push_node(ctx,n); }

const RogueUINode* rogue_ui_nodes(const RogueUIContext* ctx, int* count_out){ if(count_out) *count_out = ctx? ctx->node_count:0; return ctx? ctx->nodes:NULL; }

unsigned int rogue_ui_rng_next(RogueUIContext* ctx){ if(!ctx) return 0; return xorshift32(&ctx->rng_state); }
void rogue_ui_set_theme(RogueUIContext* ctx, const RogueUITheme* theme){ if(!ctx||!theme) return; ctx->theme = *theme; }

void rogue_ui_set_simulation_snapshot(RogueUIContext* ctx, const void* snapshot, size_t size){ if(!ctx) return; ctx->sim_snapshot=snapshot; ctx->sim_snapshot_size=size; }
const void* rogue_ui_simulation_snapshot(const RogueUIContext* ctx, size_t* size_out){ if(size_out) *size_out = ctx? ctx->sim_snapshot_size:0; return ctx? ctx->sim_snapshot:NULL; }

static size_t align_up(size_t v, size_t a){ return (v + (a-1)) & ~(a-1); }
void* rogue_ui_arena_alloc(RogueUIContext* ctx, size_t size, size_t align){ if(!ctx||size==0) return NULL; if(align==0) align=8; size_t off = align_up(ctx->arena_offset, align); if(off+size>ctx->arena_size) return NULL; void* ptr = ctx->arena+off; ctx->arena_offset = off+size; return ptr; }

int rogue_ui_text_dup(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color){ if(!ctx||!text) return -1; size_t len = strlen(text)+1; char* copy = (char*)rogue_ui_arena_alloc(ctx,len,1); if(!copy) return -1; memcpy(copy,text,len); return rogue_ui_text(ctx,r,copy,color); }

/* FNV-1a 64-bit */
static uint64_t fnv1a64(const void* data, size_t len){ const unsigned char* p=(const unsigned char*)data; uint64_t h=0xcbf29ce484222325ULL; for(size_t i=0;i<len;i++){ h^=p[i]; h*=0x100000001b3ULL; } return h; }

size_t rogue_ui_serialize(const RogueUIContext* ctx, char* buffer, size_t buffer_size){ if(!ctx||!buffer||buffer_size==0) return 0; size_t written=0; for(int i=0;i<ctx->node_count;i++){ const RogueUINode* n=&ctx->nodes[i]; int w = snprintf(buffer+written, buffer_size-written, "%d %.2f %.2f %.2f %.2f %08X %s\n", n->kind, n->rect.x,n->rect.y,n->rect.w,n->rect.h, n->color, n->text? n->text:""); if(w<0) break; if((size_t)w >= buffer_size-written){ written = buffer_size-1; break; } written += (size_t)w; } if(written<buffer_size) buffer[written]='\0'; else buffer[buffer_size-1]='\0'; return written; }

int rogue_ui_diff_changed(RogueUIContext* ctx){ if(!ctx) return 0; char tmp[1024]; size_t len = rogue_ui_serialize(ctx,tmp,sizeof tmp); uint64_t h = fnv1a64(tmp,len); if(h!=ctx->last_serial_hash){ ctx->last_serial_hash=h; return 1; } return 0; }
