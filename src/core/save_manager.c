#include "core/save_manager.h"
#include "core/persistence.h"
#include "core/app_state.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static RogueSaveComponent g_components[ROGUE_SAVE_MAX_COMPONENTS];
static int g_component_count = 0;
static int g_initialized = 0;

/* Simple CRC32 (polynomial 0xEDB88320) */
uint32_t rogue_crc32(const void* data, size_t len){
    static uint32_t table[256]; static int have=0; if(!have){ for(uint32_t i=0;i<256;i++){ uint32_t c=i; for(int k=0;k<8;k++) c = (c & 1)? 0xEDB88320u ^ (c>>1) : (c>>1); table[i]=c; } have=1; }
    uint32_t crc=0xFFFFFFFFu; const unsigned char* p=(const unsigned char*)data; for(size_t i=0;i<len;i++) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc>>8); return crc ^ 0xFFFFFFFFu;
}

static const RogueSaveComponent* find_component(int id){ for(int i=0;i<g_component_count;i++) if(g_components[i].id==id) return &g_components[i]; return NULL; }

void rogue_save_manager_register(const RogueSaveComponent* comp){ if(g_component_count>=ROGUE_SAVE_MAX_COMPONENTS) return; if(find_component(comp->id)) return; g_components[g_component_count++] = *comp; }

static int cmp_comp(const void* a, const void* b){ const RogueSaveComponent*ca=a; const RogueSaveComponent*cb=b; return (ca->id - cb->id); }

void rogue_save_manager_init(void){ if(g_initialized) return; g_initialized=1; /* ensure deterministic order */ }

static char slot_path[128];
static const char* build_slot_path(int slot){ snprintf(slot_path,sizeof slot_path,"save_slot_%d.sav", slot); return slot_path; }

int rogue_save_manager_save_slot(int slot_index){
    if(slot_index<0 || slot_index>=ROGUE_SAVE_SLOT_COUNT) return -1;
    qsort(g_components, g_component_count, sizeof(RogueSaveComponent), cmp_comp);
    /* build temp file path */
    char tmp_path[160]; snprintf(tmp_path,sizeof tmp_path,"save_slot_%d.tmp", slot_index);
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f,tmp_path,"wb");
#else
    f=fopen(tmp_path,"wb");
#endif
    if(!f) return -2;
    RogueSaveDescriptor desc={0}; desc.version=1; desc.timestamp_unix=(uint32_t)time(NULL); desc.section_count=0; desc.component_mask=0; desc.total_size=0; desc.checksum=0;
    /* reserve header space */
    if(fwrite(&desc,sizeof desc,1,f)!=1){ fclose(f); return -3; }
    for(int i=0;i<g_component_count;i++){
        const RogueSaveComponent* c=&g_components[i]; long start=ftell(f);
        uint32_t id=(uint32_t)c->id; uint32_t size_placeholder=0; if(fwrite(&id,sizeof id,1,f)!=1){ fclose(f); return -4; } if(fwrite(&size_placeholder,sizeof size_placeholder,1,f)!=1){ fclose(f); return -4; }
        long payload_start=ftell(f);
        if(c->write_fn(f)!=0){ fclose(f); return -5; }
        long end=ftell(f); uint32_t section_size=(uint32_t)(end - payload_start);
        fseek(f,start+sizeof(uint32_t),SEEK_SET); fwrite(&section_size,sizeof section_size,1,f); fseek(f,end,SEEK_SET);
        desc.section_count++; desc.component_mask |= (1u<<c->id);
    }
    long file_end=ftell(f);
    desc.total_size = (uint64_t)file_end;
    /* compute checksum over bytes after header */
    fflush(f); fseek(f,sizeof desc,SEEK_SET); size_t rest = file_end - (long)sizeof desc; unsigned char* buf=(unsigned char*)malloc(rest); if(buf){ fread(buf,1,rest,f); desc.checksum = rogue_crc32(buf,rest); free(buf);} fseek(f,0,SEEK_SET); fwrite(&desc,sizeof desc,1,f); fclose(f);
    /* atomic rename */
#if defined(_MSC_VER)
    remove(build_slot_path(slot_index)); rename(tmp_path, build_slot_path(slot_index));
#else
    rename(tmp_path, build_slot_path(slot_index));
#endif
    return 0;
}

int rogue_save_manager_load_slot(int slot_index){
    if(slot_index<0 || slot_index>=ROGUE_SAVE_SLOT_COUNT) return -1; FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, build_slot_path(slot_index), "rb");
#else
    f=fopen(build_slot_path(slot_index),"rb");
#endif
    if(!f) return -2; RogueSaveDescriptor desc; if(fread(&desc,sizeof desc,1,f)!=1){ fclose(f); return -3; }
    /* naive validation */
    if(desc.version!=1){ fclose(f); return -4; }
    /* checksum */
    long file_end=0; fseek(f,0,SEEK_END); file_end=ftell(f); if((uint64_t)file_end!=desc.total_size){ fclose(f); return -5; } size_t rest=file_end - (long)sizeof desc; fseek(f,sizeof desc,SEEK_SET); unsigned char* buf=(unsigned char*)malloc(rest); if(!buf){ fclose(f); return -6; } fread(buf,1,rest,f); uint32_t crc=rogue_crc32(buf,rest); free(buf); if(crc!=desc.checksum){ fclose(f); return -7; }
    for(uint32_t s=0;s<desc.section_count;s++){
        uint32_t id=0; uint32_t size=0; if(fread(&id,sizeof id,1,f)!=1){ fclose(f); return -8; } if(fread(&size,sizeof size,1,f)!=1){ fclose(f); return -8; }
        const RogueSaveComponent* comp=find_component((int)id); long payload_pos=ftell(f); if(comp && comp->read_fn){ if(comp->read_fn(f,size)!=0){ fclose(f); return -9; } } fseek(f,payload_pos+size,SEEK_SET);
    }
    fclose(f); return 0;
}

/* Basic player + world meta adapters bridging existing ad-hoc text save for Phase1 demonstration */
static int write_player_component(FILE* f){
    /* reuse existing function into memory buffer (simpler approach later replaced by structured binary) */
    char path_backup[260]; strncpy(path_backup, rogue__player_stats_path(), sizeof path_backup); /* assume path fits */
    /* Temporarily redirect path to temp memory file not feasible in C easily; instead serialize subset binary */
    fwrite(&g_app.player.level,sizeof g_app.player.level,1,f);
    fwrite(&g_app.player.xp,sizeof g_app.player.xp,1,f);
    fwrite(&g_app.player.health,sizeof g_app.player.health,1,f);
    fwrite(&g_app.talent_points,sizeof g_app.talent_points,1,f);
    return 0;
}
static int read_player_component(FILE* f, size_t size){
    if(size < sizeof(int)*4) return -1; fread(&g_app.player.level,sizeof g_app.player.level,1,f); fread(&g_app.player.xp,sizeof g_app.player.xp,1,f); fread(&g_app.player.health,sizeof g_app.player.health,1,f); fread(&g_app.talent_points,sizeof g_app.talent_points,1,f); return 0; }

static RogueSaveComponent PLAYER_COMP={ ROGUE_SAVE_COMP_PLAYER, write_player_component, read_player_component, "player" };

void rogue_register_core_save_components(void){
    rogue_save_manager_register(&PLAYER_COMP);
}
