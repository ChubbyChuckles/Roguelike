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
#include "graphics/font.h"
#include "core/app_state.h"
#include "graphics/sprite.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#ifndef ROGUE_DIALOGUE_MAX_SCRIPTS
#define ROGUE_DIALOGUE_MAX_SCRIPTS 64
#endif

static RogueDialogueScript g_scripts[ROGUE_DIALOGUE_MAX_SCRIPTS];
static int g_script_count = 0;
static RogueDialoguePlayback g_playback; /* zero-inited */
static int g_typewriter_enabled = 0; static float g_chars_per_ms = 0.05f;
/* Phase 7 analytics */
static int g_lines_viewed[ROGUE_DIALOGUE_MAX_SCRIPTS]; /* parallel by registry index */
static double g_last_view_time_ms[ROGUE_DIALOGUE_MAX_SCRIPTS];
static unsigned int g_digest_accum = 2166136261u; /* FNV1a seed across viewed lines */
/* Phase 2 token context */
static char g_player_name[64] = "Player"; /* default */
static unsigned int g_run_seed = 0;
/* Phase 3 execution accumulators (simple test harness state) */
static const char* g_flags[64]; static int g_flag_count=0;
typedef struct { int item_id; int qty; } GrantedItem;
static GrantedItem g_items[64]; static int g_item_count=0;

/* Avatar registry (speaker -> texture) */
#define ROGUE_DIALOGUE_MAX_AVATARS 32
typedef struct RogueDialogueAvatar { char speaker[64]; RogueTexture tex; int loaded; } RogueDialogueAvatar;
static RogueDialogueAvatar g_avatars[ROGUE_DIALOGUE_MAX_AVATARS]; static int g_avatar_count=0;
/* Dialogue style (theme) */
static RogueDialogueStyle g_style = { 0xFF222228u, 0xFF1A1A1Fu, 0xFF5F5F8Cu, 0xFFFFDC8Cu, 0xFFFFFFFFu, 0x80000000u, 1, 1, 1, 1, 0, 0xFFAA8844u, 2, 0, 0x40C8A050u, 0x30FFD080u, 2, 1, 1 };
static RogueTexture g_parchment_tex; static int g_parchment_loaded=0; /* optional parchment paper */

int rogue_dialogue_avatar_register(const char* speaker_id, const char* image_path){
#if defined(ROGUE_HAVE_SDL)
    if(!speaker_id||!*speaker_id||!image_path||!*image_path) return -1;
    for(int i=0;i<g_avatar_count;i++) if(strcmp(g_avatars[i].speaker,speaker_id)==0){
        if(g_avatars[i].loaded){ rogue_texture_destroy(&g_avatars[i].tex); g_avatars[i].loaded=0; }
        if(rogue_texture_load(&g_avatars[i].tex,image_path)) g_avatars[i].loaded=1; return g_avatars[i].loaded?0:-2; }
    if(g_avatar_count >= ROGUE_DIALOGUE_MAX_AVATARS) return -3;
    RogueDialogueAvatar* av=&g_avatars[g_avatar_count++]; memset(av,0,sizeof *av); {
        size_t sl = strlen(speaker_id); if(sl>sizeof(av->speaker)-1) sl=sizeof(av->speaker)-1; memcpy(av->speaker,speaker_id,sl); av->speaker[sl]='\0'; }
    if(rogue_texture_load(&av->tex,image_path)){ av->loaded=1; return 0; } else { g_avatar_count--; return -4; }
#else
    (void)speaker_id; (void)image_path; return -10;
#endif
}
void rogue_dialogue_avatar_reset(void){
#if defined(ROGUE_HAVE_SDL)
    for(int i=0;i<g_avatar_count;i++){ if(g_avatars[i].loaded){ rogue_texture_destroy(&g_avatars[i].tex); g_avatars[i].loaded=0; }}
#endif
    g_avatar_count=0;
}
RogueTexture* rogue_dialogue_avatar_get(const char* speaker_id){
#if defined(ROGUE_HAVE_SDL)
    if(!speaker_id) return NULL; for(int i=0;i<g_avatar_count;i++) if(strcmp(g_avatars[i].speaker,speaker_id)==0 && g_avatars[i].loaded) return &g_avatars[i].tex; return NULL;
#else
    (void)speaker_id; return NULL;
#endif
}

int rogue_dialogue_load_avatars_from_file(const char* path){
    if(!path) return -1; FILE* f=NULL; int line_no=0; int loaded=0;
#if defined(_MSC_VER)
    if(fopen_s(&f,path,"rb")!=0 || !f) return -2;
#else
    f=fopen(path,"rb"); if(!f) return -2;
#endif
    char line[512];
    while(1){
        char* got = fgets(line,sizeof line,f);
        if(!got) break;
        line_no++;
        char* p=line; while(*p==' '||*p=='\t') p++;
        if(*p=='#'||*p=='\n'||!*p) continue;
        char* eq=strchr(p,'='); if(!eq) continue; *eq='\0';
        char* speaker=p; char* img=eq+1; /* trim trail */
        char* nl=strchr(img,'\n'); if(nl) *nl='\0';
        char* cr=strchr(img,'\r'); if(cr) *cr='\0';
        /* trim spaces end */ int sl=(int)strlen(speaker); while(sl>0 && (speaker[sl-1]==' '||speaker[sl-1]=='\t')) speaker[--sl]='\0'; while(*img==' '||*img=='\t') img++; int il=(int)strlen(img); while(il>0 && (img[il-1]==' '||img[il-1]=='\t')) img[--il]='\0'; if(*speaker && *img){ if(rogue_dialogue_avatar_register(speaker,img)==0) loaded++; }
    }
    fclose(f); return loaded; /* number of avatars loaded */
}

int rogue_dialogue_style_set(const RogueDialogueStyle* style){ if(!style) return -1; g_style=*style; return 0; }
const RogueDialogueStyle* rogue_dialogue_style_get(void){ return &g_style; }

/* --- Minimal JSON helpers (non-recursive, tolerant) --- */
static const char* jd_skip_ws(const char* s){ while(*s==' '||*s=='\n'||*s=='\r'||*s=='\t') ++s; return s; }
static unsigned int jd_hex_nibble(char c){ if(c>='0'&&c<='9') return (unsigned int)(c-'0'); if(c>='a'&&c<='f') return 10u+(unsigned int)(c-'a'); if(c>='A'&&c<='F') return 10u+(unsigned int)(c-'A'); return 0u; }
static int jd_parse_color(const char* s, unsigned int* out){ if(!s||!out) return -1; if(s[0]=='#'){ size_t len=strlen(s); if(len==7){ unsigned int v=0; for(int i=1;i<7;i++){ v=(v<<4)|jd_hex_nibble(s[i]); } *out=0xFF000000u|v; return 0; } }
    else if(s[0]=='0' && (s[1]=='x'||s[1]=='X')){ unsigned int v=0; for(int i=2; s[i] && i<10; ++i){ char c=s[i]; if(!isxdigit((unsigned char)c)) break; v=(v<<4)|jd_hex_nibble(c); } *out=v; return 0; }
    else { /* decimal */ unsigned int v=0; for(int i=0;s[i];++i){ if(s[i]<'0'||s[i]>'9') break; v = v*10u + (unsigned int)(s[i]-'0'); } *out=v; return 0; }
    return -1; }
static int jd_extract_string(const char* json, const char* key, char* out, size_t cap){
    const char* search = json;
    size_t key_len = strlen(key);
    while(1){
        const char* found = strstr(search, key);
        if(!found) return -1;
        search = found + key_len; /* advance for next search if fails */
        const char* colon = strchr(found, ':');
        if(!colon) continue;
        const char* v = jd_skip_ws(colon+1);
        if(*v!='"') continue;
        v++;
        const char* end = strchr(v,'"');
        if(!end) return -1;
        size_t len=(size_t)(end-v); if(len>cap-1) len=cap-1;
        memcpy(out,v,len); out[len]='\0'; return 0;
    }
}
static int jd_extract_int(const char* json, const char* key, int* out){
    const char* search = json; size_t key_len=strlen(key);
    while(1){
        const char* found = strstr(search,key);
        if(!found) return -1;
        search = found + key_len;
        const char* colon = strchr(found, ':'); if(!colon) continue;
        const char* v = jd_skip_ws(colon+1); int sign=1; if(*v=='-'){ sign=-1; v++; }
        if(!isdigit((unsigned char)*v)) continue;
        int val=0; while(isdigit((unsigned char)*v)){ val = val*10 + (*v-'0'); v++; }
        *out = val*sign; return 0;
    }
}

/* Simple file slurp (binary) placed here so JSON loaders can use it */
static int load_file(const char* path, char** out_buf, int* out_len){
    FILE* f = NULL;
#if defined(_MSC_VER)
    int fopen_result = fopen_s(&f, path, "rb");
    if(fopen_result!=0 || !f) return -1;
#else
    f = fopen(path, "rb");
    if(!f) return -1;
#endif
    if(fseek(f,0,SEEK_END)!=0){ fclose(f); return -2; }
    long sz = ftell(f); if(sz<0){ fclose(f); return -3; } rewind(f);
    char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); return -4; }
    size_t rd = fread(buf,1,(size_t)sz,f); fclose(f); buf[rd]='\0'; *out_buf=buf; *out_len=(int)rd; return 0;
}

int rogue_dialogue_style_load_from_json(const char* path){
    if(!path) return -1;
    char* buf=NULL; int len=0; if(load_file(path,&buf,&len)!=0) return -2; buf[len]='\0';
    RogueDialogueStyle st=g_style; char tmp[128]; unsigned int col; int iv;
    if(jd_extract_string(buf,"panel_color_top",tmp,sizeof tmp)==0){ if(jd_parse_color(tmp,&col)==0) st.panel_color_top=col; }
    if(jd_extract_string(buf,"panel_color_bottom",tmp,sizeof tmp)==0){ if(jd_parse_color(tmp,&col)==0) st.panel_color_bottom=col; }
    if(jd_extract_string(buf,"border_color",tmp,sizeof tmp)==0){ if(jd_parse_color(tmp,&col)==0) st.border_color=col; }
    if(jd_extract_string(buf,"speaker_color",tmp,sizeof tmp)==0){ if(jd_parse_color(tmp,&col)==0) st.speaker_color=col; }
    if(jd_extract_string(buf,"text_color",tmp,sizeof tmp)==0){ if(jd_parse_color(tmp,&col)==0) st.text_color=col; }
    if(jd_extract_string(buf,"text_shadow_color",tmp,sizeof tmp)==0){ if(jd_parse_color(tmp,&col)==0) st.text_shadow_color=col; }
    if(jd_extract_string(buf,"accent_color",tmp,sizeof tmp)==0){ if(jd_parse_color(tmp,&col)==0) st.accent_color=col; }
    if(jd_extract_string(buf,"parchment_texture",tmp,sizeof tmp)==0){ if(rogue_texture_load(&g_parchment_tex,tmp)) g_parchment_loaded=1; }
    if(jd_extract_int(buf,"enable_gradient",&iv)==0) st.enable_gradient=iv;
    if(jd_extract_int(buf,"enable_text_shadow",&iv)==0) st.enable_text_shadow=iv;
    if(jd_extract_int(buf,"show_blink_prompt",&iv)==0) st.show_blink_prompt=iv;
    if(jd_extract_int(buf,"show_caret",&iv)==0) st.show_caret=iv;
    if(jd_extract_int(buf,"panel_height",&iv)==0) st.panel_height=iv;
    if(jd_extract_int(buf,"border_thickness",&iv)==0) st.border_thickness=iv;
    if(jd_extract_int(buf,"use_parchment",&iv)==0) st.use_parchment=iv;
    if(jd_extract_string(buf,"glow_color",tmp,sizeof tmp)==0){ if(jd_parse_color(tmp,&col)==0) st.glow_color=col; }
    if(jd_extract_string(buf,"rune_strip_color",tmp,sizeof tmp)==0){ if(jd_parse_color(tmp,&col)==0) st.rune_strip_color=col; }
    if(jd_extract_int(buf,"glow_strength",&iv)==0) st.glow_strength=iv;
    if(jd_extract_int(buf,"corner_ornaments",&iv)==0) st.corner_ornaments=iv;
    if(jd_extract_int(buf,"vignette",&iv)==0) st.vignette=iv;
    g_style=st; free(buf); return 0; }

/* Mood -> tint color helper (portable case-insensitive compare) */
static int rd_strcasecmp(const char* a, const char* b){ if(!a||!b) return (a?1:(b?-1:0)); while(*a && *b){ unsigned char ca=(unsigned char)*a; unsigned char cb=(unsigned char)*b; if(ca>='A'&&ca<='Z') ca=(unsigned char)(ca+32); if(cb>='A'&&cb<='Z') cb=(unsigned char)(cb+32); if(ca!=cb) return (int)ca - (int)cb; ++a; ++b; } return (int)(unsigned char)*a - (int)(unsigned char)*b; }
static unsigned int rd_mood_tint(const char* mood){
    if(!mood||!*mood) return 0xFFFFFFFFu; /* neutral default */
    if(rd_strcasecmp(mood,"neutral")==0) return 0xFFFFFFFFu; /* no tint */
    if(rd_strcasecmp(mood,"angry")==0)   return 0xFFFF6040u; /* fiery red/orange */
    if(rd_strcasecmp(mood,"excited")==0) return 0xFFFFD040u; /* bright gold */
    if(rd_strcasecmp(mood,"happy")==0)   return 0xFF80E070u; /* soft green */
    return 0xFFFFFFFFu; /* unknown -> neutral */
}
static int rd_debug_enabled(void){
#if defined(_MSC_VER)
    char* val = NULL; size_t len = 0; if(_dupenv_s(&val,&len,"ROGUE_DIALOGUE_DEBUG")!=0 || !val) return 0; int enabled = (*val=='1'||*val=='t'||*val=='T'||*val=='y'||*val=='Y'); free(val); return enabled;
#else
    const char* env = getenv("ROGUE_DIALOGUE_DEBUG");
    return (env && (*env=='1'||*env=='t'||*env=='T'||*env=='y'||*env=='Y'))?1:0;
#endif
}
static void rd_validate_mood(char* mood){ if(!mood||!*mood) return; const char* allowed[] = {"neutral","angry","excited","happy"}; for(size_t i=0;i<sizeof(allowed)/sizeof(allowed[0]);++i){ if(rd_strcasecmp(mood,allowed[i])==0) return; } if(rd_debug_enabled()) ROGUE_LOG_WARN("Dialogue: mood '%s' not allowed (forcing neutral)", mood); /* safe copy */
#if defined(_MSC_VER)
    strncpy_s(mood,64,"neutral",_TRUNCATE);
#else
    strncpy(mood,"neutral",63); mood[63]='\0';
#endif
}
int rogue_dialogue_load_script_from_json_file(const char* path){
    if(!path) return -1; char* buf=NULL; int len=0; int lf=load_file(path,&buf,&len); if(lf!=0){ ROGUE_LOG_WARN("Dialogue JSON open failed (%d): %s", lf, path); return -2; } buf[len]='\0';
    if(rd_debug_enabled()){ ROGUE_LOG_INFO("Dialogue JSON bytes=%d path=%s", len, path); }
    int registered=0; int lines_total=0; int lines_kept=0; int lines_skipped=0; int single=0;
    char* scripts_root = strstr(buf,"\"scripts\"");
    if(scripts_root){ /* multi-script */
    char* arr=strchr(scripts_root,'['); if(!arr){ free(buf); return -3; }
    /* Find matching closing bracket for scripts array (handle nested brackets inside objects) */
    char* arr_end=NULL; int bdepth=0; for(char* p=arr; *p; ++p){ if(*p=='['){ if(bdepth==0 && p!=arr) { /* nested array start */ } bdepth++; } else if(*p==']'){ bdepth--; if(bdepth==0){ arr_end=p; break; } } }
    if(!arr_end){ free(buf); return -4; }
        char* s=arr;
        while(s < arr_end){
            char* sobj=strchr(s,'{'); if(!sobj||sobj>=arr_end) break;
            /* Find matching closing brace for this script object (brace depth) */
            char* sobj_end=NULL; int depth=0; for(char* p=sobj; p<arr_end; ++p){ if(*p=='{'){ depth++; } else if(*p=='}'){ depth--; if(depth==0){ sobj_end=p; break; } } }
            if(!sobj_end) break; /* malformed */
            size_t slen = (size_t)(sobj_end - sobj + 1);
            /* Copy full script object into temp buffer (enlarge if needed) */
            char scopy[16384]; if(slen > sizeof scopy - 1) slen = sizeof scopy - 1; memcpy(scopy, sobj, slen); scopy[slen]='\0';
            int sid=-1; jd_extract_int(scopy,"id",&sid);
            if(sid>=0){
                char* lines_sec=strstr(scopy,"\"lines\"");
                if(lines_sec){
                    char* larr=strchr(lines_sec,'[');
                    if(larr){
                        char* larr_end=strchr(larr,']');
                        if(larr_end){
                            char temp[20000]; size_t out=0; char* lc=larr; int line_idx=0;
                            while(lc<larr_end){
                                char* lobj=strchr(lc,'{'); if(!lobj||lobj>=larr_end) break;
                                char* lobj_end=NULL; int ldepth=0; for(char* q=lobj; q<larr_end; ++q){ if(*q=='{'){ ldepth++; } else if(*q=='}'){ ldepth--; if(ldepth==0){ lobj_end=q; break; } } }
                                if(!lobj_end) break;
                                char lcopy[1024]; size_t llen=(size_t)(lobj_end-lobj+1); if(llen>sizeof lcopy -1) llen=sizeof lcopy -1; memcpy(lcopy,lobj,llen); lcopy[llen]='\0';
                                char speaker[64]="",textv[512]=""; char race[64]="", name[64]="", mood[64]=""; char side[16]="", mirror[16]="";
                                jd_extract_string(lcopy,"speaker",speaker,sizeof speaker); jd_extract_string(lcopy,"text",textv,sizeof textv);
                                jd_extract_string(lcopy,"race",race,sizeof race); jd_extract_string(lcopy,"name",name,sizeof name);
                                jd_extract_string(lcopy,"mood",mood,sizeof mood); jd_extract_string(lcopy,"side",side,sizeof side);
                                jd_extract_string(lcopy,"mirror",mirror,sizeof mirror);
                                if(mood[0]) rd_validate_mood(mood);
                                lines_total++;
                                if(!(speaker[0]&&textv[0])){ lines_skipped++; if(rd_debug_enabled()) ROGUE_LOG_WARN("Dialogue script %d skip line %d (missing speaker/text)", sid, line_idx); lc=lobj_end+1; line_idx++; continue; }
                                char avatar_path[256]="";
                                if(race[0]&&name[0]&&mood[0]) {
                                    /* Path scheme: assets/avatar_icons/<race>/<name>_<mood>.png (replaces older nested mood folder). */
                                    snprintf(avatar_path,sizeof avatar_path,"../assets/avatar_icons/%s/%s_%s.png",race,name,mood);
                                }
                                int sflag=(strcmp(side,"right")==0); int vflag=(mirror[0]=='v'||mirror[0]=='V'); unsigned int tint=rd_mood_tint(mood);
                                char meta[320]; if(avatar_path[0]) snprintf(meta,sizeof meta,"%s;S=%d;V=%d;TR=%u;TG=%u;TB=%u",avatar_path,sflag,vflag,(tint>>16)&255,(tint>>8)&255,tint&255); else meta[0]='\0';
                                int n = avatar_path[0]? snprintf(temp+out,sizeof(temp)-out,"%s%s@%s|%s\n",out?"":"",speaker,meta,textv):snprintf(temp+out,sizeof(temp)-out,"%s%s|%s\n",out?"":"",speaker,textv);
                                if(n>0){ out += (size_t)n; lines_kept++; if(rd_debug_enabled()) ROGUE_LOG_INFO("Dialogue script %d keep line %d speaker='%s' text_len=%zu", sid, line_idx, speaker, strlen(textv)); } else { lines_skipped++; }
                                lc = lobj_end+1; line_idx++;
                            }
                            if(out>0 && rogue_dialogue_register_from_buffer(sid,temp,(int)out)==0){ registered++; if(rd_debug_enabled()) ROGUE_LOG_INFO("Dialogue script %d registered lines=%d", sid, lines_kept); }
                            else if(rd_debug_enabled()) { ROGUE_LOG_WARN("Dialogue script %d registration failed (out=%zu)", sid, out); }
                        }
                    }
                }
            }
            s = sobj_end+1;
        }
        if(rd_debug_enabled()) ROGUE_LOG_INFO("Dialogue multi summary scripts=%d lines_total=%d kept=%d skipped=%d", registered, lines_total, lines_kept, lines_skipped);
        free(buf); return (registered>0)?0:-5;
    }
    /* single */
    single=1; int script_id=-1; jd_extract_int(buf,"id",&script_id); if(script_id<0){ if(rd_debug_enabled()) ROGUE_LOG_WARN("Dialogue single missing id"); free(buf); return -6; }
    char* lines_sec=strstr(buf,"\"lines\""); if(!lines_sec){ free(buf); return -7; }
    char* arr=strchr(lines_sec,'['); if(!arr){ free(buf); return -8; }
    char* arr_end=strchr(arr,']'); if(!arr_end){ free(buf); return -9; }
    char temp[20000]; size_t out=0; char* cursor=arr; int line_idx=0; while(cursor<arr_end){ char* obj=strchr(cursor,'{'); if(!obj||obj>=arr_end) break; char* obj_end=strchr(obj,'}'); if(!obj_end||obj_end>arr_end) break; char oc[1024]; size_t olen=(size_t)(obj_end-obj+1); if(olen>sizeof oc -1) olen=sizeof oc -1; memcpy(oc,obj,olen); oc[olen]='\0'; char speaker[64]="",textv[512]=""; char race[64]="",name[64]="",mood[64]=""; char side[16]="",mirror[16]=""; jd_extract_string(oc,"speaker",speaker,sizeof speaker); jd_extract_string(oc,"text",textv,sizeof textv); jd_extract_string(oc,"race",race,sizeof race); jd_extract_string(oc,"name",name,sizeof name); jd_extract_string(oc,"mood",mood,sizeof mood); jd_extract_string(oc,"side",side,sizeof side); jd_extract_string(oc,"mirror",mirror,sizeof mirror); if(mood[0]) rd_validate_mood(mood); lines_total++; if(!(speaker[0]&&textv[0])){ lines_skipped++; if(rd_debug_enabled()) ROGUE_LOG_WARN("Dialogue single skip line %d (missing speaker/text)", line_idx); cursor=obj_end+1; line_idx++; continue; } char avatar_path[256]=""; if(race[0]&&name[0]&&mood[0]) { snprintf(avatar_path,sizeof avatar_path,"../assets/avatar_icons/%s/%s_%s.png",race,name,mood); } int sflag=(strcmp(side,"right")==0); int vflag=(mirror[0]=='v'||mirror[0]=='V'); unsigned int tint=rd_mood_tint(mood); char meta[320]; if(avatar_path[0]) snprintf(meta,sizeof meta,"%s;S=%d;V=%d;TR=%u;TG=%u;TB=%u",avatar_path,sflag,vflag,(tint>>16)&255,(tint>>8)&255,tint&255); else meta[0]='\0'; int n= avatar_path[0]? snprintf(temp+out,sizeof(temp)-out,"%s%s@%s|%s\n",out?"":"",speaker,meta,textv):snprintf(temp+out,sizeof(temp)-out,"%s%s|%s\n",out?"":"",speaker,textv); if(n>0){ out+=(size_t)n; lines_kept++; if(rd_debug_enabled()) ROGUE_LOG_INFO("Dialogue single keep line %d speaker='%s' text_len=%zu", line_idx, speaker, strlen(textv)); } else { lines_skipped++; } cursor=obj_end+1; line_idx++; }
    int r=(out>0)?rogue_dialogue_register_from_buffer(script_id,temp,(int)out):-10; if(rd_debug_enabled()) ROGUE_LOG_INFO("Dialogue single summary id=%d status=%d lines_total=%d kept=%d skipped=%d", script_id, r, lines_total, lines_kept, lines_skipped); free(buf); return r; }

/* Phase 5 Localization Storage */
typedef struct RogueLocEntry { char locale[8]; char key[64]; char value[256]; } RogueLocEntry;
#ifndef ROGUE_DIALOGUE_MAX_LOC_ENTRIES
#define ROGUE_DIALOGUE_MAX_LOC_ENTRIES 256
#endif
static RogueLocEntry g_loc_entries[ROGUE_DIALOGUE_MAX_LOC_ENTRIES];
static int g_loc_entry_count = 0;
static char g_active_locale[8] = "en"; /* default */

int rogue_dialogue_locale_register(const char* locale, const char* key, const char* value){
    if(!locale||!*locale||!key||!*key||!value) return -1;
    for(int i=0;i<g_loc_entry_count;i++){
        if(strcmp(g_loc_entries[i].locale, locale)==0 && strcmp(g_loc_entries[i].key, key)==0){
            size_t vl=strlen(value); if(vl>sizeof(g_loc_entries[i].value)-1) vl=sizeof(g_loc_entries[i].value)-1;
            memcpy(g_loc_entries[i].value,value,vl); g_loc_entries[i].value[vl]='\0';
            return 0;
        }
    }
    if(g_loc_entry_count >= ROGUE_DIALOGUE_MAX_LOC_ENTRIES) return -2;
    RogueLocEntry* e = &g_loc_entries[g_loc_entry_count++];
    size_t ll=strlen(locale); if(ll>sizeof(e->locale)-1) ll=sizeof(e->locale)-1; memcpy(e->locale,locale,ll); e->locale[ll]='\0';
    size_t kl=strlen(key); if(kl>sizeof(e->key)-1) kl=sizeof(e->key)-1; memcpy(e->key,key,kl); e->key[kl]='\0';
    size_t vl=strlen(value); if(vl>sizeof(e->value)-1) vl=sizeof(e->value)-1; memcpy(e->value,value,vl); e->value[vl]='\0';
    return 0;
}
int rogue_dialogue_locale_set(const char* locale){ if(!locale||!*locale) return -1; size_t ll=strlen(locale); if(ll>sizeof(g_active_locale)-1) ll=sizeof(g_active_locale)-1; memcpy(g_active_locale,locale,ll); g_active_locale[ll]='\0'; return 0; }
const char* rogue_dialogue_locale_active(void){ return g_active_locale; }
static const char* loc_lookup(const char* key){ for(int i=0;i<g_loc_entry_count;i++){ if(strcmp(g_loc_entries[i].locale,g_active_locale)==0 && strcmp(g_loc_entries[i].key,key)==0) return g_loc_entries[i].value; } return NULL; }
/* For IS_KEY lines we store key\0fallback_text\0 in text field region */
static const char* localized_fallback(const RogueDialogueLine* ln){ if(!ln) return NULL; if(!(ln->token_flags & ROGUE_DIALOGUE_LINE_IS_KEY)) return NULL; const char* key=ln->text; size_t klen=strlen(key); const char* fb = key + klen + 1; if(!*fb) return NULL; return fb; }

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
    const char* text = ln->text;
    if(ln->token_flags & ROGUE_DIALOGUE_LINE_IS_KEY){ const char* loc = loc_lookup(ln->text); if(!loc) loc = localized_fallback(ln); if(loc) text=loc; }
    if(ln->token_flags & ROGUE_DIALOGUE_LINE_HAS_TOKENS){ return (int)expand_tokens(text, buffer, cap); }
    size_t len=strlen(text); if(len>cap-1) len=cap-1; memcpy(buffer,text,len); buffer[len]='\0'; return (int)len;
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
    /* Optional inline avatar syntax: Speaker@path;S=...;V=...;TR=..;TG=..;TB=.. */
    char* at = strchr(speaker,'@'); if(at && at[1]){ *at='\0'; char* avatar_path = at+1; trim(speaker); trim(avatar_path); if(*avatar_path){ int side_flag=0; int v_flag=0; int tr=-1,tg=-1,tb=-1; char* meta=strchr(avatar_path,';'); while(meta){ if(strncmp(meta,";S=",3)==0 && isdigit((unsigned char)meta[3])) side_flag=(meta[3]=='1'); else if(strncmp(meta,";V=",3)==0 && isdigit((unsigned char)meta[3])) v_flag=(meta[3]=='1'); else if(strncmp(meta,";TR=",4)==0) tr=atoi(meta+4); else if(strncmp(meta,";TG=",4)==0) tg=atoi(meta+4); else if(strncmp(meta,";TB=",4)==0) tb=atoi(meta+4); char* next_meta=strchr(meta+1,';'); if(!next_meta) break; meta=next_meta; } char* semi=strchr(avatar_path,';'); if(semi) *semi='\0'; if(*avatar_path){ rogue_dialogue_avatar_register(speaker, avatar_path); } lines[idx]._reserved[0]=(unsigned char)(side_flag?1:0); lines[idx]._reserved[1]=(unsigned char)(v_flag?1:0); if(tr>=0&&tr<=255 && tg>=0&&tg<=255 && tb>=0&&tb<=255){ lines[idx]._reserved[2]=(unsigned char)tr; lines[idx]._reserved[3]=(unsigned char)tg; lines[idx]._reserved[4]=(unsigned char)tb; lines[idx]._reserved[5]=255; } }}
        RogueDialogueEffect* eff_list=NULL; unsigned char eff_count=0; /* effect sections may be chained with additional '|' delimiters */
        char* next_section = strchr(text,'|');
        while(next_section){ *next_section='\0'; char* eff_section = next_section+1; /* parse this section (comma separated effects) */
            char* cursor=eff_section; while(*cursor){ while(*cursor==' '||*cursor==',') cursor++; if(!*cursor) break; if(strncmp(cursor,"SET_FLAG(",9)==0){ cursor+=9; char name[24]; int n=0; while(*cursor && *cursor!=')' && n<(int)sizeof(name)-1){ name[n++]=*cursor++; } name[n]='\0'; if(*cursor==')') cursor++; if(eff_count<4){ if(!eff_list) eff_list=(RogueDialogueEffect*)calloc(4,sizeof(RogueDialogueEffect)); eff_list[eff_count].kind=ROGUE_DIALOGUE_EFFECT_SET_FLAG; for(int ci=0; ci<(int)sizeof(eff_list[eff_count].name); ++ci) eff_list[eff_count].name[ci]=0; for(int ci=0; name[ci] && ci<(int)sizeof(eff_list[eff_count].name)-1; ++ci) eff_list[eff_count].name[ci]=name[ci]; eff_count++; } }
                else if(strncmp(cursor,"GIVE_ITEM(",10)==0){ cursor+=10; int idv=0, qtyv=0; while(isdigit((unsigned char)*cursor)){ idv=idv*10+(*cursor-'0'); cursor++; } if(*cursor==','){ cursor++; while(isdigit((unsigned char)*cursor)){ qtyv=qtyv*10+(*cursor-'0'); cursor++; } } if(qtyv<=0) qtyv=1; if(*cursor==')') cursor++; if(eff_count<4){ if(!eff_list) eff_list=(RogueDialogueEffect*)calloc(4,sizeof(RogueDialogueEffect)); eff_list[eff_count].kind=ROGUE_DIALOGUE_EFFECT_GIVE_ITEM; eff_list[eff_count].a=(unsigned short)idv; eff_list[eff_count].b=(unsigned short)qtyv; eff_count++; } }
                while(*cursor && *cursor!=',') cursor++; if(*cursor==',') cursor++; }
            next_section = strchr(next_section+1,'|');
        }
        int is_key=0; const char* key_part=NULL; const char* fallback_part=NULL;
        if(text[0]=='['){ char* close_br = strchr(text,']'); if(close_br && close_br>text+1){ *close_br='\0'; key_part=text+1; fallback_part=close_br+1; while(*fallback_part==' '||*fallback_part=='\t') fallback_part++; is_key=1; } }
        size_t speaker_len=strlen(speaker);
        if(!is_key){
            size_t text_len=strlen(text);
            if(speaker_len+text_len+2 > str_remaining){ if(eff_list) free(eff_list); free(blob); return -6; }
            char* speaker_copy=str_cursor; memcpy(speaker_copy,speaker,speaker_len+1); str_cursor+=speaker_len+1; str_remaining-=speaker_len+1;
            char* text_copy=str_cursor; memcpy(text_copy,text,text_len+1); str_cursor+=text_len+1; str_remaining-=text_len+1;
            unsigned int token_flags=0u; if(strstr(text_copy,"${")) token_flags|=ROGUE_DIALOGUE_LINE_HAS_TOKENS;
            lines[idx].speaker_id=speaker_copy; lines[idx].text=text_copy; lines[idx].token_flags=token_flags; lines[idx].effects=eff_list; lines[idx].effect_count=eff_count; idx++;
        } else {
            if(!key_part){ if(eff_list) free(eff_list); continue; }
            if(!fallback_part||!*fallback_part) fallback_part="";
            size_t key_len=strlen(key_part); size_t fb_len=strlen(fallback_part);
            if(speaker_len + key_len + fb_len + 3 > str_remaining){ if(eff_list) free(eff_list); free(blob); return -6; }
            char* speaker_copy=str_cursor; memcpy(speaker_copy,speaker,speaker_len+1); str_cursor+=speaker_len+1; str_remaining-=speaker_len+1;
            char* key_copy=str_cursor; memcpy(key_copy,key_part,key_len+1); str_cursor+=key_len+1; str_remaining-=key_len+1;
            char* fb_copy=str_cursor; memcpy(fb_copy,fallback_part,fb_len+1); str_cursor+=fb_len+1; str_remaining-=fb_len+1;
            unsigned int token_flags=ROGUE_DIALOGUE_LINE_IS_KEY; if(strstr(fb_copy,"${")) token_flags|=ROGUE_DIALOGUE_LINE_HAS_TOKENS;
            lines[idx].speaker_id=speaker_copy; lines[idx].text=key_copy; lines[idx].token_flags=token_flags; lines[idx].effects=eff_list; lines[idx].effect_count=eff_count; idx++;
        }
    }
    line_count=idx; if(line_count==0){ free(blob); return -7; }
    RogueDialogueScript* dst=&g_scripts[g_script_count++]; *dst=(RogueDialogueScript){ id, line_count, lines, blob, (int)blob_size, 0ull };
    return 0;
}

int rogue_dialogue_register_from_buffer(int id, const char* buffer, int length){
    if(!buffer || length<=0) return -1; return parse_and_register(id, buffer, length);
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
    g_flag_count=0; g_item_count=0; g_loc_entry_count=0; g_active_locale[0]='e'; g_active_locale[1]='n'; g_active_locale[2]='\0';
    for(int i=0;i<ROGUE_DIALOGUE_MAX_SCRIPTS;i++){ g_lines_viewed[i]=0; g_last_view_time_ms[i]=0.0; }
    g_digest_accum = 2166136261u;
    rogue_dialogue_avatar_reset();
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
    if(ln->token_flags & ROGUE_DIALOGUE_LINE_IS_KEY){ const char* loc = loc_lookup(ln->text); if(!loc) loc = localized_fallback(ln); if(loc) text=loc; }
    if(ln->token_flags & ROGUE_DIALOGUE_LINE_HAS_TOKENS){ expand_tokens(text, expanded, sizeof expanded); text=expanded; }
    ROGUE_LOG_INFO("DIALOGUE[%d] %s: %s", sc->id, ln->speaker_id, text);
    /* Phase 7 analytics update: on first display of a line (log moment) increment counters */
    int script_index = find_script_index(sc->id);
    if(script_index>=0){
        g_lines_viewed[script_index]++;
        extern struct RogueAppState g_app; g_last_view_time_ms[script_index] = g_app.game_time_ms;
        /* FNV1a mix script id, line index, first 4 chars hash */
        unsigned int h = (unsigned int)sc->id; h ^= (unsigned int)g_playback.line_index; h *= 16777619u; if(text){ for(int i=0;i<4 && text[i]; ++i){ h ^= (unsigned char)text[i]; h *= 16777619u; } }
        g_digest_accum ^= h; g_digest_accum *= 16777619u;
    }
}

int rogue_dialogue_advance(void){
    if(!g_playback.active) return -1;
    const RogueDialogueScript* sc = rogue_dialogue_get(g_playback.script_id); if(!sc) { g_playback.active=0; return -2; }
    /* Phase 6 skip behavior: if typewriter active and current line not fully revealed yet, finish reveal instead of advancing */
    if(g_typewriter_enabled){
        const RogueDialogueLine* ln=&sc->lines[g_playback.line_index];
        const char* text = ln->text; if(ln->token_flags & ROGUE_DIALOGUE_LINE_IS_KEY){ const char* loc=loc_lookup(ln->text); if(!loc) loc=localized_fallback(ln); if(loc) text=loc; }
        char tmp[512]; if(ln->token_flags & ROGUE_DIALOGUE_LINE_HAS_TOKENS){ expand_tokens(text,tmp,sizeof tmp); text=tmp; }
        size_t full_len = strlen(text);
        float shown_chars = g_playback.reveal_ms * g_chars_per_ms;
        if(shown_chars + 0.5f < (float)full_len){ /* not fully revealed */
            /* jump reveal to end */
            g_playback.reveal_ms = (float)full_len / (g_chars_per_ms>0?g_chars_per_ms:0.001f);
            return 2; /* indicate reveal completed, no line advance */
        }
    }
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
    char expanded[512]; const char* text = ln->text; if(ln->token_flags & ROGUE_DIALOGUE_LINE_IS_KEY){ const char* loc = loc_lookup(ln->text); if(!loc) loc = localized_fallback(ln); if(loc) text=loc; } if(ln->token_flags & ROGUE_DIALOGUE_LINE_HAS_TOKENS){ expand_tokens(text, expanded, sizeof expanded); text=expanded; }
    const char* draw_text = text; char temp_tw[512];
    if(g_typewriter_enabled){ size_t full_len=strlen(text); float shown_chars = g_playback.reveal_ms * g_chars_per_ms; size_t shown = (size_t)(shown_chars+0.01f); if(shown>full_len) shown=full_len; memcpy(temp_tw,text,shown); temp_tw[shown]='\0'; draw_text=temp_tw; }
    rogue_ui_text(ui, (RogueUIRect){x+12,y+34,panel_w-24,48}, draw_text, fg);
    rogue_ui_text(ui, (RogueUIRect){x+panel_w-80,y+panel_h-22,68,16}, "[Enter]", 0xA0A0A0FFu);
    return 1;
}

#ifdef ROGUE_HAVE_SDL
extern SDL_Renderer* g_internal_sdl_renderer_ref;
#endif

void rogue_dialogue_render_runtime(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_internal_sdl_renderer_ref) return;
    const RogueDialoguePlayback* pb = rogue_dialogue_playback(); if(!pb) return; const RogueDialogueScript* sc = rogue_dialogue_get(pb->script_id); if(!sc) return; if(pb->line_index<0||pb->line_index>=sc->line_count) return;
    const RogueDialogueLine* ln = &sc->lines[pb->line_index];
    char expanded[512]; const char* full_text = ln->text; if(ln->token_flags & ROGUE_DIALOGUE_LINE_IS_KEY){ const char* loc = loc_lookup(ln->text); if(!loc) loc=localized_fallback(ln); if(loc) full_text=loc; } if(ln->token_flags & ROGUE_DIALOGUE_LINE_HAS_TOKENS){ expand_tokens(full_text,expanded,sizeof expanded); full_text=expanded; }
    char temp_tw[512]; const char* draw_text = full_text;
    if(g_typewriter_enabled){ size_t full_len=strlen(full_text); float shown_chars = g_playback.reveal_ms * g_chars_per_ms; size_t shown=(size_t)(shown_chars+0.5f); if(shown>full_len) shown=full_len; memcpy(temp_tw,full_text,shown); temp_tw[shown]='\0'; draw_text=temp_tw; }
    extern struct RogueAppState g_app; int vw = g_app.viewport_w>0? g_app.viewport_w:1280; int vh = g_app.viewport_h>0? g_app.viewport_h:720;
    /* --- Dynamic panel sizing to accommodate large avatars (up to 154x320) --- */
    enum { ROGUE_AVATAR_MAX_W = 154, ROGUE_AVATAR_MAX_H = 320 }; /* logical display caps */
    int panel_w = (vw<700)? vw-20 : 680; int panel_h = g_style.panel_height>0? g_style.panel_height : 180;
    /* Peek avatar now to allow panel enlargement before drawing background */
    RogueTexture* av_peek = rogue_dialogue_avatar_get(ln->speaker_id);
    int avatar_display_h = 0;
    if(av_peek && av_peek->handle){
        int raw_h = av_peek->h; if(raw_h > ROGUE_AVATAR_MAX_H) raw_h = ROGUE_AVATAR_MAX_H; avatar_display_h = raw_h; /* no scale yet; panel height decides scaling */
        int needed_panel_h = avatar_display_h + 40; /* 20px top + 20px bottom padding approx */
        if(needed_panel_h > panel_h) panel_h = needed_panel_h;
        /* Keep panel from covering excessive screen (cap at ~60% viewport height) */
        int max_panel_h = (int)(vh * 0.60f); if(panel_h > max_panel_h){ panel_h = max_panel_h; if(avatar_display_h > panel_h - 40) avatar_display_h = panel_h - 40; }
    }
    int x = (vw-panel_w)/2; int y = vh - panel_h - 30;
    /* Gradient background */
    if(g_style.use_parchment && g_parchment_loaded && g_parchment_tex.handle){ /* draw parchment texture tiled */
        int tiles_x = panel_w / g_parchment_tex.w + 1; int tiles_y = panel_h / g_parchment_tex.h + 1; SDL_Rect dst; for(int ty=0; ty<tiles_y; ++ty){ for(int tx=0; tx<tiles_x; ++tx){ dst.x = x + tx * g_parchment_tex.w; dst.y = y + ty * g_parchment_tex.h; dst.w = g_parchment_tex.w; dst.h = g_parchment_tex.h; SDL_RenderCopy(g_internal_sdl_renderer_ref, g_parchment_tex.handle, NULL, &dst); }}
    } else if(g_style.enable_gradient){
        unsigned int c0=g_style.panel_color_top, c1=g_style.panel_color_bottom;
        unsigned char r0=(unsigned char)((c0>>16)&0xFF), g0=(unsigned char)((c0>>8)&0xFF), b0=(unsigned char)(c0&0xFF), a0=(unsigned char)((c0>>24)&0xFF);
        unsigned char r1=(unsigned char)((c1>>16)&0xFF), g1=(unsigned char)((c1>>8)&0xFF), b1=(unsigned char)(c1&0xFF), a1=(unsigned char)((c1>>24)&0xFF);
        for(int row=0; row<panel_h; ++row){
            float t = (panel_h>1)? (float)row/(float)(panel_h-1) : 0.0f;
            int ri = (int)(r0 + (r1 - r0) * t + 0.5f);
            int gi = (int)(g0 + (g1 - g0) * t + 0.5f);
            int bi = (int)(b0 + (b1 - b0) * t + 0.5f);
            int ai = (int)(a0 + (a1 - a0) * t + 0.5f);
            if(ri<0)ri=0; if(ri>255)ri=255; if(gi<0)gi=0; if(gi>255)gi=255; if(bi<0)bi=0; if(bi>255)bi=255; if(ai<0)ai=0; if(ai>255)ai=255;
            SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,(Uint8)ri,(Uint8)gi,(Uint8)bi,(Uint8)ai);
            SDL_RenderDrawLine(g_internal_sdl_renderer_ref,x,y+row,x+panel_w-1,y+row);
        }
    } else { unsigned int c=g_style.panel_color_top; SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref, (c>>16)&255,(c>>8)&255,c&255,(c>>24)&255); SDL_Rect bg={x,y,panel_w,panel_h}; SDL_RenderFillRect(g_internal_sdl_renderer_ref,&bg);}    
    /* Border */
    unsigned int bc=g_style.border_color; SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,(bc>>16)&255,(bc>>8)&255,bc&255,(bc>>24)&255); SDL_Rect br={x,y,panel_w,panel_h}; int bt = g_style.border_thickness<1?1:g_style.border_thickness; for(int i=0;i<bt;i++){ SDL_Rect r2={br.x+i,br.y+i,br.w-2*i,br.h-2*i}; SDL_RenderDrawRect(g_internal_sdl_renderer_ref,&r2);} unsigned int acc=g_style.accent_color; SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,(acc>>16)&255,(acc>>8)&255,acc&255,(acc>>24)&255); SDL_RenderDrawLine(g_internal_sdl_renderer_ref,x+8,y+28,x+panel_w-8,y+28);
    /* Glow (simple expanding translucent border) */
    if(g_style.glow_strength>0){ unsigned int gc=g_style.glow_color; int layers=g_style.glow_strength; for(int i=1;i<=layers;i++){ Uint8 a=(Uint8)(((gc>>24)&255)/(i+1)); SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,(gc>>16)&255,(gc>>8)&255,gc&255,a); SDL_Rect gr={br.x-i,br.y-i,br.w+2*i,br.h+2*i}; SDL_RenderDrawRect(g_internal_sdl_renderer_ref,&gr);} }
    /* Rune strip overlay across top */
    if(g_style.rune_strip_color>>24){ unsigned int rc=g_style.rune_strip_color; SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,(rc>>16)&255,(rc>>8)&255,rc&255,(rc>>24)&255); SDL_Rect rr={x+10,y+4,panel_w-20,16}; SDL_RenderDrawRect(g_internal_sdl_renderer_ref,&rr); }
    /* Corner ornaments placeholder (simple small filled squares for now) */
    if(g_style.corner_ornaments){ unsigned int oc=g_style.accent_color; SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,(oc>>16)&255,(oc>>8)&255,oc&255,(oc>>24)&255); SDL_Rect o1={x+4,y+4,8,8}; SDL_Rect o2={x+panel_w-12,y+4,8,8}; SDL_Rect o3={x+4,y+panel_h-12,8,8}; SDL_Rect o4={x+panel_w-12,y+panel_h-12,8,8}; SDL_RenderFillRect(g_internal_sdl_renderer_ref,&o1); SDL_RenderFillRect(g_internal_sdl_renderer_ref,&o2); SDL_RenderFillRect(g_internal_sdl_renderer_ref,&o3); SDL_RenderFillRect(g_internal_sdl_renderer_ref,&o4);}    
    /* Vignette darkening inside edges */
    if(g_style.vignette){ for(int i=0;i<12 && i<panel_w/2 && i<panel_h/2;i++){ Uint8 alpha=(Uint8)(8); SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,0,0,0,alpha); SDL_Rect vrect={x+i,y+i,panel_w-2*i,panel_h-2*i}; SDL_RenderDrawRect(g_internal_sdl_renderer_ref,&vrect);} }
    int text_left = x+14;
    RogueTexture* av = av_peek; /* reuse earlier lookup */
    int avatar_side = (ln->_reserved[0]==1)?1:0; /* 0 left, 1 right */
    int avatar_v_mirror = (ln->_reserved[1] & 0x1)?1:0;
    if(av && av->handle){
        int aw=av->w, ah=av->h;
        /* Enforce caps (154x320) and scale proportionally if exceeding either */
        float scale_w = (aw > ROGUE_AVATAR_MAX_W)? (float)ROGUE_AVATAR_MAX_W / (float)aw : 1.0f;
        float scale_h_cap = (ah > ROGUE_AVATAR_MAX_H)? (float)ROGUE_AVATAR_MAX_H / (float)ah : 1.0f;
        int max_panel_available_h = panel_h - 40; if(max_panel_available_h < 20) max_panel_available_h = 20; /* safety */
        float scale_panel_fit = (ah > max_panel_available_h)? (float)max_panel_available_h / (float)ah : 1.0f;
        float scale = scale_w; if(scale_panel_fit < scale) scale = scale_panel_fit; if(scale_h_cap < scale) scale = scale_h_cap; /* choose smallest to satisfy all */
        int dw=(int)(aw*scale); int dh=(int)(ah*scale);
        if(dw > ROGUE_AVATAR_MAX_W) { float adj = (float)ROGUE_AVATAR_MAX_W / (float)dw; dw = ROGUE_AVATAR_MAX_W; dh = (int)(dh*adj); }
        if(dh > max_panel_available_h){ float adj = (float)max_panel_available_h / (float)dh; dh = max_panel_available_h; dw = (int)(dw*adj); }
        SDL_Rect dst;
        if(avatar_side==0){ dst.x = x+12; text_left = dst.x + dw + 20; }
        else { dst.x = x + panel_w - dw - 12; }
        dst.y = y + panel_h - dh - 12; dst.w=dw; dst.h=dh;
        /* Mood tint application if reserved[2..4] populated */
        Uint8 tr=ln->_reserved[2], tg=ln->_reserved[3], tb=ln->_reserved[4]; Uint8 ta=ln->_reserved[5]? ln->_reserved[5] : 255; int has_tint = (ta!=0) && (tr||tg||tb) && !(tr==255 && tg==255 && tb==255);
        Uint8 old_r=255,old_g=255,old_b=255,old_a=255; if(has_tint){ SDL_GetRenderDrawColor(g_internal_sdl_renderer_ref,&old_r,&old_g,&old_b,&old_a); SDL_SetTextureColorMod(av->handle,tr,tg,tb); }
        if(avatar_v_mirror){ SDL_RendererFlip flip = SDL_FLIP_VERTICAL; SDL_RenderCopyEx(g_internal_sdl_renderer_ref,av->handle,NULL,&dst,0,NULL,flip); }
        else { SDL_RenderCopy(g_internal_sdl_renderer_ref,av->handle,NULL,&dst); }
        if(has_tint){ SDL_SetTextureColorMod(av->handle,255,255,255); }
        if(avatar_side==1){ /* keep text_left default when avatar on right */ }
    } else {
        /* Fallback silhouette rectangle tinted by mood (using stored tint; default gray)
           Size scales with panel height but capped at avatar max caps. */
        Uint8 tr=ln->_reserved[2], tg=ln->_reserved[3], tb=ln->_reserved[4]; if(tr==0&&tg==0&&tb==0){ tr=80; tg=80; tb=90; }
        int max_avail_h = panel_h - 40; if(max_avail_h < 72) max_avail_h = 72; if(max_avail_h > ROGUE_AVATAR_MAX_H) max_avail_h = ROGUE_AVATAR_MAX_H;
        int fh = max_avail_h; int fw = fh; if(fw > ROGUE_AVATAR_MAX_W) fw = ROGUE_AVATAR_MAX_W; /* square-ish, limited by width cap */
        SDL_Rect dst; if(avatar_side==0){ dst.x=x+12; text_left=dst.x+fw+20; } else { dst.x = x+panel_w-fw-12; }
        dst.y = y + panel_h - fh - 12; dst.w=fw; dst.h=fh; SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,tr,tg,tb,220); SDL_RenderFillRect(g_internal_sdl_renderer_ref,&dst); SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,0,0,0,255); SDL_RenderDrawRect(g_internal_sdl_renderer_ref,&dst);
    }
    unsigned int sc_col=g_style.speaker_color; rogue_font_draw_text(text_left, y+10, ln->speaker_id?ln->speaker_id:"?",1,(RogueColor){(sc_col>>16)&255,(sc_col>>8)&255,sc_col&255,(sc_col>>24)&255});
    /* Word wrapping with fixed glyph width assumption (6px) */
    int interior_w = (x + panel_w - 14) - text_left; if(interior_w<40) interior_w=40; int char_w=6; int max_chars_line = interior_w / char_w; if(max_chars_line<8) max_chars_line=8;
    const char* p = draw_text; char linebuf[256]; int line_chars=0; int line_idx=0; int base_y = y+38; int max_lines=4;
    unsigned int text_col=g_style.text_color; unsigned int sh_col=g_style.text_shadow_color; int draw_shadow=g_style.enable_text_shadow && (sh_col>>24)!=0;
    while(*p && line_idx < max_lines){ while(*p==' ') p++; if(!*p) break; const char* word=p; int wlen=0; while(word[wlen] && word[wlen] != ' ' && word[wlen] != '\n') wlen++; int needed = (line_chars==0?0:1) + wlen; if(line_chars>0 && needed + line_chars > max_chars_line){ linebuf[line_chars]='\0'; if(draw_shadow) rogue_font_draw_text(text_left+1, base_y + line_idx*20+1, linebuf,1,(RogueColor){(sh_col>>16)&255,(sh_col>>8)&255,sh_col&255,(sh_col>>24)&255}); rogue_font_draw_text(text_left, base_y + line_idx*20, linebuf,1,(RogueColor){(text_col>>16)&255,(text_col>>8)&255,text_col&255,(text_col>>24)&255}); line_idx++; line_chars=0; if(line_idx>=max_lines) break; }
        if(line_chars==0){ int copy=wlen; if(copy> (int)sizeof(linebuf)-1) copy=(int)sizeof(linebuf)-1; memcpy(linebuf, word, copy); line_chars=copy; linebuf[line_chars]='\0'; }
        else { if(line_chars+1 < (int)sizeof(linebuf)-1){ linebuf[line_chars++]=' '; } int copy=wlen; if(line_chars+copy > (int)sizeof(linebuf)-1) copy=(int)sizeof(linebuf)-1 - line_chars; if(copy>0){ memcpy(linebuf+line_chars, word, copy); line_chars+=copy; linebuf[line_chars]='\0'; } }
        p += wlen; if(*p=='\n'){ linebuf[line_chars]='\0'; if(draw_shadow) rogue_font_draw_text(text_left+1, base_y + line_idx*20+1, linebuf,1,(RogueColor){(sh_col>>16)&255,(sh_col>>8)&255,sh_col&255,(sh_col>>24)&255}); rogue_font_draw_text(text_left, base_y + line_idx*20, linebuf,1,(RogueColor){(text_col>>16)&255,(text_col>>8)&255,text_col&255,(text_col>>24)&255}); line_idx++; line_chars=0; p++; }
    }
    if(line_chars>0 && line_idx<max_lines){ linebuf[line_chars]='\0'; if(draw_shadow) rogue_font_draw_text(text_left+1, base_y + line_idx*20+1, linebuf,1,(RogueColor){(sh_col>>16)&255,(sh_col>>8)&255,sh_col&255,(sh_col>>24)&255}); rogue_font_draw_text(text_left, base_y + line_idx*20, linebuf,1,(RogueColor){(text_col>>16)&255,(text_col>>8)&255,text_col&255,(text_col>>24)&255}); }
    /* Typewriter caret */
    if(g_typewriter_enabled){ size_t full_len=strlen(full_text); float shown_chars = g_playback.reveal_ms * g_chars_per_ms; if(shown_chars < (float)full_len && g_style.show_caret){ int caret_phase = ((int)g_playback.reveal_ms/150)%2; if(caret_phase==0){ int cx = text_left + ((int)shown_chars % max_chars_line)*char_w; int cy = base_y + (line_idx<max_lines? line_idx: (max_lines-1))*20; SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref,255,255,255,200); SDL_Rect crr={cx,cy,6,2}; SDL_RenderFillRect(g_internal_sdl_renderer_ref,&crr);} }}
    /* Advance prompt blinking */
    if(g_style.show_blink_prompt){ extern struct RogueAppState g_app; int phase = ((int)g_app.game_time_ms/400)%2; if(phase==0){ rogue_font_draw_text(x+panel_w-70,y+panel_h-24, "[E]",1,(RogueColor){200,200,200,255}); }} else { rogue_font_draw_text(x+panel_w-70,y+panel_h-24, "[E]",1,(RogueColor){190,190,190,255}); }
#endif
}

/* Phase 3 introspection */
int rogue_dialogue_effect_flag_count(void){ return g_flag_count; }
const char* rogue_dialogue_effect_flag(int index){ if(index<0||index>=g_flag_count) return NULL; return g_flags[index]; }
int rogue_dialogue_effect_item_count(void){ return g_item_count; }
int rogue_dialogue_effect_item(int index, int* out_item_id, int* out_qty){ if(index<0||index>=g_item_count) return 0; if(out_item_id) *out_item_id=g_items[index].item_id; if(out_qty) *out_qty=g_items[index].qty; return 1; }

void rogue_dialogue_typewriter_enable(int enabled, float chars_per_ms){ g_typewriter_enabled = enabled?1:0; if(chars_per_ms>0) g_chars_per_ms=chars_per_ms; }

int rogue_dialogue_analytics_get(int script_id, int* out_lines_viewed, double* out_last_view_ms, unsigned int* out_digest){
    int idx = find_script_index(script_id); if(idx<0) return -1; if(out_lines_viewed) *out_lines_viewed = g_lines_viewed[idx]; if(out_last_view_ms) *out_last_view_ms = g_last_view_time_ms[idx]; if(out_digest) *out_digest = g_digest_accum; return 0; }
