/* dialogue.c - Dialogue System Phase 0 implementation */
#include "core/dialogue.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef ROGUE_DIALOGUE_MAX_SCRIPTS
#define ROGUE_DIALOGUE_MAX_SCRIPTS 64
#endif

static RogueDialogueScript g_scripts[ROGUE_DIALOGUE_MAX_SCRIPTS];
static int g_script_count = 0;

static const RogueDialogueScript* find_script(int id){
    for(int i=0;i<g_script_count;i++) if(g_scripts[i].id==id) return &g_scripts[i];
    return NULL;
}
static int find_script_index(int id){
    for(int i=0;i<g_script_count;i++) if(g_scripts[i].id==id) return i; return -1;
}

static void trim(char* s){ if(!s) return; int len=(int)strlen(s); int a=0; while(a<len && (s[a]==' '||s[a]=='\t' || s[a]=='\r')) a++; int b=len-1; while(b>=a && (s[b]==' '||s[b]=='\t' || s[b]=='\r')) b--; if(a>0 || b<len-1){ int n=b-a+1; if(n<0) n=0; memmove(s, s+a, (size_t)n); s[n]='\0'; } }

static int parse_and_register(int id, const char* buffer, int length){
    if(find_script(id)) return -2; /* duplicate */
    if(g_script_count >= ROGUE_DIALOGUE_MAX_SCRIPTS) return -3; /* capacity */
    /* First pass: count valid lines */
    int line_count=0; const char* p=buffer; const char* end=buffer+length;
    while(p<end){ const char* line_start=p; const char* nl=(const char*)memchr(p,'\n', (size_t)(end-p)); int l = (int)(nl? (nl-p): (end-p));
        int valid=0; for(int i=0;i<l;i++){ if(line_start[i]=='#'){ valid=0; break; } if(line_start[i]=='|'){ valid=1; break; } if(line_start[i]>' '){ /* some non-space char before '|' */ }
        }
        if(valid) line_count++; p = nl? nl+1 : end; }
    if(line_count==0){ return -4; }
    /* Allocate blob: lines array + copies of each line (speaker + text) */
    size_t lines_bytes = sizeof(RogueDialogueLine)* (size_t)line_count;
    /* Rough over-allocation for strings: original buffer length + line_count for null terminators */
    size_t blob_size = lines_bytes + (size_t)length + 8;
    char* blob = (char*)malloc(blob_size); if(!blob) return -5;
    RogueDialogueLine* lines = (RogueDialogueLine*)blob;
    char* str_cursor = blob + lines_bytes; size_t str_remaining = blob_size - lines_bytes;
    memset(lines,0,lines_bytes);
    /* Second pass: populate */
    p=buffer; int idx=0; while(p<end && idx<line_count){ const char* line_start=p; const char* nl=(const char*)memchr(p,'\n',(size_t)(end-p)); int l=(int)(nl? (nl-p):(end-p));
        /* copy to temp buffer for modification */
        char temp[1024]; if(l> (int)sizeof(temp)-1) l = (int)sizeof(temp)-1; memcpy(temp,line_start,(size_t)l); temp[l]='\0';
        p = nl? nl+1 : end;
        /* Skip empty / comment */
        int allspace=1; for(int i=0;i<l;i++){ if(temp[i]>' '){ allspace=0; break; } }
        if(allspace || temp[0]=='#') continue;
        char* bar = strchr(temp,'|'); if(!bar) continue; *bar='\0'; char* speaker=temp; char* text=bar+1; trim(speaker); while(*text==' '||*text=='\t') text++; /* left trim only */
        /* store strings */
        size_t speaker_len = strlen(speaker); size_t text_len = strlen(text);
        if(speaker_len+text_len+2 > str_remaining){ free(blob); return -6; }
        char* speaker_copy = str_cursor; memcpy(speaker_copy,speaker,speaker_len+1); str_cursor += speaker_len+1; str_remaining -= speaker_len+1;
        char* text_copy = str_cursor; memcpy(text_copy,text,text_len+1); str_cursor += text_len+1; str_remaining -= text_len+1;
        lines[idx].speaker_id = speaker_copy; lines[idx].text = text_copy; idx++;
    }
    line_count = idx; if(line_count==0){ free(blob); return -7; }
    RogueDialogueScript* dst = &g_scripts[g_script_count++];
    *dst = (RogueDialogueScript){ id, line_count, lines, blob, (int)blob_size };
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
    for(int i=0;i<g_script_count;i++){ free(g_scripts[i]._blob); g_scripts[i]._blob=NULL; g_scripts[i].lines=NULL; }
    g_script_count=0;
}
