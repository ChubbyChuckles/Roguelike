#include "core/loot_security.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t fnv1a32(const void* data, size_t len, uint32_t h){
    const unsigned char* p = (const unsigned char*)data;
    if(h==0) h = 2166136261u;
    for(size_t i=0;i<len;i++){ h ^= p[i]; h *= 16777619u; }
    return h;
}

uint32_t rogue_loot_roll_hash(int table_index, unsigned int seed_before,
                              int drop_count, const int* item_def_indices,
                              const int* quantities, const int* rarities){
    uint32_t h = 2166136261u;
    h = fnv1a32(&table_index, sizeof(table_index), h);
    h = fnv1a32(&seed_before, sizeof(seed_before), h);
    h = fnv1a32(&drop_count, sizeof(drop_count), h);
    for(int i=0;i<drop_count;i++){
        int id = item_def_indices? item_def_indices[i]:-1;
        int qty = quantities? quantities[i]:0;
        int rar = rarities? rarities[i]:-1;
        h = fnv1a32(&id,sizeof id,h);
        h = fnv1a32(&qty,sizeof qty,h);
        h = fnv1a32(&rar,sizeof rar,h);
    }
    return h;
}

static int g_obfuscation_enabled = 0;
void rogue_loot_security_enable_obfuscation(int enable){ g_obfuscation_enabled = enable?1:0; }
int  rogue_loot_security_obfuscation_enabled(void){ return g_obfuscation_enabled; }

unsigned int rogue_loot_obfuscate_seed(unsigned int raw_seed, unsigned int salt){
    /* Simple reversible mix: xor, rotate, multiply, xor */
    unsigned int x = raw_seed ^ (salt * 0x9E3779B9u);
    x = (x << 13) | (x >> 19);
    x = x * 0x85EBCA6Bu + 0xC2B2AE35u;
    x ^= (x >> 16);
    return x;
}

static uint32_t g_last_files_hash = 0;
uint32_t rogue_loot_security_last_files_hash(void){ return g_last_files_hash; }

int rogue_loot_security_snapshot_files(const char* const* paths, int count){
    if(count<0) return -1; uint32_t h=2166136261u; char buf[512];
    for(int i=0;i<count;i++){
        const char* p=paths[i]; if(!p) return -2;
        FILE* f=NULL;
    #if defined(_MSC_VER)
        fopen_s(&f,p,"rb");
    #else
        f=fopen(p,"rb");
    #endif
    if(!f){ /* treat missing file as neutral (skip) so caller can still verify subset */ continue; }
        size_t r; while((r=fread(buf,1,sizeof buf,f))>0){ h = fnv1a32(buf,r,h); }
        fclose(f);
    }
    g_last_files_hash = h; return 0;
}

int rogue_loot_security_verify_files(const char* const* paths, int count){
    if(count<0) return -1; if(g_last_files_hash==0) return -4; uint32_t h=2166136261u; char buf[512];
    for(int i=0;i<count;i++){
        const char* p=paths[i]; if(!p) return -2; FILE* f=NULL;
    #if defined(_MSC_VER)
        fopen_s(&f,p,"rb");
    #else
        f=fopen(p,"rb");
    #endif
        if(!f) continue; size_t r; while((r=fread(buf,1,sizeof buf,f))>0){ h = fnv1a32(buf,r,h); } fclose(f);
    }
    if(g_last_files_hash==0){ g_last_files_hash=h; return 0; }
    return (h==g_last_files_hash)? 0 : 1;
}
