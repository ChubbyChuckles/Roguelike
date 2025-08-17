/* dialogue.c - Dialogue System Phase 0 implementation */
#include "core/dialogue.h"
#include "util/log.h"
#include "ui/core/ui_context.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "core/save_manager.h"

#ifndef ROGUE_DIALOGUE_MAX_SCRIPTS
#define ROGUE_DIALOGUE_MAX_SCRIPTS 64
#endif

static RogueDialogueScript g_scripts[ROGUE_DIALOGUE_MAX_SCRIPTS];
static int g_script_count = 0;
static RogueDialoguePlayback g_playback; /* zero-inited */
/* Phase 2 token context */
static char g_player_name[64] = "Player"; /* default */
static unsigned int g_run_seed = 0;
/* Phase 3 execution accumulators (simple test harness state) */
static const char* g_flags[64]; static int g_flag_count=0;
typedef struct { int item_id; int qty; } GrantedItem;
static GrantedItem g_items[64]; static int g_item_count=0;

void rogue_dialogue_set_player_name(const char* name){ if(!name||!*name) return; size_t n=strlen(name); if(n>sizeof(g_player_name)-1) n=sizeof(g_player_name)-1; memcpy(g_player_name,name,n); g_player_name[n]='\0'; }
void rogue_dialogue_set_run_seed(unsigned int seed){ g_run_seed = seed; }

/* Expand tokens in a source string into dest buffer; returns length. */
static size_t expand_tokens(const char* src, char* dest, size_t cap){
    if(!src||!dest||cap==0) return 0; size_t out=0; const char* p=src; dest[0]='\0';
    while(*p && out<cap-1){
        if(p[0]=='$' && p[1]=='{' ){ const char* close=strchr(p,'}'); if(close){ size_t key_len=(size_t)(close-(p+2)); char key[32]; if(key_len>=sizeof(key)) key_len=sizeof(key)-1; memcpy(key,p+2,key_len); key[key_len]='\0'; const char* rep=NULL; char seed_buf[32];
                if(strcmp(key,"player_name")==0) rep=g_player_name; else if(strcmp(key,"run_seed")==0){ snprintf(seed_buf,sizeof seed_buf,"%u", g_run_seed); rep=seed_buf; }
                if(rep){ size_t rl=strlen(rep); if(rl>cap-1-out) rl=cap-1-out; memcpy(dest+out,rep,rl); out+=rl; p=close+1; continue; }
            }
        }
        dest[out++] = *p++;
    }
    dest[out]='\0';
    return out;
}

int rogue_dialogue_current_text(char* buffer, size_t cap){
    if(!buffer||cap==0) return -1; const RogueDialoguePlayback* pb = rogue_dialogue_playback(); if(!pb) return -2; const RogueDialogueScript* sc = rogue_dialogue_get(pb->script_id); if(!sc) return -3; if(pb->line_index<0||pb->line_index>=sc->line_count) return -4; const RogueDialogueLine* ln=&sc->lines[pb->line_index];
    if(ln->token_flags & ROGUE_DIALOGUE_LINE_HAS_TOKENS){ return (int)expand_tokens(ln->text, buffer, cap); }
    size_t len=strlen(ln->text); if(len>cap-1) len=cap-1; memcpy(buffer,ln->text,len); buffer[len]='\0'; return (int)len;
}

static const RogueDialogueScript* find_script(int id){
    for(int i=0;i<g_script_count;i++) if(g_scripts[i].id==id) return &g_scripts[i];
    return NULL;
}
static int find_script_index(int id){
    for(int i=0;i<g_script_count;i++) if(g_scripts[i].id==id) return i; return -1;
}

static void trim(char* s){ if(!s) return; int len=(int)strlen(s); int a=0; while(a<len && (s[a]==' '||s[a]=='\t' || s[a]=='\r')) a++; int b=len-1; while(b>=a && (s[b]==' '||s[b]=='\t' || s[b]=='\r')) b--; if(a>0 || b<len-1){ int n=b-a+1; if(n<0) n=0; memmove(s, s+a, (size_t)n); s[n]='\0'; } }

static int parse_and_register(int id, const char* buffer, int length){
    if(find_script(id)) return -2;
    if(g_script_count >= ROGUE_DIALOGUE_MAX_SCRIPTS) return -3;
    int line_count=0; const char* p=buffer; const char* end=buffer+length;
    while(p<end){ const char* nl=(const char*)memchr(p,'\n',(size_t)(end-p)); int l=(int)(nl? (nl-p):(end-p));
        int has_bar=0; for(int i=0;i<l;i++){ if(p[i]=='#'){ has_bar=0; break; } if(p[i]=='|'){ has_bar=1; break; } }
        if(has_bar) line_count++; p = nl? nl+1 : end; }
    if(line_count==0) return -4;
    size_t lines_bytes = sizeof(RogueDialogueLine)* (size_t)line_count;
    size_t blob_size = lines_bytes + (size_t)length + 8;
    char* blob=(char*)malloc(blob_size); if(!blob) return -5; RogueDialogueLine* lines=(RogueDialogueLine*)blob; memset(lines,0,lines_bytes);
    char* str_cursor=blob+lines_bytes; size_t str_remaining = blob_size-lines_bytes;
    p=buffer; int idx=0;
    while(p<end && idx<line_count){ const char* nl=(const char*)memchr(p,'\n',(size_t)(end-p)); int l=(int)(nl? (nl-p):(end-p));
        char temp[1024]; if(l> (int)sizeof(temp)-1) l=(int)sizeof(temp)-1; memcpy(temp,p,(size_t)l); temp[l]='\0'; p = nl? nl+1 : end;
        int allspace=1; for(int i=0;i<l;i++){ if(temp[i]>' '){ allspace=0; break; } } if(allspace || temp[0]=='#') continue;
        char* bar=strchr(temp,'|'); if(!bar) continue; *bar='\0'; char* speaker=temp; char* text=bar+1; trim(speaker); while(*text==' '||*text=='\t') text++;
        RogueDialogueEffect* eff_list=NULL; unsigned char eff_count=0; /* effect sections may be chained with additional '|' delimiters */
        char* next_section = strchr(text,'|');
        while(next_section){ *next_section='\0'; char* eff_section = next_section+1; /* parse this section (comma separated effects) */
            char* cursor=eff_section; while(*cursor){ while(*cursor==' '||*cursor==',') cursor++; if(!*cursor) break; if(strncmp(cursor,"SET_FLAG(",9)==0){ cursor+=9; char name[24]; int n=0; while(*cursor && *cursor!=')' && n<(int)sizeof(name)-1){ name[n++]=*cursor++; } name[n]='\0'; if(*cursor==')') cursor++; if(eff_count<4){ if(!eff_list) eff_list=(RogueDialogueEffect*)calloc(4,sizeof(RogueDialogueEffect)); eff_list[eff_count].kind=ROGUE_DIALOGUE_EFFECT_SET_FLAG; for(int ci=0; ci<(int)sizeof(eff_list[eff_count].name); ++ci) eff_list[eff_count].name[ci]=0; for(int ci=0; name[ci] && ci<(int)sizeof(eff_list[eff_count].name)-1; ++ci) eff_list[eff_count].name[ci]=name[ci]; eff_count++; } }
                else if(strncmp(cursor,"GIVE_ITEM(",10)==0){ cursor+=10; int idv=0, qtyv=0; while(isdigit((unsigned char)*cursor)){ idv=idv*10+(*cursor-'0'); cursor++; } if(*cursor==','){ cursor++; while(isdigit((unsigned char)*cursor)){ qtyv=qtyv*10+(*cursor-'0'); cursor++; } } if(qtyv<=0) qtyv=1; if(*cursor==')') cursor++; if(eff_count<4){ if(!eff_list) eff_list=(RogueDialogueEffect*)calloc(4,sizeof(RogueDialogueEffect)); eff_list[eff_count].kind=ROGUE_DIALOGUE_EFFECT_GIVE_ITEM; eff_list[eff_count].a=(unsigned short)idv; eff_list[eff_count].b=(unsigned short)qtyv; eff_count++; } }
                while(*cursor && *cursor!=',') cursor++; if(*cursor==',') cursor++; }
            next_section = strchr(next_section+1,'|');
        }
        size_t speaker_len=strlen(speaker); size_t text_len=strlen(text); if(speaker_len+text_len+2 > str_remaining){ if(eff_list) free(eff_list); free(blob); return -6; }
        char* speaker_copy=str_cursor; memcpy(speaker_copy,speaker,speaker_len+1); str_cursor+=speaker_len+1; str_remaining-=speaker_len+1;
        char* text_copy=str_cursor; memcpy(text_copy,text,text_len+1); str_cursor+=text_len+1; str_remaining-=text_len+1;
        unsigned int token_flags=0u; if(strstr(text_copy,"${")) token_flags|=ROGUE_DIALOGUE_LINE_HAS_TOKENS;
        lines[idx].speaker_id=speaker_copy; lines[idx].text=text_copy; lines[idx].token_flags=token_flags; lines[idx].effects=eff_list; lines[idx].effect_count=eff_count; idx++;
    }
    line_count=idx; if(line_count==0){ free(blob); return -7; }
    RogueDialogueScript* dst=&g_scripts[g_script_count++]; *dst=(RogueDialogueScript){ id, line_count, lines, blob, (int)blob_size, 0ull };
    return 0;
}

int rogue_dialogue_register_from_buffer(int id, const char* buffer, int length){
    if(!buffer || length<=0) return -1; return parse_and_register(id, buffer, length);
}

/* Simple file slurp (binary) */
static int load_file(const char* path, char** out_buf, int* out_len){
    FILE* f = NULL;
#if defined(_MSC_VER)
    if(fopen_s(&f, path, "rb")!=0 || !f) return -1;
#else
    f = fopen(path, "rb"); if(!f) return -1;
#endif
    if(fseek(f,0,SEEK_END)!=0){ fclose(f); return -2; }
    long sz = ftell(f); if(sz<0){ fclose(f); return -3; } rewind(f);
    char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); return -4; }
    size_t rd = fread(buf,1,(size_t)sz,f); fclose(f); buf[rd]='\0'; *out_buf=buf; *out_len=(int)rd; return 0;
}

int rogue_dialogue_load_script_from_file(int id, const char* path){
    if(!path) return -1; char* buf=NULL; int len=0; int r=load_file(path,&buf,&len); if(r!=0) return -2; int pr = parse_and_register(id, buf, len); free(buf); return pr;
}

const RogueDialogueScript* rogue_dialogue_get(int id){ return find_script(id); }

int rogue_dialogue_script_count(void){ return g_script_count; }

void rogue_dialogue_reset(void){
    for(int i=0;i<g_script_count;i++){ if(g_scripts[i].lines){ for(int l=0;l<g_scripts[i].line_count;l++){ if(g_scripts[i].lines[l].effects) free(g_scripts[i].lines[l].effects); } } free(g_scripts[i]._blob); g_scripts[i]._blob=NULL; g_scripts[i].lines=NULL; }
    g_script_count=0;
    g_playback = (RogueDialoguePlayback){0};
    g_flag_count=0; g_item_count=0;
}

/* Phase 4 Persistence capture/restore */
int rogue_dialogue_capture(RogueDialoguePersistState* out){
    if(!out) return -1; memset(out,0,sizeof *out); if(!g_playback.active) return 0; out->active=1; out->script_id=g_playback.script_id; out->line_index=g_playback.line_index; out->reveal_ms=g_playback.reveal_ms; return 1;
}
int rogue_dialogue_restore(const RogueDialoguePersistState* st){
    if(!st) return -1; if(!st->active){ g_playback=(RogueDialoguePlayback){0}; return 0; }
    const RogueDialogueScript* sc = rogue_dialogue_get(st->script_id); if(!sc) return -2; if(st->line_index<0 || st->line_index>=sc->line_count) return -3; g_playback=(RogueDialoguePlayback){1, st->script_id, st->line_index, st->reveal_ms, 1}; return 0;
}

/* Save component (choose id 9 unused in save manager enum range) */
#define ROGUE_SAVE_COMP_DIALOGUE 9
static int dialogue_write_fn(FILE* f){ RogueDialoguePersistState st; int cap=rogue_dialogue_capture(&st); (void)cap; return fwrite(&st, sizeof st,1,f)==1?0:-1; }
static int dialogue_read_fn(FILE* f, size_t size){ if(size!=sizeof(RogueDialoguePersistState)) return -1; RogueDialoguePersistState st; if(fread(&st,sizeof st,1,f)!=1) return -2; return rogue_dialogue_restore(&st); }
void rogue_dialogue_register_save_component(void){ static RogueSaveComponent comp={ROGUE_SAVE_COMP_DIALOGUE, dialogue_write_fn, dialogue_read_fn, "dialogue"}; rogue_save_manager_register(&comp); }

const RogueDialoguePlayback* rogue_dialogue_playback(void){ return g_playback.active? &g_playback : NULL; }

int rogue_dialogue_start(int script_id){
    const RogueDialogueScript* sc = rogue_dialogue_get(script_id); if(!sc) return -1;
    g_playback = (RogueDialoguePlayback){1, script_id, 0, 0.0f, 1};
    rogue_dialogue_log_current_line();
    return 0;
}

void rogue_dialogue_log_current_line(void){
    if(!g_playback.active) return; const RogueDialogueScript* sc = rogue_dialogue_get(g_playback.script_id); if(!sc) return;
    if(g_playback.line_index < 0 || g_playback.line_index >= sc->line_count) return;
    const RogueDialogueLine* ln = &sc->lines[g_playback.line_index];
    char expanded[512]; const char* text = ln->text;
    if(ln->token_flags & ROGUE_DIALOGUE_LINE_HAS_TOKENS){ expand_tokens(ln->text, expanded, sizeof expanded); text=expanded; }
    ROGUE_LOG_INFO("DIALOGUE[%d] %s: %s", sc->id, ln->speaker_id, text);
}

int rogue_dialogue_advance(void){
    if(!g_playback.active) return -1;
    const RogueDialogueScript* sc = rogue_dialogue_get(g_playback.script_id); if(!sc) { g_playback.active=0; return -2; }
    if(g_playback.line_index +1 < sc->line_count){
        g_playback.line_index++; g_playback.reveal_ms = 0.0f; rogue_dialogue_log_current_line(); return 1;
    } else { /* end */
        g_playback = (RogueDialoguePlayback){0}; return 0;
    }
}

void rogue_dialogue_update(double dt_ms){ if(!g_playback.active) return; g_playback.reveal_ms += (float)dt_ms; }

int rogue_dialogue_render_ui(struct RogueUIContext* ui){
    if(!ui || !g_playback.active) return 0; const RogueDialogueScript* sc = rogue_dialogue_get(g_playback.script_id); if(!sc) return 0;
    if(g_playback.line_index <0 || g_playback.line_index >= sc->line_count) return 0;
    const RogueDialogueLine* ln = &sc->lines[g_playback.line_index];
    /* Phase 3: execute line effects once */
    unsigned long long mask_bit = (g_playback.line_index < 64)? (1ull << g_playback.line_index):0ull;
    if(mask_bit && !(sc->_executed_mask & mask_bit)){
        ((RogueDialogueScript*)sc)->_executed_mask |= mask_bit; /* cast away const for internal mutation */
        for(int ei=0; ei<ln->effect_count; ++ei){ const RogueDialogueEffect* ef = &ln->effects[ei]; if(ef->kind==ROGUE_DIALOGUE_EFFECT_SET_FLAG){ if(g_flag_count<64){ g_flags[g_flag_count++]=ef->name; } }
            else if(ef->kind==ROGUE_DIALOGUE_EFFECT_GIVE_ITEM){ if(g_item_count<64){ g_items[g_item_count].item_id=ef->a; g_items[g_item_count].qty+= (int)ef->b; g_item_count++; } }
        }
    }
    /* Basic panel layout bottom of screen; using arbitrary coordinates (future: compute from viewport) */
    float panel_w = 420.0f, panel_h = 110.0f; float x = 16.0f, y = 360.0f; /* assumptions */
    uint32_t bg = 0x202020C0u; uint32_t fg = 0xFFFFFFFFu; uint32_t speaker_col = 0x80FFD040u;
    rogue_ui_panel(ui, (RogueUIRect){x,y,panel_w,panel_h}, bg);
    rogue_ui_text(ui, (RogueUIRect){x+12,y+10,panel_w-24,18}, ln->speaker_id, speaker_col);
    /* Phase 2: expand tokens into stack buffer */
    char expanded[512]; const char* text = ln->text; if(ln->token_flags & ROGUE_DIALOGUE_LINE_HAS_TOKENS){ expand_tokens(text, expanded, sizeof expanded); text=expanded; }
    rogue_ui_text(ui, (RogueUIRect){x+12,y+34,panel_w-24,48}, text, fg);
    rogue_ui_text(ui, (RogueUIRect){x+panel_w-80,y+panel_h-22,68,16}, "[Enter]", 0xA0A0A0FFu);
    return 1;
}

/* Phase 3 introspection */
int rogue_dialogue_effect_flag_count(void){ return g_flag_count; }
const char* rogue_dialogue_effect_flag(int index){ if(index<0||index>=g_flag_count) return NULL; return g_flags[index]; }
int rogue_dialogue_effect_item_count(void){ return g_item_count; }
int rogue_dialogue_effect_item(int index, int* out_item_id, int* out_qty){ if(index<0||index>=g_item_count) return 0; if(out_item_id) *out_item_id=g_items[index].item_id; if(out_qty) *out_qty=g_items[index].qty; return 1; }
