/* Phase 16.2: Set builder + live bonus preview + JSON tooling */
#include "core/equipment_content.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef ROGUE_SET_CAP
#define ROGUE_SET_CAP 64
#endif
#ifndef ROGUE_RUNEWORD_CAP
#define ROGUE_RUNEWORD_CAP 64
#endif

static RogueSetDef g_sets[ROGUE_SET_CAP]; static int g_set_count=0;
static RogueRuneword g_runewords[ROGUE_RUNEWORD_CAP]; static int g_runeword_count=0;

void rogue_sets_reset(void){ g_set_count=0; }
void rogue_runewords_reset(void){ g_runeword_count=0; }

int rogue_set_validate(const RogueSetDef* def){ if(!def||def->set_id<=0||def->bonus_count<=0||def->bonus_count>4) return -1; int last_pieces=0; for(int i=0;i<def->bonus_count;i++){ const RogueSetBonus* b=&def->bonuses[i]; if(b->pieces<=last_pieces) return -2; last_pieces=b->pieces; } return 0; }
int rogue_set_register(const RogueSetDef* def){ if(rogue_set_validate(def)!=0) return -1; if(g_set_count>=ROGUE_SET_CAP) return -2; g_sets[g_set_count]=*def; return g_set_count++; }
const RogueSetDef* rogue_set_at(int index){ if(index<0||index>=g_set_count) return NULL; return &g_sets[index]; }
int rogue_set_count(void){ return g_set_count; }
const RogueSetDef* rogue_set_find(int set_id){ for(int i=0;i<g_set_count;i++) if(g_sets[i].set_id==set_id) return &g_sets[i]; return NULL; }

int rogue_runeword_register(const RogueRuneword* rw){ if(!rw||!rw->pattern[0]) return -1; if(g_runeword_count>=ROGUE_RUNEWORD_CAP) return -2; g_runewords[g_runeword_count]=*rw; return g_runeword_count++; }
const RogueRuneword* rogue_runeword_at(int index){ if(index<0||index>=g_runeword_count) return NULL; return &g_runewords[index]; }
int rogue_runeword_count(void){ return g_runeword_count; }
const RogueRuneword* rogue_runeword_find(const char* pattern){ if(!pattern) return NULL; for(int i=0;i<g_runeword_count;i++) if(strcmp(g_runewords[i].pattern,pattern)==0) return &g_runewords[i]; return NULL; }

void rogue_set_preview_apply(int set_id, int equipped_count, int* strength,int* dexterity,int* vitality,int* intelligence,int* armor_flat,int* r_fire,int* r_cold,int* r_light,int* r_poison,int* r_status,int* r_phys){ const RogueSetDef* sd=rogue_set_find(set_id); if(!sd||equipped_count<=0) return; const RogueSetBonus* last=NULL; const RogueSetBonus* next=NULL; for(int i=0;i<sd->bonus_count;i++){ const RogueSetBonus* b=&sd->bonuses[i]; if(equipped_count>=b->pieces){ last=b; } else { next=b; break; } } if(!last){ last=&sd->bonuses[0]; next=last; } int ls=last->pieces; int ns=next?next->pieces:ls; float t=0.f; if(next && equipped_count>ls && equipped_count<ns) t=(float)(equipped_count-ls)/(float)(ns-ls); if(strength) *strength += last->strength + (next? (int)((next->strength-last->strength)*t):0); if(dexterity) *dexterity += last->dexterity + (next? (int)((next->dexterity-last->dexterity)*t):0); if(vitality) *vitality += last->vitality + (next? (int)((next->vitality-last->vitality)*t):0); if(intelligence) *intelligence += last->intelligence + (next? (int)((next->intelligence-last->intelligence)*t):0); if(armor_flat) *armor_flat += last->armor_flat + (next? (int)((next->armor_flat-last->armor_flat)*t):0); if(r_fire) *r_fire += last->resist_fire + (next? (int)((next->resist_fire-last->resist_fire)*t):0); if(r_cold) *r_cold += last->resist_cold + (next? (int)((next->resist_cold-last->resist_cold)*t):0); if(r_light) *r_light += last->resist_light + (next? (int)((next->resist_light-last->resist_light)*t):0); if(r_poison) *r_poison += last->resist_poison + (next? (int)((next->resist_poison-last->resist_poison)*t):0); if(r_status) *r_status += last->resist_status + (next? (int)((next->resist_status-last->resist_status)*t):0); if(r_phys) *r_phys += last->resist_physical + (next? (int)((next->resist_physical-last->resist_physical)*t):0); }

/* JSON Loader for sets */
static const char* ws(const char* s){ while(*s && (unsigned char)*s<=32) ++s; return s; }
static const char* jstring(const char* s,char* out,int cap){ s=ws(s); if(*s!='"') return NULL; ++s; int i=0; while(*s && *s!='"'){ if(*s=='\\'&&s[1]) s++; if(i+1<cap) out[i++]=*s; ++s;} if(*s!='"') return NULL; out[i]='\0'; return s+1; }
static const char* jnumber(const char* s,int* out){ s=ws(s); char* end=NULL; long v=strtol(s,&end,10); if(end==s) return NULL; *out=(int)v; return end; }
int rogue_sets_load_from_json(const char* path){ if(!path) return -1; FILE* f=NULL; 
#if defined(_MSC_VER)
 if(fopen_s(&f,path,"rb")!=0) f=NULL;
#else
 f=fopen(path,"rb");
#endif
 if(!f) return -1; fseek(f,0,SEEK_END); long sz=ftell(f); if(sz<0){ fclose(f); return -1;} fseek(f,0,SEEK_SET); char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); return -1;} size_t rd=fread(buf,1,(size_t)sz,f); buf[rd]='\0'; fclose(f); const char* s=ws(buf); if(*s!='['){ free(buf); return -1;} ++s; int added=0; char key[64]; char k2[64]; while(1){ s=ws(s); if(*s==']'){ ++s; break; } if(*s!='{') break; ++s; RogueSetDef def; memset(&def,0,sizeof def); while(1){ s=ws(s); if(*s=='}'){ ++s; break; } const char* ns=jstring(s,key,sizeof key); if(!ns) break; s=ws(ns); if(*s!=':') break; ++s; s=ws(s); if(strcmp(key,"set_id")==0){ int v; const char* vs=jnumber(s,&v); if(!vs){ free(buf); return added; } s=ws(vs); def.set_id=v; } else if(strcmp(key,"bonuses")==0){ if(*s!='['){ free(buf); return added; } ++s; while(1){ s=ws(s); if(*s==']'){ ++s; break; } if(*s!='{') break; ++s; RogueSetBonus b; memset(&b,0,sizeof b); while(1){ s=ws(s); if(*s=='}'){ ++s; break; } const char* ns2=jstring(s,k2,sizeof k2); if(!ns2){ s++; continue; } s=ws(ns2); if(*s!=':') break; ++s; if(strcmp(k2,"pieces")==0){ int v; const char* vs=jnumber(s,&v); if(!vs){ break; } b.pieces=v; s=ws(vs);} else { int v; const char* vs=jnumber(s,&v); if(!vs){ break; } if(strcmp(k2,"strength")==0) b.strength=v; else if(strcmp(k2,"dexterity")==0) b.dexterity=v; else if(strcmp(k2,"vitality")==0) b.vitality=v; else if(strcmp(k2,"intelligence")==0) b.intelligence=v; else if(strcmp(k2,"armor_flat")==0) b.armor_flat=v; else if(strcmp(k2,"resist_fire")==0) b.resist_fire=v; else if(strcmp(k2,"resist_cold")==0) b.resist_cold=v; else if(strcmp(k2,"resist_light")==0) b.resist_light=v; else if(strcmp(k2,"resist_poison")==0) b.resist_poison=v; else if(strcmp(k2,"resist_status")==0) b.resist_status=v; else if(strcmp(k2,"resist_physical")==0) b.resist_physical=v; s=ws(vs);} s=ws(s); if(*s==','){ ++s; continue; } }
                if(def.bonus_count<4) def.bonuses[def.bonus_count++]=b; s=ws(s); if(*s==','){ ++s; continue; } }
        } else { int dummy; const char* vs=jnumber(s,&dummy); if(!vs){ free(buf); return added; } s=ws(vs); }
        s=ws(s); if(*s==','){ ++s; continue; }
    }
        if(def.set_id>0 && def.bonus_count>0 && g_set_count<ROGUE_SET_CAP){ if(rogue_set_validate(&def)==0){ g_sets[g_set_count++]=def; added++; } }
    s=ws(s); if(*s==','){ ++s; continue; }
 }
 free(buf); return added; }

int rogue_sets_export_json(char* buf,int cap){ if(!buf||cap<4) return -1; int off=0; buf[off++]='['; for(int i=0;i<g_set_count;i++){ if(off+2>=cap) return -1; if(i>0) buf[off++]=','; buf[off++]='{'; char tmp[256]; int n=snprintf(tmp,sizeof tmp,"\"set_id\":%d,\"bonuses\":[", g_sets[i].set_id); if(n<0||off+n>=cap) return -1; memcpy(buf+off,tmp,(size_t)n); off+=n; for(int b=0;b<g_sets[i].bonus_count;b++){ if(off+2>=cap) return -1; if(b>0) buf[off++]=','; const RogueSetBonus* sb=&g_sets[i].bonuses[b]; n=snprintf(tmp,sizeof tmp,"{\"pieces\":%d,\"strength\":%d,\"dexterity\":%d,\"vitality\":%d,\"intelligence\":%d,\"armor_flat\":%d,\"resist_fire\":%d,\"resist_cold\":%d,\"resist_light\":%d,\"resist_poison\":%d,\"resist_status\":%d,\"resist_physical\":%d}", sb->pieces,sb->strength,sb->dexterity,sb->vitality,sb->intelligence,sb->armor_flat,sb->resist_fire,sb->resist_cold,sb->resist_light,sb->resist_poison,sb->resist_status,sb->resist_physical); if(n<0||off+n>=cap) return -1; memcpy(buf+off,tmp,(size_t)n); off+=n; }
        if(off+2>=cap) return -1; buf[off++]=']'; buf[off++]='}'; }
 if(off+2>=cap) return -1; buf[off++]=']'; buf[off]='\0'; return off; }
