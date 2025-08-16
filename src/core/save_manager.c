#include "core/save_manager.h"
#include "core/persistence.h"
#include "core/app_state.h"
#include "core/skills.h"
#include "core/buffs.h"
#include "core/loot_instances.h"
#include "core/vendor.h"
#include "world/tilemap.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h> /* qsort, malloc */
#include <assert.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

static RogueSaveComponent g_components[ROGUE_SAVE_MAX_COMPONENTS];
static int g_component_count = 0;
static int g_initialized = 0;
static int g_migrations_registered = 0; /* ensure migrations registered once */

/* Migration registry (linear chain for now) */
static RogueSaveMigration g_migrations[16];
static int g_migration_count = 0;
static int g_durable_writes = 0; /* fsync/_commit toggle */
static int g_last_migration_steps = 0; /* metrics */
static int g_last_migration_failed = 0;
static double g_last_migration_ms = 0.0; /* milliseconds */
static int g_debug_json_dump = 0; /* Phase 3.2 debug export toggle */

int rogue_save_last_migration_steps(void){ return g_last_migration_steps; }
int rogue_save_last_migration_failed(void){ return g_last_migration_failed; }
double rogue_save_last_migration_ms(void){ return g_last_migration_ms; }

/* Simple CRC32 (polynomial 0xEDB88320) */
uint32_t rogue_crc32(const void* data, size_t len){
    static uint32_t table[256]; static int have=0; if(!have){ for(uint32_t i=0;i<256;i++){ uint32_t c=i; for(int k=0;k<8;k++) c = (c & 1)? 0xEDB88320u ^ (c>>1) : (c>>1); table[i]=c; } have=1; }
    uint32_t crc=0xFFFFFFFFu; const unsigned char* p=(const unsigned char*)data; for(size_t i=0;i<len;i++) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc>>8); return crc ^ 0xFFFFFFFFu;
}

static const RogueSaveComponent* find_component(int id){ for(int i=0;i<g_component_count;i++) if(g_components[i].id==id) return &g_components[i]; return NULL; }

void rogue_save_manager_register(const RogueSaveComponent* comp){ if(g_component_count>=ROGUE_SAVE_MAX_COMPONENTS) return; if(find_component(comp->id)) return; g_components[g_component_count++] = *comp; }

static int cmp_comp(const void* a, const void* b){ const RogueSaveComponent*ca=a; const RogueSaveComponent*cb=b; return (ca->id - cb->id); }

/* Core migration registration (defined later) */
static void rogue_register_core_migrations(void);
void rogue_save_manager_init(void){
    if(!g_initialized){
        g_initialized=1; /* ensure deterministic order */
    }
    if(!g_migrations_registered){
        rogue_register_core_migrations();
        g_migrations_registered=1;
    }
}

void rogue_save_register_migration(const RogueSaveMigration* mig){ if(g_migration_count< (int)(sizeof g_migrations/sizeof g_migrations[0])) g_migrations[g_migration_count++]=*mig; }

void rogue_save_manager_reset_for_tests(void){ g_component_count=0; g_initialized=0; g_migration_count=0; g_migrations_registered=0; g_last_migration_steps=0; g_last_migration_failed=0; g_last_migration_ms=0.0; }
int rogue_save_set_debug_json(int enabled){ g_debug_json_dump = enabled?1:0; return 0; }

static char slot_path[128];
static const char* build_slot_path(int slot){ snprintf(slot_path,sizeof slot_path,"save_slot_%d.sav", slot); return slot_path; }
static char autosave_path[128];
static const char* build_autosave_path(int logical){ int ring = logical % ROGUE_AUTOSAVE_RING; snprintf(autosave_path,sizeof autosave_path,"autosave_%d.sav", ring); return autosave_path; }

int rogue_save_format_endianness_is_le(void){ uint32_t x=0x01020304u; unsigned char* p=(unsigned char*)&x; return p[0]==0x04; }

static int internal_save_to(const char* final_path){
    qsort(g_components, g_component_count, sizeof(RogueSaveComponent), cmp_comp);
    char tmp_path[160]; snprintf(tmp_path,sizeof tmp_path,"./tmp_save_%u.tmp", (unsigned)time(NULL));
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f,tmp_path,"w+b");
#else
    f=fopen(tmp_path,"w+b");
#endif
    if(!f) return -2;
    RogueSaveDescriptor desc={0}; desc.version=ROGUE_SAVE_FORMAT_VERSION; desc.timestamp_unix=(uint32_t)time(NULL);
    if(fwrite(&desc,sizeof desc,1,f)!=1){ fclose(f); return -3; }
    desc.section_count=0; desc.component_mask=0;
    for(int i=0;i<g_component_count;i++){
        const RogueSaveComponent* c=&g_components[i]; long start=ftell(f);
    if(desc.version>=3){
            uint16_t id16=(uint16_t)c->id; uint32_t size_placeholder32=0;
            if(fwrite(&id16,sizeof id16,1,f)!=1){ fclose(f); return -4; }
            if(fwrite(&size_placeholder32,sizeof size_placeholder32,1,f)!=1){ fclose(f); return -4; }
            long payload_start=ftell(f);
            if(c->write_fn(f)!=0){ fclose(f); return -5; }
            long end=ftell(f); uint32_t section_size=(uint32_t)(end - payload_start);
            fseek(f,start+sizeof(uint16_t),SEEK_SET); fwrite(&section_size,sizeof section_size,1,f); fseek(f,end,SEEK_SET);
        } else {
            uint32_t id=(uint32_t)c->id, size_placeholder=0; if(fwrite(&id,sizeof id,1,f)!=1){ fclose(f); return -4; } if(fwrite(&size_placeholder,sizeof size_placeholder,1,f)!=1){ fclose(f); return -4; }
            long payload_start=ftell(f); if(c->write_fn(f)!=0){ fclose(f); return -5; }
            long end=ftell(f); uint32_t section_size=(uint32_t)(end - payload_start);
            fseek(f,start+sizeof(uint32_t),SEEK_SET); fwrite(&section_size,sizeof section_size,1,f); fseek(f,end,SEEK_SET);
        }
        desc.section_count++; desc.component_mask |= (1u<<c->id);
    }
    long file_end=ftell(f); desc.total_size=(uint64_t)file_end; fflush(f);
    size_t rest = file_end - (long)sizeof desc; fseek(f,sizeof desc,SEEK_SET);
    unsigned char chunk[512]; uint32_t crc=0xFFFFFFFFu; static uint32_t table[256]; static int have=0; if(!have){ for(uint32_t i=0;i<256;i++){ uint32_t c=i; for(int k=0;k<8;k++) c = (c & 1)? 0xEDB88320u ^ (c>>1) : (c>>1); table[i]=c; } have=1; }
    size_t remaining=rest; while(remaining>0){ size_t to_read= remaining>sizeof(chunk)?sizeof(chunk):remaining; size_t n=fread(chunk,1,to_read,f); if(n!=to_read){ fclose(f); return -13; } for(size_t i=0;i<n;i++){ crc = table[(crc ^ chunk[i]) & 0xFF] ^ (crc>>8); } remaining-=n; }
    desc.checksum = crc ^ 0xFFFFFFFFu; fseek(f,0,SEEK_SET); fwrite(&desc,sizeof desc,1,f); fflush(f);
#if defined(_WIN32)
    if(g_durable_writes){ int fd=_fileno(f); if(fd!=-1) _commit(fd); }
#else
    if(g_durable_writes){ int fd=fileno(f); if(fd!=-1) fsync(fd); }
#endif
    fclose(f);
#if defined(_MSC_VER)
    remove(final_path); rename(tmp_path, final_path);
#else
    rename(tmp_path, final_path);
#endif
    return 0;
}

int rogue_save_manager_save_slot(int slot_index){ if(slot_index<0 || slot_index>=ROGUE_SAVE_SLOT_COUNT) return -1; int rc=internal_save_to(build_slot_path(slot_index)); if(rc==0 && g_debug_json_dump){ char json_path[128]; snprintf(json_path,sizeof json_path,"save_slot_%d.json", slot_index); char buf[2048]; if(rogue_save_export_json(slot_index,buf,sizeof buf)==0){ FILE* jf=NULL; 
#if defined(_MSC_VER)
    fopen_s(&jf,json_path,"wb");
#else
    jf=fopen(json_path,"wb");
#endif
    if(jf){ fwrite(buf,1,strlen(buf),jf); fclose(jf);} }
    } return rc; }
int rogue_save_manager_autosave(int slot_index){ if(slot_index<0) slot_index=0; return internal_save_to(build_autosave_path(slot_index)); }
int rogue_save_manager_set_durable(int enabled){ g_durable_writes = enabled?1:0; return 0; }

/* Internal helper: validate & load entire save file (returns malloc buffer after header) */
static int load_and_validate(const char* path, RogueSaveDescriptor* out_desc, unsigned char** out_buf){
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) return -2; if(fread(out_desc,sizeof *out_desc,1,f)!=1){ fclose(f); return -3; }
    long file_end=0; fseek(f,0,SEEK_END); file_end=ftell(f); if((uint64_t)file_end!=out_desc->total_size){ fclose(f); return -5; }
    size_t rest=file_end - (long)sizeof *out_desc; fseek(f,sizeof *out_desc,SEEK_SET); unsigned char* buf=(unsigned char*)malloc(rest); if(!buf){ fclose(f); return -6; }
    size_t n=fread(buf,1,rest,f); if(n!=rest){ free(buf); fclose(f); return -6; }
    uint32_t crc=rogue_crc32(buf,rest); if(crc!=out_desc->checksum){ free(buf); fclose(f); return -7; }
    fclose(f); *out_buf=buf; return 0;
}

int rogue_save_for_each_section(int slot_index, RogueSaveSectionIterFn fn, void* user){
    if(slot_index<0||slot_index>=ROGUE_SAVE_SLOT_COUNT) return -1;
    RogueSaveDescriptor desc; unsigned char* buf=NULL;
    int rc=load_and_validate(build_slot_path(slot_index), &desc, &buf); if(rc!=0) return rc;
    unsigned char* p=buf; size_t total_payload = (size_t)(desc.total_size - sizeof desc);
    for(uint32_t s=0; s<desc.section_count; s++){
        if(desc.version>=3){
            if((size_t)(p-buf)+6 > total_payload){ free(buf); return -8; }
            uint16_t id16; memcpy(&id16,p,2); uint32_t size; memcpy(&size,p+2,4); p+=6;
            if((size_t)(p-buf)+size > total_payload){ free(buf); return -8; }
            if(fn){ int frc=fn(&desc,(uint32_t)id16,p,size,user); if(frc!=0){ free(buf); return frc; } }
            p+=size;
        } else {
            if((size_t)(p-buf)+8 > total_payload){ free(buf); return -8; }
            uint32_t id; memcpy(&id,p,4); uint32_t size; memcpy(&size,p+4,4); p+=8;
            if((size_t)(p-buf)+size > total_payload){ free(buf); return -8; }
            if(fn){ int frc=fn(&desc,id,p,size,user); if(frc!=0){ free(buf); return frc; } }
            p+=size;
        }
    }
    free(buf);
    return 0;
}

int rogue_save_export_json(int slot_index, char* out, size_t out_cap){
    if(!out||out_cap==0) return -1;
    RogueSaveDescriptor d; unsigned char* buf=NULL; int rc=load_and_validate(build_slot_path(slot_index), &d, &buf); if(rc!=0){ return rc; }
    int written=snprintf(out,out_cap,"{\n  \"version\":%u,\n  \"timestamp\":%u,\n  \"sections\":[", d.version,d.timestamp_unix);
    unsigned char* p=buf; size_t total_payload=(size_t)(d.total_size - sizeof d);
    for(uint32_t s=0; s<d.section_count && written<(int)out_cap; s++){
        uint32_t id=0; uint32_t size=0;
        if(d.version>=3){ if((size_t)(p-buf)+6>total_payload) break; uint16_t id16; memcpy(&id16,p,2); memcpy(&size,p+2,4); id=(uint32_t)id16; p+=6; }
        else { if((size_t)(p-buf)+8>total_payload) break; memcpy(&id,p,4); memcpy(&size,p+4,4); p+=8; }
    if((size_t)(p-buf)+size>total_payload) break; p+=size;
    written += snprintf(out+written, out_cap-written, "%s{\"id\":%u,\"size\":%u}", (s==0?"":","), id,size);
    }
    if(written<(int)out_cap) written += snprintf(out+written, out_cap-written, "]\n}\n");
    free(buf);
    if(written>=(int)out_cap) return -2; return 0;
}

int rogue_save_reload_component_from_slot(int slot_index, int component_id){
    if(slot_index<0||slot_index>=ROGUE_SAVE_SLOT_COUNT) return -1;
    const RogueSaveComponent* comp=find_component(component_id);
    if(!comp||!comp->read_fn) return -2;
    RogueSaveDescriptor d; unsigned char* buf=NULL; int rc=load_and_validate(build_slot_path(slot_index), &d, &buf); if(rc!=0) return rc;
    unsigned char* p=buf; size_t total_payload=(size_t)(d.total_size - sizeof d); int applied=-3;
    for(uint32_t s=0; s<d.section_count; s++){
        uint32_t id=0; uint32_t size=0;
        if(d.version>=3){ if((size_t)(p-buf)+6>total_payload) break; uint16_t id16; memcpy(&id16,p,2); memcpy(&size,p+2,4); id=(uint32_t)id16; p+=6; }
        else { if((size_t)(p-buf)+8>total_payload) break; memcpy(&id,p,4); memcpy(&size,p+4,4); p+=8; }
        if((size_t)(p-buf)+size>total_payload) break;
        if((int)id==component_id){
            const unsigned char* payload=p; char tmp[64]; snprintf(tmp,sizeof tmp,"_tmp_section_%d.bin", component_id);
            FILE* tf=NULL;
#if defined(_MSC_VER)
            fopen_s(&tf,tmp,"wb");
#else
            tf=fopen(tmp,"wb");
#endif
            if(!tf){ free(buf); return -4; }
            fwrite(payload,1,size,tf); fclose(tf);
#if defined(_MSC_VER)
            fopen_s(&tf,tmp,"rb");
#else
            tf=fopen(tmp,"rb");
#endif
            if(!tf){ free(buf); return -4; }
            comp->read_fn(tf,size); fclose(tf); remove(tmp); applied=0; break; }
        p+=size;
    }
    free(buf);
    return applied;
}

int rogue_save_manager_load_slot(int slot_index){
    if(slot_index<0 || slot_index>=ROGUE_SAVE_SLOT_COUNT) return -1; FILE* f=NULL;
#ifdef ROGUE_STRICT_ENDIAN
    if(!rogue_save_format_endianness_is_le()){ return -30; }
#endif
#if defined(_MSC_VER)
    fopen_s(&f, build_slot_path(slot_index), "rb");
#else
    f=fopen(build_slot_path(slot_index),"rb");
#endif
    if(!f){ return -2; }
    RogueSaveDescriptor desc; if(fread(&desc,sizeof desc,1,f)!=1){ fclose(f); return -3; }
    /* naive validation */
    if(desc.version!=ROGUE_SAVE_FORMAT_VERSION){
        /* Load entire payload for potential in-place migration with rollback */
        long file_end_pos=0; fseek(f,0,SEEK_END); file_end_pos=ftell(f); long payload_size = file_end_pos - (long)sizeof desc; if(payload_size<0){ fclose(f); return -4; }
        unsigned char* payload=(unsigned char*)malloc((size_t)payload_size); if(!payload){ fclose(f); return -4; }
        fseek(f,sizeof desc,SEEK_SET); if(fread(payload,1,(size_t)payload_size,f)!=(size_t)payload_size){ free(payload); fclose(f); return -4; }
        unsigned char* rollback=(unsigned char*)malloc((size_t)payload_size); if(!rollback){ free(payload); fclose(f); return -4; }
        memcpy(rollback,payload,(size_t)payload_size);
        g_last_migration_steps=0; g_last_migration_failed=0; g_last_migration_ms=0.0; double t0=(double)clock();
        uint32_t cur = desc.version; int failure=0;
        while(cur < ROGUE_SAVE_FORMAT_VERSION){
            int advanced=0; for(int i=0;i<g_migration_count;i++) if(g_migrations[i].from_version==cur && g_migrations[i].to_version==cur+1){
                if(g_migrations[i].apply_fn){ int mrc=g_migrations[i].apply_fn(payload,(size_t)payload_size); if(mrc!=0){ failure=1; break; } }
                cur=g_migrations[i].to_version; advanced=1; g_last_migration_steps++; break; }
            if(failure) break; if(!advanced){ failure=1; break; }
        }
        g_last_migration_ms = ((double)clock() - t0) * 1000.0 / (double)CLOCKS_PER_SEC;
        if(failure || cur!=ROGUE_SAVE_FORMAT_VERSION){ memcpy(payload,rollback,(size_t)payload_size); g_last_migration_failed=1; free(payload); free(rollback); fclose(f); return failure? ROGUE_SAVE_ERR_MIGRATION_FAIL : ROGUE_SAVE_ERR_MIGRATION_CHAIN; }
        free(rollback);
        /* For now we don't persist upgraded payload back; future phase may write-through */
        /* Reset file pointer to just after header for section iteration; we still rely on on-disk layout (no structural changes yet) */
        fseek(f, sizeof desc, SEEK_SET);
        free(payload);
    }
    /* checksum */
    long file_end=0; fseek(f,0,SEEK_END); file_end=ftell(f); if((uint64_t)file_end!=desc.total_size){ fclose(f); return -5; } size_t rest=file_end - (long)sizeof desc; fseek(f,sizeof desc,SEEK_SET); unsigned char* buf=(unsigned char*)malloc(rest); if(!buf){ fclose(f); return -6; } size_t n=fread(buf,1,rest,f); if(n!=rest){ free(buf); fclose(f); return -6; } uint32_t crc=rogue_crc32(buf,rest); if(crc!=desc.checksum){ free(buf); fclose(f); return -7; } free(buf); fseek(f, sizeof desc, SEEK_SET);
    if(desc.version>=3){
        for(uint32_t s=0;s<desc.section_count;s++){
            uint16_t id16=0; uint32_t size=0; if(fread(&id16,sizeof id16,1,f)!=1){ fclose(f); return -8; } if(fread(&size,sizeof size,1,f)!=1){ fclose(f); return -8; }
            uint32_t id = (uint32_t)id16;
            const RogueSaveComponent* comp=find_component((int)id); long payload_pos=ftell(f); if(comp && comp->read_fn){ if(comp->read_fn(f,size)!=0){ fclose(f); return -9; } } fseek(f,payload_pos+size,SEEK_SET);
        }
    } else {
        for(uint32_t s=0;s<desc.section_count;s++){
            uint32_t id=0; uint32_t size=0; if(fread(&id,sizeof id,1,f)!=1){ fclose(f); return -8; } if(fread(&size,sizeof size,1,f)!=1){ fclose(f); return -8; }
            const RogueSaveComponent* comp=find_component((int)id); long payload_pos=ftell(f); if(comp && comp->read_fn){ if(comp->read_fn(f,size)!=0){ fclose(f); return -9; } } fseek(f,payload_pos+size,SEEK_SET);
        }
    }
    fclose(f); return 0;
}

/* Basic player + world meta adapters bridging existing ad-hoc text save for Phase1 demonstration */
static int write_player_component(FILE* f){
    /* Serialize subset of player progression (minimal Phase 1 binary form) */
    fwrite(&g_app.player.level,sizeof g_app.player.level,1,f);
    fwrite(&g_app.player.xp,sizeof g_app.player.xp,1,f);
    fwrite(&g_app.player.health,sizeof g_app.player.health,1,f);
    fwrite(&g_app.talent_points,sizeof g_app.talent_points,1,f);
    return 0;
}
static int read_player_component(FILE* f, size_t size){
    if(size < sizeof(int)*4) return -1; fread(&g_app.player.level,sizeof g_app.player.level,1,f); fread(&g_app.player.xp,sizeof g_app.player.xp,1,f); fread(&g_app.player.health,sizeof g_app.player.health,1,f); fread(&g_app.talent_points,sizeof g_app.talent_points,1,f); return 0; }

/* ---------------- Additional Phase 1 Components ---------------- */

/* INVENTORY: serialize active item instances (count + each record) */
static int write_inventory_component(FILE* f){
    int count = 0; if(g_app.item_instances){
        for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances[i].active) count++;
    }
    fwrite(&count, sizeof count,1,f);
    if(count==0) return 0;
    for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances[i].active){
        RogueItemInstance* it=&g_app.item_instances[i];
        fwrite(&it->def_index,sizeof it->def_index,1,f);
        fwrite(&it->quantity,sizeof it->quantity,1,f);
        fwrite(&it->rarity,sizeof it->rarity,1,f);
        fwrite(&it->prefix_index,sizeof it->prefix_index,1,f);
        fwrite(&it->prefix_value,sizeof it->prefix_value,1,f);
        fwrite(&it->suffix_index,sizeof it->suffix_index,1,f);
        fwrite(&it->suffix_value,sizeof it->suffix_value,1,f);
    }
    return 0;
}
static int read_inventory_component(FILE* f, size_t size){
    (void)size; int count=0; if(fread(&count,sizeof count,1,f)!=1) return -1; for(int i=0;i<count;i++){
        int def_index,quantity,rarity,pidx,pval,sidx,sval; if(fread(&def_index,sizeof def_index,1,f)!=1) return -1;
        fread(&quantity,sizeof quantity,1,f); fread(&rarity,sizeof rarity,1,f); fread(&pidx,sizeof pidx,1,f);
        fread(&pval,sizeof pval,1,f); fread(&sidx,sizeof sidx,1,f); fread(&sval,sizeof sval,1,f);
        int inst = rogue_items_spawn(def_index, quantity, 0.0f,0.0f); if(inst>=0){ rogue_item_instance_apply_affixes(inst, rarity, pidx,pval,sidx,sval); }
    } return 0; }

/* SKILLS: ranks + cooldown state (id ordered) */
static int write_skills_component(FILE* f){
    fwrite(&g_app.skill_count,sizeof g_app.skill_count,1,f);
    for(int i=0;i<g_app.skill_count;i++){
        const RogueSkillState* st = rogue_skill_get_state(i); int rank = st?st->rank:0; fwrite(&rank,sizeof rank,1,f);
        double cd= st? st->cooldown_end_ms:0; fwrite(&cd,sizeof cd,1,f);
    }
    return 0;
}
static int read_skills_component(FILE* f, size_t size){ (void)size; int count=0; if(fread(&count,sizeof count,1,f)!=1) return -1; int limit = (count<g_app.skill_count)?count:g_app.skill_count; for(int i=0;i<limit;i++){ int rank; double cd; fread(&rank,sizeof rank,1,f); fread(&cd,sizeof cd,1,f); const RogueSkillDef* d=rogue_skill_get_def(i); struct RogueSkillState* st=(struct RogueSkillState*)rogue_skill_get_state(i); if(d && st){ if(rank>d->max_rank) rank=d->max_rank; st->rank=rank; st->cooldown_end_ms=cd; } } /* skip any extra */ if(count>limit){ size_t skip = (size_t)(count-limit)*(sizeof(int)+sizeof(double)); fseek(f,(long)skip,SEEK_CUR); } return 0; }

/* BUFFS: active buffs list */
extern RogueBuff g_buffs_internal[]; extern int g_buff_count_internal; /* assume symbols provided elsewhere */
static int write_buffs_component(FILE* f){
    int active_count=0; for(int i=0;i<g_buff_count_internal;i++) if(g_buffs_internal[i].active) active_count++;
    fwrite(&active_count,sizeof active_count,1,f);
    for(int i=0;i<g_buff_count_internal;i++) if(g_buffs_internal[i].active){ fwrite(&g_buffs_internal[i], sizeof(RogueBuff),1,f);} return 0; }
static int read_buffs_component(FILE* f, size_t size){ (void)size; int count=0; if(fread(&count,sizeof count,1,f)!=1) return -1; for(int i=0;i<count;i++){ RogueBuff b; if(fread(&b,sizeof b,1,f)!=1) return -1; rogue_buffs_apply(b.type,b.magnitude,b.end_ms,0.0); } return 0; }

/* VENDOR: seed + restock timers */
static int write_vendor_component(FILE* f){ fwrite(&g_app.vendor_seed,sizeof g_app.vendor_seed,1,f); fwrite(&g_app.vendor_time_accum_ms,sizeof g_app.vendor_time_accum_ms,1,f); fwrite(&g_app.vendor_restock_interval_ms,sizeof g_app.vendor_restock_interval_ms,1,f); return 0; }
static int read_vendor_component(FILE* f, size_t size){ (void)size; fread(&g_app.vendor_seed,sizeof g_app.vendor_seed,1,f); fread(&g_app.vendor_time_accum_ms,sizeof g_app.vendor_time_accum_ms,1,f); fread(&g_app.vendor_restock_interval_ms,sizeof g_app.vendor_restock_interval_ms,1,f); return 0; }

/* WORLD META: world seed + generation params subset */
static int write_world_meta_component(FILE* f){ fwrite(&g_app.pending_seed,sizeof g_app.pending_seed,1,f); fwrite(&g_app.gen_water_level,sizeof g_app.gen_water_level,1,f); fwrite(&g_app.gen_cave_thresh,sizeof g_app.gen_cave_thresh,1,f); return 0; }
static int read_world_meta_component(FILE* f, size_t size){ (void)size; fread(&g_app.pending_seed,sizeof g_app.pending_seed,1,f); fread(&g_app.gen_water_level,sizeof g_app.gen_water_level,1,f); fread(&g_app.gen_cave_thresh,sizeof g_app.gen_cave_thresh,1,f); return 0; }

static RogueSaveComponent PLAYER_COMP={ ROGUE_SAVE_COMP_PLAYER, write_player_component, read_player_component, "player" };
static RogueSaveComponent INVENTORY_COMP={ ROGUE_SAVE_COMP_INVENTORY, write_inventory_component, read_inventory_component, "inventory" };
static RogueSaveComponent SKILLS_COMP={ ROGUE_SAVE_COMP_SKILLS, write_skills_component, read_skills_component, "skills" };
static RogueSaveComponent BUFFS_COMP={ ROGUE_SAVE_COMP_BUFFS, write_buffs_component, read_buffs_component, "buffs" };
static RogueSaveComponent VENDOR_COMP={ ROGUE_SAVE_COMP_VENDOR, write_vendor_component, read_vendor_component, "vendor" };
static RogueSaveComponent WORLD_META_COMP={ ROGUE_SAVE_COMP_WORLD_META, write_world_meta_component, read_world_meta_component, "world_meta" };

void rogue_register_core_save_components(void){
    rogue_save_manager_register(&PLAYER_COMP);
    rogue_save_manager_register(&WORLD_META_COMP);
    rogue_save_manager_register(&INVENTORY_COMP);
    rogue_save_manager_register(&SKILLS_COMP);
    rogue_save_manager_register(&BUFFS_COMP);
    rogue_save_manager_register(&VENDOR_COMP);
}

/* Migration definitions */
static int migrate_v2_to_v3(unsigned char* data, size_t size){ (void)data; (void)size; return 0; }
static RogueSaveMigration MIG_V2_TO_V3 = {2u,3u,migrate_v2_to_v3,"v2_to_v3_tlv_header"};
static void rogue_register_core_migrations(void){ if(!g_migrations_registered){ rogue_save_register_migration(&MIG_V2_TO_V3); /* flag set in init */ } }
