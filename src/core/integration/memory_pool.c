#include "memory_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

// Optional thread safety using SDL mutex if available.
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

// ------------------ Configuration constants ------------------
#define ROGUE_POOL_PAGE_SIZE 4096u
#define ROGUE_SLAB_MAX_CLASSES 64
#define ROGUE_SLAB_MAX_PAGES   256
#define ROGUE_BUDDY_MIN_ORDER  5   // 2^5 = 32 bytes min block
#define ROGUE_BUDDY_MAX_ORDER  20  // 1MB max block (adjusted dynamically if arena bigger)

// Category block sizes
static const size_t s_cat_block[ROGUE_POOL_COUNT] = {32,64,128,256,512};

// ------------------ Fixed block pools ------------------
typedef struct FixedPoolPage {
    struct FixedPoolPage *next;
    uint32_t free_bitmap; // up to 128 slots (we'll compute needed bits)
    uint16_t slots;       // number of slots in this page
    uint16_t free_count;  // free slots remaining
    // Immediately followed by slot data region
} FixedPoolPage;

typedef struct FixedPool {
    FixedPoolPage *pages; // singly linked list
    size_t slot_size;
    uint32_t allocs;
    uint32_t frees;
    size_t capacity_bytes; // total bytes reserved
    size_t in_use_bytes;   // bytes handed out currently
} FixedPool;

static FixedPool s_fixed[ROGUE_POOL_COUNT];

// ------------------ Buddy allocator ------------------
typedef struct BuddyBlock { struct BuddyBlock *next; } BuddyBlock;
static uint8_t *s_buddy_arena = NULL;
static size_t   s_buddy_arena_size = 0;
static int      s_buddy_max_order  = ROGUE_BUDDY_MAX_ORDER; // may adjust
static BuddyBlock *s_free_lists[ROGUE_BUDDY_MAX_ORDER+1];
static size_t s_buddy_free_bytes = 0;
static size_t s_buddy_alloc_failures = 0;

// ------------------ Slab allocator ------------------
typedef struct SlabPage {
    struct SlabPage *next;
    uint32_t free_bitmap; // up to 32 objects/page for simplicity
    uint16_t free_count;
} SlabPage;

typedef struct SlabClass {
    char name[32];
    size_t obj_size;
    int page_obj_count;
    RogueSlabCtor ctor;
    RogueSlabDtor dtor;
    SlabPage *pages;
    size_t live; // objects allocated
    size_t capacity; // total slots across pages
    bool used;
} SlabClass;

static SlabClass s_slab_classes[ROGUE_SLAB_MAX_CLASSES];
static int s_slab_class_count = 0;
static int s_slab_pages_total = 0;

// ------------------ Tracking / diagnostics ------------------
static size_t s_live_allocs = 0;
static size_t s_live_bytes = 0;
static size_t s_peak_live_bytes = 0;
static size_t s_alloc_failures_total = 0;
static bool   s_thread_safe = false;

#ifdef ROGUE_HAVE_SDL
static SDL_mutex *s_mutex = NULL;
#define LOCK() do { if(s_thread_safe && s_mutex) SDL_LockMutex(s_mutex); } while(0)
#define UNLOCK() do { if(s_thread_safe && s_mutex) SDL_UnlockMutex(s_mutex); } while(0)
#else
#define LOCK() do {} while(0)
#define UNLOCK() do {} while(0)
#endif

// Allocation header used for generic allocations to know origin for free & leak reporting.
typedef struct AllocHeader {
    uint32_t magic; // 0xBEEFCAFE
    uint16_t origin; // 0=category,1=buddy
    uint16_t category; // if origin==0
    uint32_t size; // requested size
} AllocHeader;
#define ALLOC_MAGIC 0xBEEFCAFEu

// ------------------ Helpers ------------------
static size_t next_pow2(size_t x){
    if(x<=1) return 1; x--; x|=x>>1; x|=x>>2; x|=x>>4; x|=x>>8; x|=x>>16;
#if SIZE_MAX > 0xFFFFFFFFu
    x|=x>>32;
#endif
    return x+1; }
static int order_for_size(size_t n){ int o=ROGUE_BUDDY_MIN_ORDER; size_t s=((size_t)1)<<o; while(s<n && o<=s_buddy_max_order){ o++; s<<=1; } return o; }

static void fixed_pool_init(FixedPool *p, size_t slot_size){ memset(p,0,sizeof(*p)); p->slot_size=slot_size; }

static void *fixed_pool_alloc(FixedPool *p){
    for(FixedPoolPage *pg=p->pages; pg; pg=pg->next){ if(pg->free_count){ // find first free bit
            uint32_t mask=1u; for(int i=0;i<pg->slots;i++,mask<<=1){ if(!(pg->free_bitmap & mask)){ pg->free_bitmap |= mask; pg->free_count--; p->allocs++; p->in_use_bytes += p->slot_size; return (uint8_t*)(pg+1) + p->slot_size*i; } }
        } }
    // need new page
    size_t slots = (ROGUE_POOL_PAGE_SIZE - sizeof(FixedPoolPage)) / p->slot_size; if(slots>32) slots=32; if(slots==0) return NULL;
    FixedPoolPage *pg = (FixedPoolPage*)malloc(ROGUE_POOL_PAGE_SIZE); if(!pg) return NULL; memset(pg,0,sizeof(FixedPoolPage));
    pg->slots=(uint16_t)slots; pg->free_count=(uint16_t)slots; pg->next=p->pages; p->pages=pg; p->capacity_bytes += slots * p->slot_size; // allocate first slot
    pg->free_bitmap |= 1u; pg->free_count--; p->allocs++; p->in_use_bytes += p->slot_size; return (uint8_t*)(pg+1); }

static void fixed_pool_free(FixedPool *p, void *ptr){ for(FixedPoolPage *pg=p->pages; pg; pg=pg->next){ uint8_t *base = (uint8_t*)(pg+1); size_t bytes = p->slot_size * pg->slots; if(ptr >= (void*)base && ptr < (void*)(base+bytes)){ size_t idx = ((uint8_t*)ptr - base)/p->slot_size; uint32_t mask = 1u<<idx; if(pg->free_bitmap & mask){ pg->free_bitmap &= ~mask; pg->free_count++; p->frees++; p->in_use_bytes -= p->slot_size; } return; } } }

// Buddy allocator -------------------------------------------------------
static void buddy_add_block(int order, BuddyBlock *blk){ blk->next = s_free_lists[order]; s_free_lists[order]=blk; }

static BuddyBlock *buddy_remove_block(int order){ BuddyBlock *blk=s_free_lists[order]; if(blk) s_free_lists[order]=blk->next; return blk; }

static size_t buddy_block_offset(uint8_t *ptr){ return (size_t)(ptr - s_buddy_arena); }

static uint8_t *buddy_alloc(size_t size){ if(!s_buddy_arena) return NULL; size_t needed = size; int order = order_for_size(needed); if(order> s_buddy_max_order) return NULL; LOCK(); int cur=order; while(cur<=s_buddy_max_order && !s_free_lists[cur]) cur++; if(cur> s_buddy_max_order){ s_buddy_alloc_failures++; s_alloc_failures_total++; UNLOCK(); return NULL; }
    // split down
    BuddyBlock *blk = buddy_remove_block(cur); while(cur>order){ cur--; size_t half = (size_t)1<<cur; BuddyBlock *buddy=(BuddyBlock*)((uint8_t*)blk + half); buddy_add_block(cur,buddy); }
    s_buddy_free_bytes -= (size_t)1<<order; UNLOCK(); return (uint8_t*)blk; }

static void buddy_free(uint8_t *ptr, size_t size){ if(!ptr) return; int order = order_for_size(size); LOCK(); size_t offset = buddy_block_offset(ptr); while(order < s_buddy_max_order){ size_t buddy_off = offset ^ ((size_t)1<<order); uint8_t *buddy_ptr = s_buddy_arena + buddy_off; // search free list for buddy
        BuddyBlock **pp=&s_free_lists[order]; bool found=false; while(*pp){ if(*pp == (BuddyBlock*)buddy_ptr){ // remove buddy
                BuddyBlock *rem=*pp; *pp=rem->next; found=true; break; } pp=&(*pp)->next; }
        if(found){ // merge
            offset = offset & buddy_off ? buddy_off : offset; order++; continue; } else break; }
    BuddyBlock *blk=(BuddyBlock*)(s_buddy_arena+offset); buddy_add_block(order, blk); s_buddy_free_bytes += (size_t)1<<order; UNLOCK(); }

int rogue_mp_buddy_defragment(void){ // attempt merges by iterating orders
    int merges=0; LOCK(); // naive single pass; coalescing already done in free path.
    UNLOCK(); return merges; }

// Slab allocator --------------------------------------------------------
static SlabClass *slab_get(RogueSlabHandle h){ if(h<0 || h>=ROGUE_SLAB_MAX_CLASSES) return NULL; if(!s_slab_classes[h].used) return NULL; return &s_slab_classes[h]; }

RogueSlabHandle rogue_slab_register(const char *name, size_t obj_size, int page_obj_count,
                                    RogueSlabCtor ctor, RogueSlabDtor dtor){ if(obj_size==0 || obj_size>2048 || page_obj_count<8 || page_obj_count>32) return -1; for(int i=0;i<ROGUE_SLAB_MAX_CLASSES;i++){ if(!s_slab_classes[i].used){ SlabClass *c=&s_slab_classes[i]; memset(c,0,sizeof(*c)); c->used=true; c->obj_size=obj_size; c->page_obj_count=page_obj_count; if(name) { size_t len=strlen(name); if(len>=sizeof(c->name)) len=sizeof(c->name)-1; memcpy(c->name,name,len); c->name[len]='\0'; } c->ctor=ctor; c->dtor=dtor; s_slab_class_count++; return i; } } return -1; }

static void *slab_alloc_page(SlabClass *c){ size_t need = c->obj_size * c->page_obj_count; if(need + sizeof(SlabPage) > ROGUE_POOL_PAGE_SIZE) return NULL; SlabPage *pg=(SlabPage*)malloc(ROGUE_POOL_PAGE_SIZE); if(!pg) return NULL; memset(pg,0,sizeof(SlabPage)); pg->free_count = (uint16_t)c->page_obj_count; pg->next = c->pages; c->pages=pg; c->capacity += c->page_obj_count; s_slab_pages_total++; return pg; }

void *rogue_slab_alloc(RogueSlabHandle handle){ SlabClass *c=slab_get(handle); if(!c) return NULL; LOCK(); for(SlabPage *pg=c->pages; pg; pg=pg->next){ if(pg->free_count){ uint32_t mask=1u; for(int i=0;i<c->page_obj_count;i++,mask<<=1){ if(!(pg->free_bitmap & mask)){ pg->free_bitmap |= mask; pg->free_count--; uint8_t *base=(uint8_t*)(pg+1); void *obj= base + c->obj_size*i; c->live++; if(c->ctor) c->ctor(obj); UNLOCK(); return obj; } } } }
    // need new page
    if(!slab_alloc_page(c)){ UNLOCK(); return NULL; }
    SlabPage *pg=c->pages; pg->free_bitmap |= 1u; pg->free_count--; uint8_t *base=(uint8_t*)(pg+1); void *obj=base; c->live++; if(c->ctor) c->ctor(obj); UNLOCK(); return obj; }

void rogue_slab_free(RogueSlabHandle handle, void *obj){ if(!obj) return; SlabClass *c=slab_get(handle); if(!c) return; LOCK(); for(SlabPage *pg=c->pages; pg; pg=pg->next){ uint8_t *base=(uint8_t*)(pg+1); size_t bytes=c->obj_size * c->page_obj_count; if(obj >= (void*)base && obj < (void*)(base+bytes)){ size_t idx=((uint8_t*)obj - base)/c->obj_size; uint32_t mask=1u<<idx; if(pg->free_bitmap & mask){ if(c->dtor) c->dtor(obj); pg->free_bitmap &= ~mask; pg->free_count++; c->live--; } break; } } UNLOCK(); }

int rogue_slab_shrink(void){ int freed=0; LOCK(); for(int i=0;i<ROGUE_SLAB_MAX_CLASSES;i++){ SlabClass *c=&s_slab_classes[i]; if(!c->used) continue; SlabPage **pp=&c->pages; while(*pp){ SlabPage *pg=*pp; if(pg->free_count == c->page_obj_count){ *pp=pg->next; free(pg); freed++; s_slab_pages_total--; c->capacity -= c->page_obj_count; } else pp=&(*pp)->next; } } UNLOCK(); return freed; }

// Public API ------------------------------------------------------------
int rogue_memory_pool_init(size_t buddy_arena_bytes, bool thread_safe){ if(s_buddy_arena) return 0; if(buddy_arena_bytes==0) buddy_arena_bytes = 1<<20; // 1MB default
    // ensure power of two >=64KB
    if(buddy_arena_bytes < (1<<16)) buddy_arena_bytes = 1<<16; size_t pow2 = next_pow2(buddy_arena_bytes); if(pow2 != buddy_arena_bytes) buddy_arena_bytes = pow2;
    s_buddy_arena_size = buddy_arena_bytes; s_buddy_arena = (uint8_t*)malloc(s_buddy_arena_size); if(!s_buddy_arena) return -1; memset(s_buddy_arena,0,s_buddy_arena_size);
    // init buddy free lists
    while((((size_t)1)<<s_buddy_max_order) > s_buddy_arena_size) s_buddy_max_order--; for(int i=0;i<=s_buddy_max_order;i++) s_free_lists[i]=NULL; BuddyBlock *root=(BuddyBlock*)s_buddy_arena; root->next=NULL; s_free_lists[s_buddy_max_order]=root; s_buddy_free_bytes = ((size_t)1)<<s_buddy_max_order;
    for(int i=0;i<ROGUE_POOL_COUNT;i++) fixed_pool_init(&s_fixed[i], s_cat_block[i]);
    s_thread_safe = thread_safe;
#ifdef ROGUE_HAVE_SDL
    if(s_thread_safe){ s_mutex = SDL_CreateMutex(); }
#endif
    return 0; }

void rogue_memory_pool_shutdown(void){ // free fixed pages
    for(int i=0;i<ROGUE_POOL_COUNT;i++){ FixedPoolPage *pg=s_fixed[i].pages; while(pg){ FixedPoolPage *n=pg->next; free(pg); pg=n; } memset(&s_fixed[i],0,sizeof(FixedPool)); }
    // free slabs
    for(int i=0;i<ROGUE_SLAB_MAX_CLASSES;i++){ SlabClass *c=&s_slab_classes[i]; if(!c->used) continue; SlabPage *pg=c->pages; while(pg){ SlabPage *n=pg->next; // call dtors for any live objects (best-effort)
            if(c->dtor){ uint32_t mask=pg->free_bitmap; for(int j=0;j<c->page_obj_count;j++){ if(mask & (1u<<j)){ /* free slot */ } else { uint8_t *base=(uint8_t*)(pg+1); void *obj= base + c->obj_size*j; c->dtor(obj); } } }
            free(pg); pg=n; } memset(c,0,sizeof(*c)); }
    // buddy arena
    free(s_buddy_arena); s_buddy_arena=NULL; s_buddy_arena_size=0; for(int i=0;i<=s_buddy_max_order;i++) s_free_lists[i]=NULL; s_buddy_free_bytes=0;
#ifdef ROGUE_HAVE_SDL
    if(s_mutex){ SDL_DestroyMutex(s_mutex); s_mutex=NULL; }
#endif
}

void *rogue_mp_alloc_category(RoguePoolCategory cat){ if(cat<0 || cat>=ROGUE_POOL_COUNT) return NULL; FixedPool *p=&s_fixed[cat]; void *slot=fixed_pool_alloc(p); if(!slot){ s_alloc_failures_total++; return NULL; } LOCK(); s_live_allocs++; s_live_bytes += p->slot_size; if(s_live_bytes > s_peak_live_bytes) s_peak_live_bytes = s_live_bytes; UNLOCK(); AllocHeader *hdr=(AllocHeader*)slot; // store hdr before user? easier: repurpose start of slot; ensure slot large enough
    if(p->slot_size < sizeof(AllocHeader)) return NULL; hdr->magic=ALLOC_MAGIC; hdr->origin=0; hdr->category=(uint16_t)cat; hdr->size=(uint32_t)p->slot_size; return (uint8_t*)slot + sizeof(AllocHeader); }

void *rogue_mp_alloc(size_t size){ if(size==0) size=1; if(size <= s_cat_block[ROGUE_POOL_XL]){ // category route pick minimal category that fits header+size
        size_t need = size + sizeof(AllocHeader); for(int c=0;c<ROGUE_POOL_COUNT;c++){ if(s_cat_block[c] >= need){ void *p=rogue_mp_alloc_category((RoguePoolCategory)c); return p; } }
    }
    // buddy path: allocate header+size rounded to power of two
    size_t total = size + sizeof(AllocHeader); size_t pow2 = next_pow2(total); uint8_t *blk = buddy_alloc(pow2); if(!blk) return NULL; AllocHeader *hdr=(AllocHeader*)blk; hdr->magic=ALLOC_MAGIC; hdr->origin=1; hdr->category=0; hdr->size=(uint32_t)size; LOCK(); s_live_allocs++; s_live_bytes += size; if(s_live_bytes> s_peak_live_bytes) s_peak_live_bytes=s_live_bytes; UNLOCK(); return blk + sizeof(AllocHeader); }

void rogue_mp_free(void *ptr){ if(!ptr) return; AllocHeader *hdr=(AllocHeader*)((uint8_t*)ptr - sizeof(AllocHeader)); if(hdr->magic != ALLOC_MAGIC) return; if(hdr->origin==0){ // category
        RoguePoolCategory cat=(RoguePoolCategory)hdr->category; if(cat>=0 && cat<ROGUE_POOL_COUNT){ FixedPool *p=&s_fixed[cat]; fixed_pool_free(p, hdr); LOCK(); s_live_allocs--; s_live_bytes -= (hdr->size); UNLOCK(); }
    } else if(hdr->origin==1){ // buddy
        size_t total = hdr->size + sizeof(AllocHeader); size_t pow2=next_pow2(total); buddy_free((uint8_t*)hdr, pow2); LOCK(); s_live_allocs--; s_live_bytes -= hdr->size; UNLOCK(); }
}

void rogue_memory_pool_get_stats(RogueMemoryPoolStats *o){ if(!o) return; memset(o,0,sizeof(*o)); for(int i=0;i<ROGUE_POOL_COUNT;i++){ o->category_capacity[i]=s_fixed[i].capacity_bytes; o->category_in_use[i]=s_fixed[i].in_use_bytes; o->category_allocs[i]=s_fixed[i].allocs; o->category_frees[i]=s_fixed[i].frees; }
    o->buddy_total_bytes = s_buddy_arena_size; o->buddy_free_bytes = s_buddy_free_bytes; // naive fragmentation metric (1 - largest_free / total_free)
    size_t largest=0; for(int o2=ROGUE_BUDDY_MIN_ORDER;o2<=s_buddy_max_order;o2++){ if(s_free_lists[o2]){ size_t sz = (size_t)1<<o2; if(sz>largest) largest=sz; } }
    if(s_buddy_free_bytes>0) o->buddy_fragmentation = 1.0f - (float)largest / (float)s_buddy_free_bytes; else o->buddy_fragmentation=0.f;
    o->slab_classes = s_slab_class_count; o->slab_pages = s_slab_pages_total; size_t live=0, cap=0; for(int i=0;i<ROGUE_SLAB_MAX_CLASSES;i++){ if(s_slab_classes[i].used){ live += s_slab_classes[i].live; cap += s_slab_classes[i].capacity; } } o->slab_objects_live=live; o->slab_objects_capacity=cap; o->live_allocs = s_live_allocs; o->live_bytes = s_live_bytes; o->peak_live_bytes = s_peak_live_bytes; o->alloc_failures = s_alloc_failures_total; }

void rogue_memory_pool_get_recommendations(RogueMemoryPoolRecommendation *r){ if(!r) return; memset(r,0,sizeof(*r)); RogueMemoryPoolStats s; rogue_memory_pool_get_stats(&s); // simple heuristics
    if(s.category_in_use[ROGUE_POOL_TINY] > s.category_capacity[ROGUE_POOL_TINY]*3/4) r->advise_expand_tiny=true; if(s.category_in_use[ROGUE_POOL_XL] < s.category_capacity[ROGUE_POOL_XL]/10 && s.category_capacity[ROGUE_POOL_XL]>0) r->advise_reduce_xl=true; if(s.buddy_fragmentation > 0.65f) r->advise_rebalance_buddy=true; if(s.slab_objects_capacity>0 && s.slab_objects_live > s.slab_objects_capacity*7/8) r->advise_add_slab_page=true; if(!s_thread_safe && s.live_allocs > 10000) r->advise_enable_thread_safety=true; }

void rogue_memory_pool_dump(void){ RogueMemoryPoolStats s; rogue_memory_pool_get_stats(&s); printf("[memory_pool]\n"); for(int i=0;i<ROGUE_POOL_COUNT;i++){ printf(" category %d: cap=%zu in_use=%zu allocs=%zu frees=%zu\n", i, s.category_capacity[i], s.category_in_use[i], s.category_allocs[i], s.category_frees[i]); }
    printf(" buddy: total=%zu free=%zu frag=%.2f\n", s.buddy_total_bytes, s.buddy_free_bytes, s.buddy_fragmentation); printf(" slabs: classes=%zu pages=%zu live=%zu/%zu\n", s.slab_classes, s.slab_pages, s.slab_objects_live, s.slab_objects_capacity); printf(" live_allocs=%zu live_bytes=%zu peak=%zu failures=%zu\n", s.live_allocs, s.live_bytes, s.peak_live_bytes, s.alloc_failures); }

bool rogue_memory_pool_validate(void){ bool ok=true; for(int i=0;i<ROGUE_POOL_COUNT;i++){ FixedPool *p=&s_fixed[i]; size_t calc_in_use=0; for(FixedPoolPage *pg=p->pages; pg; pg=pg->next){ uint32_t mask=pg->free_bitmap; int used=0; for(int b=0;b<pg->slots;b++){ if(mask & (1u<<b)) used++; } calc_in_use += (size_t)used * p->slot_size; if(used + pg->free_count != pg->slots) ok=false; }
        if(calc_in_use != p->in_use_bytes) ok=false; }
    return ok; }

void rogue_memory_pool_enumerate_leaks(RogueLeakCallback cb){ (void)cb; // Not storing individual allocation metadata beyond header; future enhancement could keep list.
}
