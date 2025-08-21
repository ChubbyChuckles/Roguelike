// Reference counting implementation with portable atomics.
// MSVC C mode currently lacks full <stdatomic.h>; use Interlocked APIs there.
#include "ref_count.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#ifdef _MSC_VER
#include <windows.h>
#else
#include <stdatomic.h>
#endif

// Internal header placed before user payload.
typedef struct RogueRcHeader {
#ifdef _MSC_VER
    volatile LONG strong;
    volatile LONG weak;
#else
    atomic_uint strong;
    atomic_uint weak;
#endif
    uint32_t type_id;
    uint64_t id;
    void (*dtor)(void *);
    struct RogueRcHeader *next;
    struct RogueRcHeader *prev;
} RogueRcHeader;

// Edge enumerator registry (simple fixed table for now)
#define RC_MAX_EDGE_ENUM 64
static struct { uint32_t type_id; RogueRcEdgeEnumFn fn; } s_edge_enum[RC_MAX_EDGE_ENUM];
#ifdef _MSC_VER
static volatile LONG s_edge_enum_count = 0;
static volatile LONG64 s_next_id = 1;
static volatile LONG64 s_total_allocs = 0;
static volatile LONG64 s_total_frees = 0;
static volatile LONG64 s_live_objects = 0;
static volatile LONG64 s_peak_live = 0;
#else
static atomic_uint s_edge_enum_count = 0;
static atomic_uint_fast64_t s_next_id = 1;
static atomic_uint_fast64_t s_total_allocs = 0;
static atomic_uint_fast64_t s_total_frees = 0;
static atomic_uint_fast64_t s_live_objects = 0;
static atomic_uint_fast64_t s_peak_live = 0;
#endif

// Live list head (doubly).
static RogueRcHeader *s_live_head = NULL;
// Simple spin lock for list mutation.
#ifdef _MSC_VER
static volatile LONG s_live_lock = 0;
static void live_lock(void){ while(InterlockedCompareExchange(&s_live_lock,1,0)!=0) { /* spin */ } }
static void live_unlock(void){ InterlockedExchange(&s_live_lock,0); }
#else
static atomic_flag s_live_lock = ATOMIC_FLAG_INIT;
static void live_lock(void){ while(atomic_flag_test_and_set_explicit(&s_live_lock, memory_order_acquire)) { } }
static void live_unlock(void){ atomic_flag_clear_explicit(&s_live_lock, memory_order_release); }
#endif

static RogueRcHeader *hdr_from_obj(void *obj){ if(!obj) return NULL; return ((RogueRcHeader*)obj)-1; }
static const RogueRcHeader *hdr_from_cobj(const void *obj){ if(!obj) return NULL; return ((const RogueRcHeader*)obj)-1; }

void *rogue_rc_alloc(size_t size, uint32_t type_id, void (*dtor)(void *)){
    size_t total = sizeof(RogueRcHeader) + (size?size:1);
    RogueRcHeader *h = (RogueRcHeader*)calloc(1,total);
    if(!h) return NULL;
    #ifdef _MSC_VER
    h->strong = 1;
    h->weak = 1;
    #else
    atomic_init(&h->strong,1);
    atomic_init(&h->weak,1);
    #endif
    h->type_id = type_id;
    #ifdef _MSC_VER
    h->id = (uint64_t)InterlockedIncrement64(&s_next_id) - 1;
    #else
    h->id = (uint64_t)atomic_fetch_add_explicit(&s_next_id,1,memory_order_relaxed);
    #endif
    h->dtor = dtor;
    #ifdef _MSC_VER
    InterlockedIncrement64(&s_total_allocs);
    LONG64 live_now = InterlockedIncrement64(&s_live_objects);
    live_lock(); if(live_now > s_peak_live) s_peak_live = live_now; live_unlock();
    #else
    atomic_fetch_add_explicit(&s_total_allocs,1,memory_order_relaxed);
    atomic_fetch_add_explicit(&s_live_objects,1,memory_order_relaxed);
    uint64_t live_now = atomic_load_explicit(&s_live_objects,memory_order_relaxed);
    uint64_t peak = atomic_load_explicit(&s_peak_live,memory_order_relaxed);
    while(live_now>peak && !atomic_compare_exchange_weak(&s_peak_live,&peak,live_now)) { peak = atomic_load(&s_peak_live); }
    #endif

    // insert into live list
    live_lock();
    h->next = s_live_head;
    if(s_live_head) s_live_head->prev = h;
    s_live_head = h;
    live_unlock();

    return (void*)(h+1);
}

void rogue_rc_retain(void *obj)
{
    RogueRcHeader *h = hdr_from_obj(obj);
    if(!h) return;
#ifdef _MSC_VER
    InterlockedIncrement(&h->strong);
#else
    atomic_fetch_add_explicit(&h->strong,1,memory_order_relaxed);
#endif
}

static void live_remove(RogueRcHeader *h){ if(!h) return; if(h->prev) h->prev->next = h->next; else s_live_head = h->next; if(h->next) h->next->prev = h->prev; h->next=h->prev=NULL; }

void rogue_rc_release(void *obj){
    RogueRcHeader *h=hdr_from_obj(obj); if(!h) return;
    unsigned prev;
#ifdef _MSC_VER
    prev = (unsigned)InterlockedExchangeAdd(&h->strong,-1);
#else
    prev = atomic_fetch_sub_explicit(&h->strong,1,memory_order_acq_rel);
#endif
    assert(prev>0);
    if(prev==1){ // last strong
        // call destructor before removing from list (payload still valid)
        if(h->dtor) h->dtor((void*)(h+1));
        // remove from live list
        live_lock();
        live_remove(h);
        live_unlock();
    #ifdef _MSC_VER
    InterlockedDecrement64(&s_live_objects);
    #else
    atomic_fetch_sub_explicit(&s_live_objects,1,memory_order_relaxed);
    #endif
        // drop implicit weak held by strong lifetime
    unsigned wprev;
#ifdef _MSC_VER
    wprev = (unsigned)InterlockedExchangeAdd(&h->weak,-1);
#else
    wprev = atomic_fetch_sub_explicit(&h->weak,1,memory_order_acq_rel);
#endif
        (void)wprev;
        // if weak now zero free memory
    #ifdef _MSC_VER
    if(h->weak==0){ InterlockedIncrement64(&s_total_frees); free(h); }
    #else
    if(atomic_load_explicit(&h->weak,memory_order_acquire)==0){ atomic_fetch_add_explicit(&s_total_frees,1,memory_order_relaxed); free(h); }
    #endif
    }
}

uint64_t rogue_rc_get_id(const void *obj){ const RogueRcHeader *h=hdr_from_cobj(obj); return h? h->id : 0; }
uint32_t rogue_rc_get_type(const void *obj){ const RogueRcHeader *h=hdr_from_cobj(obj); return h? h->type_id : 0; }
uint32_t rogue_rc_get_strong(const void *obj)
{
    const RogueRcHeader *h = hdr_from_cobj(obj);
    if(!h) return 0;
#ifdef _MSC_VER
    return (uint32_t)h->strong;
#else
    return atomic_load_explicit(&((RogueRcHeader*)h)->strong,memory_order_relaxed);
#endif
}

uint32_t rogue_rc_get_weak(const void *obj)
{
    const RogueRcHeader *h = hdr_from_cobj(obj);
    if(!h) return 0;
#ifdef _MSC_VER
    return (uint32_t)h->weak;
#else
    return atomic_load_explicit(&((RogueRcHeader*)h)->weak,memory_order_relaxed);
#endif
}
/* duplicate rogue_rc_get_weak removed during refactor */

RogueWeakRef rogue_rc_weak_from(void *obj)
{
    RogueWeakRef w; w.hdr=NULL;
    RogueRcHeader *h = hdr_from_obj(obj);
    if(!h){ return w; }
#ifdef _MSC_VER
    InterlockedIncrement(&h->weak);
#else
    atomic_fetch_add_explicit(&h->weak,1,memory_order_relaxed);
#endif
    w.hdr = h;
    return w;
}

void *rogue_rc_weak_acquire(RogueWeakRef weak)
{
    RogueRcHeader *h = weak.hdr;
    if(!h) return NULL;
#ifdef _MSC_VER
    for(;;){
        LONG s = h->strong;
        if(s==0) return NULL;
        if(InterlockedCompareExchange(&h->strong,s+1,s)==s) return (void*)(h+1);
    }
#else
    unsigned s = atomic_load_explicit(&h->strong,memory_order_acquire);
    while(s){
        if(atomic_compare_exchange_weak(&h->strong,&s,s+1)) return (void*)(h+1);
    }
    return NULL;
#endif
}

void rogue_rc_weak_release(RogueWeakRef *weak)
{
    if(!weak || !weak->hdr) return;
    RogueRcHeader *h = weak->hdr;
    weak->hdr = NULL;
    unsigned prev;
#ifdef _MSC_VER
    prev = (unsigned)InterlockedExchangeAdd(&h->weak,-1);
#else
    prev = atomic_fetch_sub_explicit(&h->weak,1,memory_order_acq_rel);
#endif
    assert(prev>0);
    if(prev==1){
#ifdef _MSC_VER
        InterlockedIncrement64(&s_total_frees);
#else
        atomic_fetch_add_explicit(&s_total_frees,1,memory_order_relaxed);
#endif
        free(h);
    }
}

void rogue_rc_get_stats(RogueRcStats *out)
{
    if(!out) return;
#ifdef _MSC_VER
    out->total_allocs=(uint64_t)s_total_allocs;
    out->total_frees=(uint64_t)s_total_frees;
    out->live_objects=(uint64_t)s_live_objects;
    out->peak_live=(uint64_t)s_peak_live;
#else
    out->total_allocs=atomic_load(&s_total_allocs);
    out->total_frees=atomic_load(&s_total_frees);
    out->live_objects=atomic_load(&s_live_objects);
    out->peak_live=atomic_load(&s_peak_live);
#endif
}

void rogue_rc_dump_leaks(void *fptr)
{
    FILE *f = fptr? (FILE*)fptr : stdout;
    live_lock();
    RogueRcHeader *cur=s_live_head;
    if(!cur){
        fprintf(f,"[rc] no leaks (live strong=0)\n");
        live_unlock();
        return;
    }
    fprintf(f,"[rc] live strong objects:\n");
    while(cur){
#ifdef _MSC_VER
        fprintf(f," id=%llu type=%u strong=%ld weak=%ld\n", (unsigned long long)cur->id, cur->type_id, cur->strong, cur->weak);
#else
        fprintf(f," id=%llu type=%u strong=%u weak=%u\n", (unsigned long long)cur->id, cur->type_id, atomic_load(&cur->strong), atomic_load(&cur->weak));
#endif
        cur=cur->next;
    }
    live_unlock();
}

void rogue_rc_iterate(RogueRcIterFn fn, void *ud){ if(!fn) return; live_lock(); RogueRcHeader *cur=s_live_head; while(cur){ RogueRcHeader *next=cur->next; // allow callback to release
        if(!fn((void*)(cur+1), cur->type_id, cur->id, ud)){ live_unlock(); return; } cur=next; } live_unlock(); }

bool rogue_rc_register_edge_enumerator(uint32_t type_id, RogueRcEdgeEnumFn fn)
{
    if(!fn) return false;
    unsigned idx;
#ifdef _MSC_VER
    idx = (unsigned)InterlockedExchangeAdd(&s_edge_enum_count,1);
#else
    idx = atomic_fetch_add_explicit(&s_edge_enum_count,1,memory_order_acq_rel);
#endif
    if(idx>=RC_MAX_EDGE_ENUM) return false;
    s_edge_enum[idx].type_id = type_id;
    s_edge_enum[idx].fn = fn;
    return true;
}

static RogueRcEdgeEnumFn find_enum(uint32_t type)
{
    unsigned count;
#ifdef _MSC_VER
    count = (unsigned)s_edge_enum_count;
#else
    count = atomic_load(&s_edge_enum_count);
#endif
    for(unsigned i=0;i<count;i++){
        if(s_edge_enum[i].type_id==type) return s_edge_enum[i].fn;
    }
    return NULL;
}

size_t rogue_rc_generate_dot(char *buf, size_t max){ // 2-pass length compute
    const char *header="digraph rc {\n"; const char *footer="}\n"; size_t needed=strlen(header)+strlen(footer); // enumerate nodes & edges
    // Conservative temporary storage in stack iteration (edges up to 32 per node for now).
    char line[256];
    live_lock(); RogueRcHeader *cur=s_live_head; while(cur){
#ifdef _MSC_VER
    needed += snprintf(line,sizeof(line)," n%llu [label=\"t%u s%ld w%ld\"];\n", (unsigned long long)cur->id, cur->type_id, cur->strong, cur->weak);
#else
    needed += snprintf(line,sizeof(line)," n%llu [label=\"t%u s%u w%u\"];\n", (unsigned long long)cur->id, cur->type_id, atomic_load(&cur->strong), atomic_load(&cur->weak));
#endif
    RogueRcEdgeEnumFn efn=find_enum(cur->type_id); if(efn){ void *children[32]; size_t c=efn((void*)(cur+1),children,32); for(size_t i=0;i<c;i++){ RogueRcHeader *ch = hdr_from_obj(children[i]); if(ch) needed += snprintf(line,sizeof(line)," n%llu -> n%llu;\n", (unsigned long long)cur->id, (unsigned long long)ch->id); } }
        cur=cur->next; }
    live_unlock();
    if(!buf) return needed;
    size_t written=0;
#define APPEND_STR(s) do { const char *_s=(s); size_t _l=strlen(_s); if(written+_l<max){ memcpy(buf+written,_s,_l); written+=_l; } } while(0)
    APPEND_STR(header);
    live_lock(); cur=s_live_head; while(cur){
#ifdef _MSC_VER
    int l=snprintf(line,sizeof(line)," n%llu [label=\"t%u s%ld w%ld\"];\n", (unsigned long long)cur->id, cur->type_id, cur->strong, cur->weak);
#else
    int l=snprintf(line,sizeof(line)," n%llu [label=\"t%u s%u w%u\"];\n", (unsigned long long)cur->id, cur->type_id, atomic_load(&cur->strong), atomic_load(&cur->weak));
#endif
    if(written+(size_t)l<max){ memcpy(buf+written,line,l); written+=l; }
    RogueRcEdgeEnumFn efn=find_enum(cur->type_id); if(efn){ void *children[32]; size_t c=efn((void*)(cur+1),children,32); for(size_t i=0;i<c;i++){ RogueRcHeader *ch=hdr_from_obj(children[i]); if(ch){ int l2=snprintf(line,sizeof(line)," n%llu -> n%llu;\n", (unsigned long long)cur->id, (unsigned long long)ch->id); if(written+(size_t)l2<max){ memcpy(buf+written,line,l2); written+=l2; } } } }
        cur=cur->next; }
    live_unlock();
    APPEND_STR(footer);
    if(written<max) buf[written]='\0'; return written; }

size_t rogue_rc_snapshot(char *buf, size_t max){ size_t needed=0; char line[128]; live_lock(); RogueRcHeader *cur=s_live_head; while(cur){
#ifdef _MSC_VER
    needed += snprintf(line,sizeof(line),"%llu %u %ld %ld\n", (unsigned long long)cur->id, cur->type_id, cur->strong, cur->weak);
#else
    needed += snprintf(line,sizeof(line),"%llu %u %u %u\n", (unsigned long long)cur->id, cur->type_id, atomic_load(&cur->strong), atomic_load(&cur->weak));
#endif
    cur=cur->next; } live_unlock(); if(!buf) return needed; size_t written=0; live_lock(); cur=s_live_head; while(cur){
#ifdef _MSC_VER
    int l=snprintf(line,sizeof(line),"%llu %u %ld %ld\n", (unsigned long long)cur->id, cur->type_id, cur->strong, cur->weak);
#else
    int l=snprintf(line,sizeof(line),"%llu %u %u %u\n", (unsigned long long)cur->id, cur->type_id, atomic_load(&cur->strong), atomic_load(&cur->weak));
#endif
    if(written+(size_t)l<max){ memcpy(buf+written,line,l); written+=l; } cur=cur->next; } live_unlock(); if(written<max) buf[written]='\0'; return written; }

bool rogue_rc_validate(void)
{
    bool ok=true;
    uint64_t count=0;
    live_lock();
    RogueRcHeader *cur=s_live_head;
    while(cur){
#ifdef _MSC_VER
        if(cur->strong==0){ ok=false; break; }
#else
        if(atomic_load(&cur->strong)==0){ ok=false; break; }
#endif
        count++;
        cur=cur->next;
    }
    live_unlock();
#ifdef _MSC_VER
    if(count != (uint64_t)s_live_objects) ok=false;
#else
    if(count != atomic_load(&s_live_objects)) ok=false;
#endif
    return ok;
}

/* end of ref_count.c */
