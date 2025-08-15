#include "ui/core/ui_context.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

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
    /* Default key repeat configuration */
    ctx->key_repeat_initial_ms = 400.0; /* typical desktop delay */
    ctx->key_repeat_interval_ms = 65.0; /* ~15 repeats/sec */
    ctx->chord_timeout_ms = 900.0; /* generous default */
    return 1;
}

void rogue_ui_shutdown(RogueUIContext* ctx){ if(!ctx) return; free(ctx->nodes); ctx->nodes=NULL; ctx->node_capacity=0; ctx->node_count=0; free(ctx->arena); ctx->arena=NULL; ctx->arena_size=ctx->arena_offset=0; }

void rogue_ui_begin(RogueUIContext* ctx, double delta_time_ms){ if(!ctx) return; ctx->frame_dt_ms=delta_time_ms; ctx->time_ms += delta_time_ms; ctx->node_count=0; ctx->stats.draw_calls=0; ctx->frame_active=1; ctx->arena_offset=0; ctx->hot_index=-1; }
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

void rogue_ui_set_input(RogueUIContext* ctx, const RogueUIInputState* in){
    if(!ctx||!in) return; ctx->input=*in; ctx->hot_index=-1;
    /* Record input for replay if enabled */
    if(ctx->replay_recording && ctx->replay_count < (int)(sizeof(ctx->replay_buffer)/sizeof(ctx->replay_buffer[0]))){ ctx->replay_buffer[ctx->replay_count++] = *in; }
    /* Chord prime evaluation */
    if(ctx->input.key_ctrl && ctx->input.key_char){ char c=ctx->input.key_char; if(ctx->pending_chord){ char first=ctx->pending_chord; for(int i=0;i<ctx->chord_count;i++){ if(ctx->chord_commands[i].k1==first && ctx->chord_commands[i].k2==c){ ctx->last_command_executed=ctx->chord_commands[i].command_id; break; } } ctx->pending_chord=0; } else { for(int i=0;i<ctx->chord_count;i++){ if(ctx->chord_commands[i].k1==c){ ctx->pending_chord=c; ctx->pending_chord_time_ms=ctx->time_ms; break; } } } }
}
int rogue_ui_focused_index(const RogueUIContext* ctx){ return ctx? ctx->focus_index:-1; }

static int interactive_push(RogueUIContext* ctx, RogueUINode* node){
    int idx = push_node(ctx,*node); if(idx<0) return idx;
    float mx=ctx->input.mouse_x, my=ctx->input.mouse_y;
    if(rect_contains(&node->rect,mx,my)) ctx->hot_index=idx;
    return idx;
}

int rogue_ui_button(RogueUIContext* ctx, RogueUIRect r, const char* label, uint32_t bg_color, uint32_t text_color){
    if(!ctx||!ctx->frame_active) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.text=label; n.color=bg_color; n.aux_color=text_color; n.kind=5; assign_id(&n); int idx=interactive_push(ctx,&n); if(idx<0) return -1; int clicked=0;
    if(ctx->modal_index>=0 && ctx->modal_index!=idx) return idx; /* modal gating */
    if(ctx->hot_index==idx){ if(ctx->input.mouse_pressed){ ctx->active_index=idx; } if(ctx->input.mouse_released && ctx->active_index==idx){ clicked=1; ctx->active_index=-1; } }
    if(idx>=0 && clicked) ctx->nodes[idx].value=1.0f; return idx;
}

int rogue_ui_toggle(RogueUIContext* ctx, RogueUIRect r, const char* label, int* state, uint32_t off_color, uint32_t on_color, uint32_t text_color){
    if(!ctx||!ctx->frame_active||!state) return -1; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.text=label; n.color=*state? on_color:off_color; n.aux_color=text_color; n.kind=6; assign_id(&n); int idx=interactive_push(ctx,&n); if(idx<0) return -1; if(ctx->modal_index>=0 && ctx->modal_index!=idx) return idx; if(ctx->hot_index==idx){ if(ctx->input.mouse_pressed){ ctx->active_index=idx; } if(ctx->input.mouse_released && ctx->active_index==idx){ *state = !*state; ctx->nodes[idx].color=*state? on_color:off_color; ctx->active_index=-1; } }
    ctx->nodes[idx].value=(float)(*state); return idx;
}

int rogue_ui_slider(RogueUIContext* ctx, RogueUIRect r, float min_v, float max_v, float* value, uint32_t track_color, uint32_t fill_color){
    if(!ctx||!ctx->frame_active||!value) return -1; if(max_v==min_v){ max_v=min_v+1.0f; }
    if(*value<min_v) *value=min_v; if(*value>max_v) *value=max_v;
    RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.color=track_color; n.aux_color=fill_color; n.kind=7; n.value=*value; n.value_max=max_v; assign_id(&n); int idx=interactive_push(ctx,&n); if(idx<0) return -1;
    if(ctx->modal_index>=0 && ctx->modal_index!=idx) return idx;
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
    if(ctx->modal_index>=0 && ctx->modal_index!=idx) return idx;
    if(ctx->focus_index==idx){
        if(ctx->input.key_paste){ const char* clip = rogue_ui_clipboard_get(); if(clip){ int len=(int)strlen(buffer); for(int i=0; clip[i] && len<buffer_cap-1; ++i){ buffer[len++]=clip[i]; } buffer[len]='\0'; } }
        if(ctx->input.text_char){ int len=(int)strlen(buffer); if(len<buffer_cap-1){ buffer[len]=(char)ctx->input.text_char; buffer[len+1]='\0'; } }
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

/* Scroll Container Implementation (Phase 2.4) */
int rogue_ui_scroll_begin(RogueUIContext* ctx, RogueUIRect r, float content_height){ if(!ctx||!ctx->frame_active) return -1; if(content_height<r.h) content_height=r.h; RogueUINode n; memset(&n,0,sizeof n); n.rect=r; n.kind=0; n.text="__scroll"; n.value=0.0f; /* scroll offset */ n.value_max=content_height; assign_id(&n); int idx=push_node(ctx,n); if(idx<0) return -1; /* Apply wheel */ if(ctx->input.wheel_delta!=0){ float delta = -ctx->input.wheel_delta * 24.0f; float off = ctx->nodes[idx].value + delta; float max_off = content_height - r.h; if(max_off<0) max_off=0; if(off<0) off=0; if(off>max_off) off=max_off; ctx->nodes[idx].value=off; } return idx; }
void rogue_ui_scroll_set_content(int scroll_index, RogueUIContext* ctx, float content_height){ if(!ctx||scroll_index<0||scroll_index>=ctx->node_count) return; if(content_height<ctx->nodes[scroll_index].rect.h) content_height=ctx->nodes[scroll_index].rect.h; ctx->nodes[scroll_index].value_max=content_height; float max_off = content_height - ctx->nodes[scroll_index].rect.h; if(max_off<0) max_off=0; if(ctx->nodes[scroll_index].value>max_off) ctx->nodes[scroll_index].value=max_off; }
float rogue_ui_scroll_offset(const RogueUIContext* ctx, int scroll_index){ if(!ctx||scroll_index<0||scroll_index>=ctx->node_count) return 0.0f; return ctx->nodes[scroll_index].value; }
RogueUIRect rogue_ui_scroll_apply(const RogueUIContext* ctx, int scroll_index, RogueUIRect child_raw){ RogueUIRect r=child_raw; if(!ctx||scroll_index<0||scroll_index>=ctx->node_count) return r; r.y -= ctx->nodes[scroll_index].value; return r; }

/* Tooltip (Phase 2.5) */
int rogue_ui_tooltip(RogueUIContext* ctx, int target_index, const char* text, uint32_t bg_color, uint32_t text_color, int delay_ms){ if(!ctx||!ctx->frame_active||!text||target_index<0||target_index>=ctx->node_count) return -1; if(ctx->hot_index==target_index){ if(ctx->last_hover_index!=target_index){ ctx->last_hover_index=target_index; ctx->last_hover_start_ms=ctx->time_ms; } if((ctx->time_ms - ctx->last_hover_start_ms) >= (double)delay_ms){ RogueUIRect tr = ctx->nodes[target_index].rect; RogueUIRect tip={tr.x+tr.w+6.0f, tr.y, 160.0f, 24.0f}; int panel = rogue_ui_panel(ctx,tip,bg_color); if(panel>=0){ RogueUIRect text_r=tip; rogue_ui_text(ctx,text_r,text,text_color); } return panel; } } else { if(ctx->last_hover_index==target_index){ ctx->last_hover_index=-1; } } return -1; }

/* Navigation (Phase 2.8) advanced directional heuristics */
static int nav_key_down(const RogueUIInputState* in, int key_index){
    switch(key_index){
        case 0: return in->key_left; case 1: return in->key_right; case 2: return in->key_up; case 3: return in->key_down; case 4: return in->key_tab; case 5: return in->key_activate; default: return 0;
    }
}

static void rogue_apply_repeat(RogueUIContext* c, int idx, int* move_h, int* move_v, int* activate_ptr){
    if(c->key_repeat_state[idx]){
        double acc = c->key_repeat_accum[idx];
        if(acc >= c->key_repeat_initial_ms){
            double over = acc - c->key_repeat_initial_ms;
            int pulses = (int)(over / c->key_repeat_interval_ms);
            if(pulses>0){
                c->key_repeat_accum[idx] = c->key_repeat_initial_ms + over - pulses*c->key_repeat_interval_ms;
                if(idx==0) *move_h=-1; else if(idx==1) *move_h=1; else if(idx==2) *move_v=-1; else if(idx==3) *move_v=1; else if(idx==4) *move_h=1; else if(idx==5) *activate_ptr=1;
            }
        }
    }
}

void rogue_ui_navigation_update(RogueUIContext* ctx){
    if(!ctx||!ctx->frame_active) return;
    /* Phase 3.1 input replay injection */
    if(ctx->replay_playing){ if(!rogue_ui_replay_step(ctx)) {/* finished */} }
    /* Phase 3.4 key repeat update (simplified across arrow + tab + activate) */
    for(int i=0;i<6;i++){
        if(nav_key_down(&ctx->input,i)){
            if(ctx->key_repeat_state[i]==0){ ctx->key_repeat_state[i]=1; ctx->key_repeat_accum[i]=0.0; }
        } else {
            ctx->key_repeat_state[i]=0;
        }
    }
    int focusable_count=0; for(int i=0;i<ctx->node_count;i++){ int k=ctx->nodes[i].kind; if(k>=5&&k<=8) focusable_count++; }
    if(!focusable_count) return;
    /* Controller axis mapping (3.2) */
    int axis_move_h=0, axis_move_v=0;
    float threshold = 0.55f;
    if(ctx->controller.axis_x > threshold) axis_move_h=1; else if(ctx->controller.axis_x < -threshold) axis_move_h=-1;
    if(ctx->controller.axis_y > threshold) axis_move_v=1; else if(ctx->controller.axis_y < -threshold) axis_move_v=-1;
    /* Axis repeat gating using key_repeat slots 6,7 */
    if(axis_move_h){ if(ctx->key_repeat_state[6]==0){ ctx->key_repeat_state[6]=1; ctx->key_repeat_accum[6]=0.0; } } else ctx->key_repeat_state[6]=0;
    if(axis_move_v){ if(ctx->key_repeat_state[7]==0){ ctx->key_repeat_state[7]=1; ctx->key_repeat_accum[7]=0.0; } } else ctx->key_repeat_state[7]=0;
    int move_h=0, move_v=0, activate=0;
    /* Base key edge triggers */
    if(ctx->input.key_left) move_h=-1; else if(ctx->input.key_right) move_h=1;
    if(ctx->input.key_up) move_v=-1; else if(ctx->input.key_down) move_v=1;
    if(ctx->input.key_tab) move_h=1;
    if(ctx->input.key_activate) activate=1;
    if(!move_h && axis_move_h) move_h=axis_move_h;
    if(!move_v && axis_move_v) move_v=axis_move_v;
    /* Key repeat pulses */
    for(int i=0;i<6;i++) if(ctx->key_repeat_state[i]) ctx->key_repeat_accum[i]+=ctx->frame_dt_ms;
    for(int i=6;i<8;i++) if(ctx->key_repeat_state[i]) ctx->key_repeat_accum[i]+=ctx->frame_dt_ms;
    /* helper inline for repeat application */
    rogue_apply_repeat(ctx,0,&move_h,&move_v,&activate);
    rogue_apply_repeat(ctx,1,&move_h,&move_v,&activate);
    rogue_apply_repeat(ctx,2,&move_h,&move_v,&activate);
    rogue_apply_repeat(ctx,3,&move_h,&move_v,&activate);
    rogue_apply_repeat(ctx,4,&move_h,&move_v,&activate);
    rogue_apply_repeat(ctx,5,&move_h,&move_v,&activate);
    if(axis_move_h) rogue_apply_repeat(ctx, axis_move_h>0?1:0,&move_h,&move_v,&activate);
    if(axis_move_v) rogue_apply_repeat(ctx, axis_move_v>0?3:2,&move_h,&move_v,&activate);
    if(ctx->focus_index<0||ctx->focus_index>=ctx->node_count){ /* choose first focusable */ for(int i=0;i<ctx->node_count;i++){ int k=ctx->nodes[i].kind; if(k>=5&&k<=8){ ctx->focus_index=i; break; } } }
    /* Modal focus enforcement */
    if(ctx->modal_index>=0) ctx->focus_index = ctx->modal_index;
    if(ctx->focus_index<0) return;
    if(activate){ RogueUINode* cur=&ctx->nodes[ctx->focus_index]; if(cur->kind==5){ cur->value = 1.0f; } else if(cur->kind==6){ cur->value = (cur->value==0.0f)?1.0f:0.0f; } }
    if(!move_h && !move_v) return;
    RogueUINode* cur=&ctx->nodes[ctx->focus_index]; float cx=cur->rect.x+cur->rect.w*0.5f; float cy=cur->rect.y+cur->rect.h*0.5f;
    int best=-1; float best_score=1e9f;
    for(int i=0;i<ctx->node_count;i++){ if(i==ctx->focus_index) continue; int k=ctx->nodes[i].kind; if(k<5||k>8) continue; if(ctx->modal_index>=0 && i!=ctx->modal_index) continue; RogueUINode* n=&ctx->nodes[i]; float nx=n->rect.x+n->rect.w*0.5f; float ny=n->rect.y+n->rect.h*0.5f; float dx=nx-cx; float dy=ny-cy; if(move_h){ if(move_h<0 && dx>=-1e-3f) continue; if(move_h>0 && dx<=1e-3f) continue; } if(move_v){ if(move_v<0 && dy>=-1e-3f) continue; if(move_v>0 && dy<=1e-3f) continue; } float primary = move_h? fabsf(dx):fabsf(dy); float secondary = move_h? fabsf(dy):fabsf(dx); if(secondary > primary*2.5f) continue; float dist = (float)sqrt(dx*dx+dy*dy); float score = dist + secondary*0.25f + primary*0.1f; if(score<best_score){ best_score=score; best=i; } }
    if(best>=0){ ctx->focus_index=best; return; }
    /* fallback linear wrap */
    int dir = (move_h>0||move_v>0)?1:-1; int start=ctx->focus_index; int curi=start; for(;;){ curi+=dir; if(curi>=ctx->node_count) curi=0; if(curi<0) curi=ctx->node_count-1; if(curi==start) break; int k=ctx->nodes[curi].kind; if(k>=5&&k<=8){ ctx->focus_index=curi; break; } }
    /* Chord timeout maintenance */
    if(ctx->pending_chord && (ctx->time_ms - ctx->pending_chord_time_ms) > ctx->chord_timeout_ms){ ctx->pending_chord=0; }
}

/* Phase 3 scaffolding implementations (stubs / storage already added in context) */
void rogue_ui_set_modal(RogueUIContext* ctx, int modal_index){ if(!ctx) return; ctx->modal_index=modal_index; }
void rogue_ui_set_controller(RogueUIContext* ctx, const RogueUIControllerState* st){ if(!ctx||!st) return; ctx->controller=*st; }
static char g_clipboard[256];
void rogue_ui_clipboard_set(const char* text){ if(!text) text=""; size_t i=0; for(; i<sizeof(g_clipboard)-1 && text[i]; ++i) g_clipboard[i]=text[i]; g_clipboard[i]='\0'; }
const char* rogue_ui_clipboard_get(void){ return g_clipboard; }
void rogue_ui_ime_start(void){}
void rogue_ui_ime_cancel(void){}
void rogue_ui_ime_commit(RogueUIContext* ctx, const char* text){ (void)ctx; (void)text; }
void rogue_ui_key_repeat_config(RogueUIContext* ctx, double initial_delay_ms, double interval_ms){ if(!ctx) return; ctx->key_repeat_initial_ms=initial_delay_ms; ctx->key_repeat_interval_ms=interval_ms; }
int rogue_ui_register_chord(RogueUIContext* ctx, char k1, char k2, int command_id){ if(!ctx) return 0; if(ctx->chord_count>=8) return 0; ctx->chord_commands[ctx->chord_count].k1=k1; ctx->chord_commands[ctx->chord_count].k2=k2; ctx->chord_commands[ctx->chord_count].command_id=command_id; ctx->chord_count++; return 1; }
int rogue_ui_last_command(const RogueUIContext* ctx){ return ctx? ctx->last_command_executed:0; }
void rogue_ui_replay_start_record(RogueUIContext* ctx){ if(!ctx) return; ctx->replay_recording=1; ctx->replay_playing=0; ctx->replay_count=0; }
void rogue_ui_replay_stop_record(RogueUIContext* ctx){ if(!ctx) return; ctx->replay_recording=0; }
void rogue_ui_replay_start_playback(RogueUIContext* ctx){ if(!ctx) return; ctx->replay_playing=1; ctx->replay_recording=0; ctx->replay_cursor=0; }
int rogue_ui_replay_step(RogueUIContext* ctx){ if(!ctx) return 0; if(!ctx->replay_playing) return 0; if(ctx->replay_cursor>=ctx->replay_count){ ctx->replay_playing=0; return 0; } ctx->input = ctx->replay_buffer[ctx->replay_cursor++]; return 1; }

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
