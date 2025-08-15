#include "ui/core/ui_context.h"
#include <stdlib.h>
#include <string.h>

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
    return 1;
}

void rogue_ui_shutdown(RogueUIContext* ctx){ if(!ctx) return; free(ctx->nodes); ctx->nodes=NULL; ctx->node_capacity=0; ctx->node_count=0; }

void rogue_ui_begin(RogueUIContext* ctx, double delta_time_ms){ (void)delta_time_ms; if(!ctx) return; ctx->node_count=0; ctx->stats.draw_calls=0; ctx->frame_active=1; }
void rogue_ui_end(RogueUIContext* ctx){ if(!ctx) return; ctx->frame_active=0; }

static int push_node(RogueUIContext* ctx, RogueUINode n){ if(ctx->node_count>=ctx->node_capacity) return -1; ctx->nodes[ctx->node_count]=n; ctx->node_count++; ctx->stats.node_count=ctx->node_count; return ctx->node_count-1; }

int rogue_ui_panel(RogueUIContext* ctx, RogueUIRect r, uint32_t color){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n={r,NULL,color,0}; return push_node(ctx,n); }
int rogue_ui_text(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color){ if(!ctx||!ctx->frame_active) return -1; RogueUINode n={r,text,color,1}; return push_node(ctx,n); }

const RogueUINode* rogue_ui_nodes(const RogueUIContext* ctx, int* count_out){ if(count_out) *count_out = ctx? ctx->node_count:0; return ctx? ctx->nodes:NULL; }

unsigned int rogue_ui_rng_next(RogueUIContext* ctx){ if(!ctx) return 0; return xorshift32(&ctx->rng_state); }
void rogue_ui_set_theme(RogueUIContext* ctx, const RogueUITheme* theme){ if(!ctx||!theme) return; ctx->theme = *theme; }
