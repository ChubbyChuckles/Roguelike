#include "core/vendor/vendor_inventory_templates.h"
#include "core/path_utils.h"
#include "util/determinism.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static RogueVendorInventoryTemplate g_templates[ROGUE_MAX_VENDOR_INV_TEMPLATES];
static int g_template_count = 0;

static void copy_str_local(char* dst, size_t cap, const char* src){ if(cap==0) return; if(!src){ dst[0]='\0'; return;} size_t i=0; for(; i<cap-1 && src[i]; ++i) dst[i]=src[i]; dst[i]='\0'; }

/* Lightweight JSON helpers (duplicated minimal subset to keep modules decoupled). */
static int read_entire_file_local(const char* path, char** out_buf){
    FILE* f=NULL;
#ifdef _MSC_VER
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) return 0; fseek(f,0,SEEK_END); long sz=ftell(f); if(sz<0){ fclose(f); return 0; }
    fseek(f,0,SEEK_SET); char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); return 0; }
    size_t n=fread(buf,1,(size_t)sz,f); fclose(f); buf[n]='\0'; *out_buf=buf; return 1; }

static int json_find_string_local(const char* obj, const char* key, char* out, size_t out_sz){ if(!obj||!key||!out||out_sz==0) return 0; out[0]='\0'; const char* p=obj; size_t klen=strlen(key); for(;;){ const char* found=strstr(p,key); if(!found) break; p=found; const char* q=p+klen; while(*q && isspace((unsigned char)*q)) q++; if(*q!=':'){ p=p+1; continue; } q++; while(*q && isspace((unsigned char)*q)) q++; if(*q!='"'){ p=p+1; continue; } q++; size_t i=0; while(*q && *q!='"' && i<out_sz-1){ out[i++]=*q++; } out[i]='\0'; return i>0; } return 0; }
static int json_find_int_array(const char* obj, const char* key, int* out, int expected, int fill){ for(int i=0;i<expected;i++) out[i]=fill; if(!obj||!key) return 0; const char* p=strstr(obj,key); if(!p) return 0; p+=strlen(key); while(*p && *p!='[') p++; if(*p!='[') return 0; p++; int idx=0; while(*p && *p!=']' && idx<expected){ while(*p && (isspace((unsigned char)*p) || *p==',')) p++; int neg=0; if(*p=='-'){ neg=1; p++; } int val=0; int digits=0; while(*p && isdigit((unsigned char)*p)){ val = val*10 + (*p - '0'); p++; digits++; } if(digits>0){ out[idx++] = neg? -val : val; } while(*p && *p!=',' && *p!=']') p++; }
    return 1; }

static const char* find_next_object(const char* buf){ const char* c=strchr(buf,'{'); return c; }
static const char* find_end_object(const char* start){ if(!start) return NULL; const char* p=start+1; int depth=1; while(*p){ if(*p=='{') depth++; else if(*p=='}'){ depth--; if(depth==0) return p; } p++; } return NULL; }

int rogue_vendor_inventory_templates_load(void){
    g_template_count=0;
    char path[256]; if(!rogue_find_asset_path("vendors/inventory_templates.json", path, sizeof path)) return 0;
    char* buf=NULL; if(!read_entire_file_local(path,&buf)) return 0;
    const char* section = strstr(buf,"\"inventory_templates\""); if(!section){ free(buf); return 0; }
    const char* p=section; int added=0;
    while(g_template_count < ROGUE_MAX_VENDOR_INV_TEMPLATES){
        const char* obj=find_next_object(p); if(!obj) break; const char* end=find_end_object(obj); if(!end) break; size_t obj_len=(size_t)(end-obj+1); if(obj_len>1023) obj_len=1023; char tmp[1024]; memcpy(tmp,obj,obj_len); tmp[obj_len]='\0';
        char arche[32]; if(!json_find_string_local(tmp,"\"archetype\"", arche, sizeof arche)){ p=end+1; continue; }
        RogueVendorInventoryTemplate* t=&g_templates[g_template_count]; memset(t,0,sizeof *t); copy_str_local(t->archetype,sizeof t->archetype, arche);
        json_find_int_array(tmp,"\"category_weights\"", t->category_weights, ROGUE_ITEM__COUNT, 0);
        json_find_int_array(tmp,"\"rarity_weights\"", t->rarity_weights, 5, 0);
        g_template_count++; added++; p=end+1; }
    free(buf); return added>0; }

int rogue_vendor_inventory_template_count(void){ return g_template_count; }
const RogueVendorInventoryTemplate* rogue_vendor_inventory_template_at(int idx){ if(idx<0||idx>=g_template_count) return NULL; return &g_templates[idx]; }
const RogueVendorInventoryTemplate* rogue_vendor_inventory_template_find(const char* archetype){ if(!archetype) return NULL; for(int i=0;i<g_template_count;i++) if(strcmp(g_templates[i].archetype,archetype)==0) return &g_templates[i]; return NULL; }

unsigned int rogue_vendor_inventory_seed(unsigned int world_seed, const char* vendor_id, int day_cycle){
    if(day_cycle < 0) day_cycle = 0; unsigned long long h = 0xcbf29ce484222325ULL; /* FNV offset */
    if(vendor_id){ h = rogue_fnv1a64(vendor_id, strlen(vendor_id), h); }
    h ^= (unsigned long long)(unsigned int)day_cycle;
    unsigned int folded = (unsigned int)(h ^ (h>>32));
    return world_seed ^ folded;
}
