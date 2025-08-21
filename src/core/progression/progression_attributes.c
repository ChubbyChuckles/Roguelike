#include "core/progression/progression_attributes.h"
#include "core/app_state.h"
#include "entities/player.h"
#include <string.h>

/* Simple rolling hash fold */
static unsigned long long fold(unsigned long long h, unsigned long long v){ h ^= v + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2); return h; }

/* Global attribute state (exposed for progression persistence) */
RogueAttributeState g_attr_state;

void rogue_attr_state_init(RogueAttributeState* st, int str,int dex,int vit,int intl){ if(!st) return; memset(st,0,sizeof *st); st->strength=str; st->dexterity=dex; st->vitality=vit; st->intelligence=intl; st->journal_hash=0xcbf29ce484222325ULL; }

int rogue_attr_unspent_points(void){ return g_app.unspent_stat_points; }

void rogue_attr_grant_points(int points){ if(points>0) g_app.unspent_stat_points += points; }

static int* attr_ptr(RogueAttributeState* st, char code){ if(!st) return 0; switch(code){ case 'S': return &st->strength; case 'D': return &st->dexterity; case 'V': return &st->vitality; case 'I': return &st->intelligence; default: return 0; } }

int rogue_attr_spend(RogueAttributeState* st, char code){ if(!st) return -1; if(g_app.unspent_stat_points <=0) return -2; int* p = attr_ptr(st, code); if(!p) return -3; (*p)++; st->spent_points++; g_app.unspent_stat_points--; st->journal_hash = fold(st->journal_hash, (unsigned long long)(code<<8 | 1)); rogue_attr__journal_append(code,0); return 0; }

int rogue_attr_respec(RogueAttributeState* st, char code){ if(!st) return -1; if(st->respec_tokens <=0) return -2; int* p = attr_ptr(st, code); if(!p) return -3; if(*p <=0) return -4; (*p)--; st->respec_tokens--; g_app.unspent_stat_points++; st->journal_hash = fold(st->journal_hash, (unsigned long long)(code<<8 | 2)); rogue_attr__journal_append(code,1); return 0; }

unsigned long long rogue_attr_fingerprint(const RogueAttributeState* st){ if(!st) return 0; unsigned long long h=0xcbf29ce484222325ULL; h=fold(h,(unsigned long long)st->strength); h=fold(h,(unsigned long long)st->dexterity); h=fold(h,(unsigned long long)st->vitality); h=fold(h,(unsigned long long)st->intelligence); h=fold(h,(unsigned long long)st->spent_points); h=fold(h,(unsigned long long)st->respec_tokens); h=fold(h, st->journal_hash); return h; }

/* Phase 12.3: Attribute operation journal implementation */
typedef struct RogueAttrOp { char code; char kind; } RogueAttrOp;
void rogue_attr__journal_append(char code, int kind){ RogueAttrOp op; op.code=code; op.kind=(char)kind; if(g_attr_state.op_count==g_attr_state.op_cap){ int nc = g_attr_state.op_cap? g_attr_state.op_cap*2:32; void* nre = realloc(g_attr_state.ops, sizeof(RogueAttrOp)*(size_t)nc); if(!nre) return; g_attr_state.ops=(RogueAttrOp*)nre; g_attr_state.op_cap=nc; } if(g_attr_state.ops){ g_attr_state.ops[g_attr_state.op_count++]=op; } }
int rogue_attr_journal_count(void){ return g_attr_state.op_count; }
int rogue_attr_journal_entry(int index, char* code_out, int* kind_out){ if(index<0||index>=g_attr_state.op_count) return -1; RogueAttrOp* op=&g_attr_state.ops[index]; if(code_out) *code_out=op->code; if(kind_out) *kind_out=op->kind; return 0; }
