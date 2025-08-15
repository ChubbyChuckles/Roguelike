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

static int push_node(RogueUIContext* ctx, RogueUINode n){ if(ctx->node_count>=ctx->node_capacity) return -1; if(n.parent_index < -1) n.parent_index=-1; ctx->nodes[ctx->node_count]=n; ctx->node_count++; ctx->stats.node_count=ctx->node_count; return ctx->node_count-1; }

/* Simple FNV1a 32-bit for IDs */
static uint32_t fnv1a32(const char* s){ uint32_t h=2166136261u; while(s && *s){ h^=(unsigned char)(*s++); h*=16777619u; } return h; }
uint32_t rogue_ui_make_id(const char* label){ return fnv1a32(label?label:"\0"); }
const RogueUINode* rogue_ui_find_by_id(const RogueUIContext* ctx, uint32_t id_hash){ if(!ctx) return NULL; for(int i=0;i<ctx->node_count;i++) if(ctx->nodes[i].id_hash==id_hash) return &ctx->nodes[i]; return NULL; }

static void assign_id(RogueUINode* n){ if(n->text) n->id_hash = rogue_ui_make_id(n->text); }
int rogue_ui_panel(RogueUIContext* ctx, RogueUIRect r, uint32_t color){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.color=color; n.kind=0; assign_id(&n); return push_node(ctx,n); }
int rogue_ui_text(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.text=text; n.color=color; n.kind=1; assign_id(&n); return push_node(ctx,n); }
int rogue_ui_image(RogueUIContext* ctx, RogueUIRect r, const char* path, uint32_t tint){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.text=path; n.color=tint; n.kind=2; assign_id(&n); return push_node(ctx,n); }
int rogue_ui_sprite(RogueUIContext* ctx, RogueUIRect r, int sheet_id, int frame, uint32_t tint){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.color=tint; n.data_i0=sheet_id; n.data_i1=frame; n.kind=3; return push_node(ctx,n); }
int rogue_ui_progress_bar(RogueUIContext* ctx, RogueUIRect r, float value, float max_value, uint32_t bg_color, uint32_t fill_color, int orientation){ if(!ctx||!ctx->frame_active) return -1; if(max_value<=0) max_value=1.0f; if(value<0) value=0; if(value>max_value) value=max_value; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.color=bg_color; n.aux_color=fill_color; n.value=value; n.value_max=max_value; n.data_i0=orientation; n.kind=4; return push_node(ctx,n); }

/* ---- Interaction helpers ---- */
static int rect_contains(const RogueUIRect* r, float x, float y){ return x>=r->x && y>=r->y && x<=r->x+r->w && y<=r->y+r->h; }

void rogue_ui_set_input(RogueUIContext* ctx, const RogueUIInputState* in){ if(!ctx||!in) return; ctx->input=*in; ctx->hot_index=-1; }
int rogue_ui_focused_index(const RogueUIContext* ctx){ return ctx? ctx->focus_index:-1; }

static int interactive_push(RogueUIContext* ctx, RogueUINode* node){
    int idx = push_node(ctx,*node); if(idx<0) return idx;
    float mx=ctx->input.mouse_x, my=ctx->input.mouse_y;
    if(rect_contains(&node->rect,mx,my)) ctx->hot_index=idx;
    return idx;
}

int rogue_ui_button(RogueUIContext* ctx, RogueUIRect r, const char* label, uint32_t bg_color, uint32_t text_color){
    if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.text=label; n.color=bg_color; n.aux_color=text_color; n.kind=5; assign_id(&n); int idx=interactive_push(ctx,&n); if(idx<0) return -1; int clicked=0; if(ctx->hot_index==idx){ if(ctx->input.mouse_pressed){ ctx->active_index=idx; } if(ctx->input.mouse_released && ctx->active_index==idx){ clicked=1; ctx->active_index=-1; } }
    if(idx>=0 && clicked) ctx->nodes[idx].value=1.0f; return idx;
}

int rogue_ui_toggle(RogueUIContext* ctx, RogueUIRect r, const char* label, int* state, uint32_t off_color, uint32_t on_color, uint32_t text_color){
    if(!ctx||!ctx->frame_active||!state) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.text=label; n.color=*state? on_color:off_color; n.aux_color=text_color; n.kind=6; assign_id(&n); int idx=interactive_push(ctx,&n); if(idx<0) return -1; if(ctx->hot_index==idx){ if(ctx->input.mouse_pressed){ ctx->active_index=idx; } if(ctx->input.mouse_released && ctx->active_index==idx){ *state = !*state; ctx->nodes[idx].color=*state? on_color:off_color; ctx->active_index=-1; } }
    ctx->nodes[idx].value=(float)(*state); return idx;
}

int rogue_ui_slider(RogueUIContext* ctx, RogueUIRect r, float min_v, float max_v, float* value, uint32_t track_color, uint32_t fill_color){
    if(!ctx||!ctx->frame_active||!value) return -1; if(max_v==min_v){ max_v=min_v+1.0f; }
    if(*value<min_v) *value=min_v; if(*value>max_v) *value=max_v;
    RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.color=track_color; n.aux_color=fill_color; n.kind=7; n.value=*value; n.value_max=max_v; assign_id(&n); int idx=interactive_push(ctx,&n); if(idx<0) return -1;
    if(ctx->hot_index==idx){ if(ctx->input.mouse_pressed){ ctx->active_index=idx; }
        if(ctx->active_index==idx && ctx->input.mouse_down){
            float t = (ctx->input.mouse_x - r.x)/r.w; if(t<0) t=0; if(t>1) t=1; *value = min_v + t*(max_v-min_v); ctx->nodes[idx].value=*value; }
        if(ctx->input.mouse_released && ctx->active_index==idx){ ctx->active_index=-1; }
    }
    return idx;
}

int rogue_ui_text_input(RogueUIContext* ctx, RogueUIRect r, char* buffer, int buffer_cap, uint32_t bg_color, uint32_t text_color){
    if(!ctx||!ctx->frame_active||!buffer||buffer_cap<=0) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.text=buffer; n.color=bg_color; n.aux_color=text_color; n.kind=8; assign_id(&n); int idx=interactive_push(ctx,&n); if(idx<0) return -1;
    int hovered = (ctx->hot_index==idx);
    if(hovered && ctx->input.mouse_pressed){ ctx->focus_index=idx; }
    if(ctx->focus_index==idx){
        if(ctx->input.text_char){
            int len=(int)strlen(buffer); if(len<buffer_cap-1){ buffer[len]=(char)ctx->input.text_char; buffer[len+1]='\0'; }
        }
        if(ctx->input.backspace){ int len=(int)strlen(buffer); if(len>0){ buffer[len-1]='\0'; } }
        if(ctx->input.key_tab){ ctx->focus_index = (idx+1<ctx->node_count)? idx+1:0; }
    }
    return idx;
}

/* Layout containers */
int rogue_ui_row_begin(RogueUIContext* ctx, RogueUIRect r, int padding, int spacing){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.kind=0; n.data_i0=padding; n.data_i1=spacing; n.text="__row"; assign_id(&n); int idx=push_node(ctx,n); return idx; }
int rogue_ui_column_begin(RogueUIContext* ctx, RogueUIRect r, int padding, int spacing){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.kind=0; n.data_i0=padding; n.data_i1=spacing; n.text="__col"; assign_id(&n); int idx=push_node(ctx,n); return idx; }
int rogue_ui_row_next(RogueUIContext* ctx, int row_index, float width, float height, RogueUIRect* out_rect){ if(!ctx||row_index<0||row_index>=ctx->node_count) return 0; RogueUINode* row=&ctx->nodes[row_index]; float cursor = row->value; float padding=(float)row->data_i0; float spacing=(float)row->data_i1; if(cursor==0) cursor=padding; RogueUIRect rr=row->rect; RogueUIRect child={rr.x+cursor, rr.y+padding, width, height}; cursor += width + spacing; row->value=cursor; if(out_rect) *out_rect=child; return 1; }
int rogue_ui_column_next(RogueUIContext* ctx, int col_index, float width, float height, RogueUIRect* out_rect){ if(!ctx||col_index<0||col_index>=ctx->node_count) return 0; RogueUINode* col=&ctx->nodes[col_index]; float cursor=col->value; float padding=(float)col->data_i0; float spacing=(float)col->data_i1; if(cursor==0) cursor=padding; RogueUIRect cr=col->rect; RogueUIRect child={cr.x+padding, cr.y+cursor, width, height}; cursor += height + spacing; col->value=cursor; if(out_rect) *out_rect=child; return 1; }
RogueUIRect rogue_ui_grid_cell(RogueUIRect grid_rect, int rows, int cols, int r, int c, int padding, int spacing){ RogueUIRect cell={0,0,0,0}; if(rows<=0||cols<=0) return cell; float fpad=(float)padding; float fsp=(float)spacing; float total_spacing_x = fsp*(cols-1) + fpad*2.0f; float total_spacing_y = fsp*(rows-1) + fpad*2.0f; float cw = (grid_rect.w - total_spacing_x)/(float)cols; float ch=(grid_rect.h - total_spacing_y)/(float)rows; cell.x = grid_rect.x + fpad + (float)c*(cw+fsp); cell.y = grid_rect.y + fpad + (float)r*(ch+fsp); cell.w=cw; cell.h=ch; return cell; }
int rogue_ui_layer(RogueUIContext* ctx, RogueUIRect r, int layer_order){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.kind=0; n.data_i0=layer_order; n.text="__layer"; assign_id(&n); return push_node(ctx,n); }

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
