#include "core/progression_attributes.h"
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

int rogue_attr_spend(RogueAttributeState* st, char code){ if(!st) return -1; if(g_app.unspent_stat_points <=0) return -2; int* p = attr_ptr(st, code); if(!p) return -3; (*p)++; st->spent_points++; g_app.unspent_stat_points--; st->journal_hash = fold(st->journal_hash, (unsigned long long)(code<<8 | 1)); return 0; }

int rogue_attr_respec(RogueAttributeState* st, char code){ if(!st) return -1; if(st->respec_tokens <=0) return -2; int* p = attr_ptr(st, code); if(!p) return -3; if(*p <=0) return -4; (*p)--; st->respec_tokens--; g_app.unspent_stat_points++; st->journal_hash = fold(st->journal_hash, (unsigned long long)(code<<8 | 2)); return 0; }

unsigned long long rogue_attr_fingerprint(const RogueAttributeState* st){ if(!st) return 0; unsigned long long h=0xcbf29ce484222325ULL; h=fold(h,(unsigned long long)st->strength); h=fold(h,(unsigned long long)st->dexterity); h=fold(h,(unsigned long long)st->vitality); h=fold(h,(unsigned long long)st->intelligence); h=fold(h,(unsigned long long)st->spent_points); h=fold(h,(unsigned long long)st->respec_tokens); h=fold(h, st->journal_hash); return h; }
