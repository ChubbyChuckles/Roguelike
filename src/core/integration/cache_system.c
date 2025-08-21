#include "cache_system.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// Simple hash map per level with open addressing linear probe.
// Compression: naive RLE (byte,run) pairs if saves >= (raw_size/8) and size >= threshold.

#define ROGUE_CACHE_DEFAULT_L1 256
#define ROGUE_CACHE_DEFAULT_L2 512
#define ROGUE_CACHE_DEFAULT_L3 1024

typedef struct CacheEntry {
    uint64_t key;
    uint32_t version;
    uint32_t level;      // current level (0,1,2)
    uint32_t raw_size;   // original size
    uint32_t data_size;  // stored size (compressed or raw)
    unsigned compressed:1;
    unsigned tombstone:1;
    unsigned pad:30;
    void *data;          // owned buffer
} CacheEntry;

typedef struct CacheLevel {
    CacheEntry *entries;
    size_t capacity; // table slots
    size_t count;    // live entries (excluding tombstones)
} CacheLevel;

static CacheLevel s_levels[ROGUE_CACHE_LEVELS];
static size_t s_capacity_entries[ROGUE_CACHE_LEVELS];
static uint64_t s_hits[ROGUE_CACHE_LEVELS];
static uint64_t s_misses[ROGUE_CACHE_LEVELS];
static uint64_t s_evictions[ROGUE_CACHE_LEVELS];
static uint64_t s_invalidations[ROGUE_CACHE_LEVELS];
static uint64_t s_promotions[ROGUE_CACHE_LEVELS];
static uint64_t s_compressed_entries = 0;
static size_t   s_compressed_saved = 0;
static uint64_t s_preloads = 0;
static size_t   s_compress_threshold = 1024;

static uint64_t hash_key(uint64_t k){ // xorshift mix
    k ^= k >> 33; k *= 0xff51afd7ed558ccdULL; k ^= k >> 33; k *= 0xc4ceb9fe1a85ec53ULL; k ^= k >> 33; return k; }

static size_t next_pow2(size_t x){
    if(x<=1) return 1;
    x--;
    x|=x>>1;
    x|=x>>2;
    x|=x>>4;
    x|=x>>8;
    x|=x>>16;
#if SIZE_MAX > 0xFFFFFFFFu
    x|=x>>32;
#endif
    return x+1;
}

static int level_init(CacheLevel *lvl, size_t cap){ lvl->capacity = cap; lvl->entries = (CacheEntry*)calloc(cap, sizeof(CacheEntry)); return lvl->entries?0:-1; }

int rogue_cache_init(size_t cap_l1, size_t cap_l2, size_t cap_l3){
    if(cap_l1==0) cap_l1=ROGUE_CACHE_DEFAULT_L1; if(cap_l2==0) cap_l2=ROGUE_CACHE_DEFAULT_L2; if(cap_l3==0) cap_l3=ROGUE_CACHE_DEFAULT_L3;
    cap_l1 = next_pow2(cap_l1*2); cap_l2 = next_pow2(cap_l2*2); cap_l3 = next_pow2(cap_l3*2); // load factor target 0.5
    if(level_init(&s_levels[0], cap_l1) || level_init(&s_levels[1], cap_l2) || level_init(&s_levels[2], cap_l3)) return -1;
    s_capacity_entries[0]=cap_l1/2; s_capacity_entries[1]=cap_l2/2; s_capacity_entries[2]=cap_l3/2; // logical capacity (#entries before rehash needed; we don't resize in this slice)
    return 0;
}

void rogue_cache_shutdown(void){ for(int i=0;i<ROGUE_CACHE_LEVELS;i++){ CacheLevel *lvl=&s_levels[i]; if(lvl->entries){ for(size_t j=0;j<lvl->capacity;j++){ CacheEntry *e=&lvl->entries[j]; if(e->key && !e->tombstone && e->data) free(e->data); } free(lvl->entries); memset(lvl,0,sizeof(*lvl)); } }
    memset(s_hits,0,sizeof(s_hits)); memset(s_misses,0,sizeof(s_misses)); memset(s_evictions,0,sizeof(s_evictions)); memset(s_invalidations,0,sizeof(s_invalidations)); memset(s_promotions,0,sizeof(s_promotions)); s_compressed_entries=0; s_compressed_saved=0; s_preloads=0;
}

static void *rle_compress(const void *data, size_t size, size_t *out_size){ const unsigned char *in=(const unsigned char*)data; unsigned char *out = (unsigned char*)malloc(size*2); if(!out) return NULL; size_t w=0; for(size_t i=0;i<size;){ unsigned char b=in[i]; size_t run=1; while(i+run<size && in[i+run]==b && run<255) run++; out[w++]=b; out[w++]=(unsigned char)run; i+=run; } if(w >= size - size/8){ free(out); return NULL; } *out_size=w; return out; }

static CacheEntry *find_slot(CacheLevel *lvl, uint64_t key, size_t *out_index){ size_t mask = lvl->capacity - 1; uint64_t h = hash_key(key); size_t idx = (size_t)h & mask; size_t first_tomb=(size_t)-1; for(size_t probe=0; probe<lvl->capacity; probe++){ CacheEntry *e=&lvl->entries[idx]; if(!e->key){ if(out_index){ *out_index = (first_tomb!=(size_t)-1)?first_tomb:idx; } return NULL; } if(e->key==key && !e->tombstone){ if(out_index) *out_index=idx; return e; } if(e->tombstone && first_tomb==(size_t)-1) first_tomb=idx; idx=(idx+1)&mask; }
    return NULL; }

static int insert_entry(int level, uint64_t key, const void *data, size_t size, uint32_t version){ CacheLevel *lvl=&s_levels[level]; if(lvl->count >= s_capacity_entries[level]){ // simple eviction: linear scan remove oldest tombstone or first live
        for(size_t i=0;i<lvl->capacity;i++){ CacheEntry *e=&lvl->entries[i]; if(e->key && !e->tombstone){ if(e->data) free(e->data); e->tombstone=1; s_evictions[level]++; lvl->count--; break; } }
    }
    size_t idx; CacheEntry *existing = find_slot(lvl,key,&idx); if(existing){ // update
        if(existing->data) free(existing->data);
    existing->version=version; existing->level=level; existing->compressed=0; existing->raw_size=(uint32_t)size; existing->tombstone=0;
    size_t csize=0; void *cbuf = (s_compress_threshold && size>=s_compress_threshold)? rle_compress(data,size,&csize):NULL; if(cbuf){ existing->data=cbuf; existing->data_size=(uint32_t)csize; existing->compressed=1; s_compressed_entries++; s_compressed_saved += size - csize; } else { existing->data=malloc(size); if(!existing->data) return -1; memcpy(existing->data,data,size); existing->data_size=(uint32_t)size; }
        return 0;
    }
    CacheEntry *e=&lvl->entries[idx]; if(!e->key || e->tombstone){ if(!e->key) lvl->count++; e->key=key; e->tombstone=0; e->version=version; e->level=level; e->compressed=0; e->raw_size=(uint32_t)size; size_t csize=0; void *cbuf=(s_compress_threshold && size>=s_compress_threshold)? rle_compress(data,size,&csize):NULL; if(cbuf){ e->data=cbuf; e->data_size=(uint32_t)csize; e->compressed=1; s_compressed_entries++; s_compressed_saved += size - csize; } else { e->data=malloc(size); if(!e->data) return -1; memcpy(e->data,data,size); e->data_size=(uint32_t)size; }
        return 0; }
    return -1; }

static int pick_level(size_t size){ if(size <= 256) return 0; if(size <= 4096) return 1; return 2; }

int rogue_cache_put(uint64_t key, const void *data, size_t size, uint32_t version, int level_hint){ if(level_hint<0 || level_hint>=ROGUE_CACHE_LEVELS) level_hint = pick_level(size); return insert_entry(level_hint,key,data,size,version); }

int rogue_cache_get(uint64_t key, void **out_data, size_t *out_size, uint32_t *out_version){ for(int lvl=0; lvl<ROGUE_CACHE_LEVELS; lvl++){ CacheLevel *L=&s_levels[lvl]; size_t idx; CacheEntry *e=find_slot(L,key,&idx); if(e){ s_hits[lvl]++; if(out_data) *out_data=e->data; if(out_size) *out_size=e->raw_size; if(out_version) *out_version=e->version; // promote if not L1
            if(lvl>0){ // reinsert at higher priority (L1)
                insert_entry(0,key,e->data,e->raw_size,e->version); s_promotions[0]++; s_promotions[lvl]++; }
            return 1; } }
    for(int lvl=0; lvl<ROGUE_CACHE_LEVELS; lvl++) s_misses[lvl]++; return 0; }

void rogue_cache_invalidate(uint64_t key){ for(int lvl=0; lvl<ROGUE_CACHE_LEVELS; lvl++){ CacheLevel *L=&s_levels[lvl]; size_t mask=L->capacity-1; uint64_t h=hash_key(key); size_t idx=(size_t)h & mask; for(size_t probe=0; probe<L->capacity; probe++){ CacheEntry *e=&L->entries[idx]; if(!e->key) break; if(e->key==key && !e->tombstone){ if(e->data) free(e->data); e->data=NULL; e->tombstone=1; L->count--; s_invalidations[lvl]++; break; } idx=(idx+1)&mask; } } }

void rogue_cache_invalidate_all(void){ for(int lvl=0; lvl<ROGUE_CACHE_LEVELS; lvl++){ CacheLevel *L=&s_levels[lvl]; for(size_t i=0;i<L->capacity;i++){ CacheEntry *e=&L->entries[i]; if(e->key && !e->tombstone){ if(e->data) free(e->data); e->data=NULL; e->tombstone=1; } } s_invalidations[lvl]+=L->count; L->count=0; } }

int rogue_cache_preload(const uint64_t *keys, int count, RogueCacheLevel target_level,
                        int (*loader)(uint64_t, void **, size_t *, uint32_t *)){
    if(target_level<0 || target_level>=ROGUE_CACHE_LEVELS) target_level=ROGUE_CACHE_L2; int loaded=0;
    for(int i=0;i<count;i++){ void *buf=NULL; size_t sz=0; uint32_t ver=0; if(loader(keys[i], &buf, &sz, &ver)==0){ if(insert_entry(target_level, keys[i], buf, sz, ver)==0){ loaded++; s_preloads++; } if(buf) free(buf); } }
    return loaded;
}

void rogue_cache_get_stats(RogueCacheStats *out){ if(!out) return; memset(out,0,sizeof(*out)); for(int i=0;i<ROGUE_CACHE_LEVELS;i++){ out->level_capacity[i]=s_capacity_entries[i]; out->level_entries[i]=s_levels[i].count; out->level_hits[i]=s_hits[i]; out->level_misses[i]=s_misses[i]; out->level_evictions[i]=s_evictions[i]; out->level_invalidations[i]=s_invalidations[i]; out->level_promotions[i]=s_promotions[i]; } out->compressed_entries = s_compressed_entries; out->compressed_bytes_saved = s_compressed_saved; out->preload_operations = s_preloads; }

void rogue_cache_dump(void){ RogueCacheStats s; rogue_cache_get_stats(&s); printf("[cache]\n"); for(int i=0;i<ROGUE_CACHE_LEVELS;i++){ printf(" L%d: entries=%zu cap=%zu hits=%llu misses=%llu evict=%llu inval=%llu promo=%llu\n", i, s.level_entries[i], s.level_capacity[i], (unsigned long long)s.level_hits[i], (unsigned long long)s.level_misses[i], (unsigned long long)s.level_evictions[i], (unsigned long long)s.level_invalidations[i], (unsigned long long)s.level_promotions[i]); } printf(" compressed=%llu saved=%zu preload=%llu\n", (unsigned long long)s.compressed_entries, s.compressed_bytes_saved, (unsigned long long)s.preload_operations); }

void rogue_cache_iterate(RogueCacheIterFn fn, void *ud){ if(!fn) return; for(int lvl=0; lvl<ROGUE_CACHE_LEVELS; lvl++){ CacheLevel *L=&s_levels[lvl]; for(size_t i=0;i<L->capacity;i++){ CacheEntry *e=&L->entries[i]; if(e->key && !e->tombstone){ if(!fn(e->key, e->data, e->raw_size, e->version, lvl, ud)) return; } } } }

void rogue_cache_promote(uint64_t key){ for(int lvl=ROGUE_CACHE_LEVELS-1; lvl>0; lvl--){ CacheLevel *L=&s_levels[lvl]; size_t idx; CacheEntry *e=find_slot(L,key,&idx); if(e){ insert_entry(lvl-1,key,e->data,e->raw_size,e->version); s_promotions[lvl-1]++; s_promotions[lvl]++; break; } } }

void rogue_cache_set_compress_threshold(size_t bytes){ s_compress_threshold=bytes; }
