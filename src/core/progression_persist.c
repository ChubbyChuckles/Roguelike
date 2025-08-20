/* Phase 12: Versioned progression persistence & migration */
#include "core/progression_persist.h"
#include "core/save_manager.h"
#include "core/progression_passives.h"
#include "core/progression_stats.h"
#include "core/progression_attributes.h"
#include <string.h>

#define ROGUE_PROG_SAVE_VERSION 2u

typedef struct ProgHeaderV1 { uint32_t version; uint32_t level; uint64_t xp_total; uint32_t attr_str, attr_dex, attr_vit, attr_int; uint32_t unspent_pts; uint32_t respec_tokens; uint64_t attr_journal_hash; uint64_t passive_journal_hash; uint32_t passive_entry_count; } ProgHeaderV1;
typedef struct PassiveEntryDisk { int32_t node_id; uint32_t timestamp_ms; } PassiveEntryDisk;
typedef struct ProgHeaderV2 { uint32_t version; uint32_t level; uint64_t xp_total; uint64_t stat_registry_fp; uint32_t maze_node_count; uint32_t attr_str, attr_dex, attr_vit, attr_int; uint32_t unspent_pts; uint32_t respec_tokens; uint64_t attr_journal_hash; uint64_t passive_journal_hash; uint32_t passive_entry_count; } ProgHeaderV2;

static unsigned long long g_chain_hash=0; static unsigned int g_last_migration_flags=0;
static unsigned long long fold64(unsigned long long h, unsigned long long v){ h ^= v + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2); return h; }
unsigned long long rogue_progression_persist_chain_hash(void){ return g_chain_hash; }
unsigned int rogue_progression_persist_last_migration_flags(void){ return g_last_migration_flags; }
extern struct AppState { int level; unsigned long long xp_total_accum; } g_app; 
extern RogueAttributeState g_attr_state; extern unsigned long long rogue_progression_passives_journal_hash(void);
static void write_unlocked_passives(FILE* f, uint32_t* count_out){ *count_out=0; (void)f; }
static int read_unlocked_passives(FILE* f, uint32_t count){ for(uint32_t i=0;i<count;i++){ PassiveEntryDisk e; if(fread(&e,sizeof e,1,f)!=1) return -1; } return 0; }
int rogue_progression_persist_write(FILE* f){ if(!f) return -1; g_chain_hash=0xcbf29ce484222325ULL; g_last_migration_flags=0; ProgHeaderV2 h; memset(&h,0,sizeof h); h.version=ROGUE_PROG_SAVE_VERSION; h.level=(uint32_t)g_app.level; h.xp_total=g_app.xp_total_accum; h.stat_registry_fp=rogue_stat_registry_fingerprint(); h.maze_node_count=0; h.attr_str=(uint32_t)g_attr_state.strength; h.attr_dex=(uint32_t)g_attr_state.dexterity; h.attr_vit=(uint32_t)g_attr_state.vitality; h.attr_int=(uint32_t)g_attr_state.intelligence; h.unspent_pts=(uint32_t)g_attr_state.spent_points; h.respec_tokens=(uint32_t)g_attr_state.respec_tokens; h.attr_journal_hash=g_attr_state.journal_hash; h.passive_journal_hash=rogue_progression_passives_journal_hash(); uint32_t entry_count=0; write_unlocked_passives(f,&entry_count); h.passive_entry_count=entry_count; fseek(f,0,SEEK_SET); if(fwrite(&h,sizeof h,1,f)!=1) return -2; g_chain_hash = fold64(g_chain_hash,h.version); g_chain_hash=fold64(g_chain_hash,h.level); g_chain_hash=fold64(g_chain_hash,h.xp_total); g_chain_hash=fold64(g_chain_hash,h.stat_registry_fp); g_chain_hash=fold64(g_chain_hash,h.passive_journal_hash); return 0; }
int rogue_progression_persist_read(FILE* f){ if(!f) return -1; g_chain_hash=0xcbf29ce484222325ULL; g_last_migration_flags=0; long start=ftell(f); uint32_t version=0; if(fread(&version,sizeof version,1,f)!=1) return -2; fseek(f,start,SEEK_SET); if(version==1){ ProgHeaderV1 h; if(fread(&h,sizeof h,1,f)!=1) return -3; g_app.level=(int)h.level; g_app.xp_total_accum=h.xp_total; g_attr_state.strength=(int)h.attr_str; g_attr_state.dexterity=(int)h.attr_dex; g_attr_state.vitality=(int)h.attr_vit; g_attr_state.intelligence=(int)h.attr_int; g_attr_state.spent_points=(int)h.unspent_pts; g_attr_state.respec_tokens=(int)h.respec_tokens; g_attr_state.journal_hash=h.attr_journal_hash; if(h.passive_journal_hash!=rogue_progression_passives_journal_hash()) g_last_migration_flags |= ROGUE_PROG_MIG_STAT_REG_CHANGED; if(read_unlocked_passives(f,h.passive_entry_count)!=0) return -4; g_chain_hash=fold64(g_chain_hash,h.version); g_chain_hash=fold64(g_chain_hash,h.level); g_chain_hash=fold64(g_chain_hash,h.xp_total); g_chain_hash=fold64(g_chain_hash,0ULL); g_chain_hash=fold64(g_chain_hash,h.passive_journal_hash); }
 else if(version==2){ ProgHeaderV2 h; if(fread(&h,sizeof h,1,f)!=1) return -5; g_app.level=(int)h.level; g_app.xp_total_accum=h.xp_total; g_attr_state.strength=(int)h.attr_str; g_attr_state.dexterity=(int)h.attr_dex; g_attr_state.vitality=(int)h.attr_vit; g_attr_state.intelligence=(int)h.attr_int; g_attr_state.spent_points=(int)h.unspent_pts; g_attr_state.respec_tokens=(int)h.respec_tokens; g_attr_state.journal_hash=h.attr_journal_hash; unsigned long long cur_fp=rogue_stat_registry_fingerprint(); if(cur_fp!=h.stat_registry_fp) g_last_migration_flags |= ROGUE_PROG_MIG_STAT_REG_CHANGED; if(read_unlocked_passives(f,h.passive_entry_count)!=0) return -6; g_chain_hash=fold64(g_chain_hash,h.version); g_chain_hash=fold64(g_chain_hash,h.level); g_chain_hash=fold64(g_chain_hash,h.xp_total); g_chain_hash=fold64(g_chain_hash,h.stat_registry_fp); g_chain_hash=fold64(g_chain_hash,h.passive_journal_hash); }
 else { return -7; } return 0; }
static int save_component_write(FILE* f){ return rogue_progression_persist_write(f); }
static int save_component_read(FILE* f, size_t size){ (void)size; return rogue_progression_persist_read(f); }
int rogue_progression_persist_register(void){ RogueSaveComponent comp; memset(&comp,0,sizeof comp); comp.id=27; comp.name="progression"; comp.write_fn=save_component_write; comp.read_fn=save_component_read; rogue_save_manager_register(&comp); return 0; }
void rogue_progression_persist_reset_state_for_tests(void){ g_chain_hash=0; g_last_migration_flags=0; }
