#include "cow.h"
#include "ref_count.h" // reuse intrusive rc for shared pages
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#ifndef COW_DEFAULT_PAGE
#define COW_DEFAULT_PAGE 4096
#endif

// Page struct managed via ref_count for sharing.
typedef struct RogueCowPage {
    size_t size;      // bytes used (<= page_size for last page)
    // followed by raw bytes
} RogueCowPage;

struct RogueCowBuffer {
    size_t page_size;
    size_t length;        // logical size
    size_t page_count;
    RogueCowPage **pages; // array of pointers to page payload (after rc header)
};

static struct {
    uint64_t buffers_created;
    uint64_t pages_created;
    uint64_t cow_triggers;
    uint64_t page_copies;
    uint64_t dedup_hits;
    uint64_t serialize_linearizations;
} g_stats;

static RogueCowPage *alloc_page(size_t page_size, const void *src, size_t bytes){
    RogueCowPage *p = (RogueCowPage*)rogue_rc_alloc(sizeof(RogueCowPage)+page_size, 0xC0A401u, NULL);
    if(!p) return NULL;
    p->size = bytes;
    if(src && bytes) memcpy((unsigned char*)(p+1), src, bytes);
    return p;
}

RogueCowBuffer *rogue_cow_create_from_bytes(const void *data, size_t len, size_t chunk_size){
    if(chunk_size==0) chunk_size = COW_DEFAULT_PAGE;
    RogueCowBuffer *b = (RogueCowBuffer*)calloc(1,sizeof(RogueCowBuffer));
    if(!b) return NULL;
    b->page_size = chunk_size;
    b->length = len;
    b->page_count = (len + chunk_size - 1)/chunk_size;
    if(b->page_count==0) b->page_count = 1; // support empty buffer with one zero page
    b->pages = (RogueCowPage**)calloc(b->page_count,sizeof(RogueCowPage*));
    if(!b->pages){ free(b); return NULL; }
    for(size_t i=0;i<b->page_count;i++){
        size_t off = i*chunk_size;
        size_t remain = (off < len)? (len - off) : 0;
        size_t to_copy = remain>chunk_size?chunk_size:remain;
        RogueCowPage *p = alloc_page(chunk_size, data? (const unsigned char*)data+off : NULL, to_copy);
        if(!p){ rogue_cow_destroy(b); return NULL; }
        b->pages[i] = p;
        g_stats.pages_created++;
    }
    g_stats.buffers_created++;
    return b;
}

RogueCowBuffer *rogue_cow_clone(RogueCowBuffer *src){
    if(!src) return NULL;
    RogueCowBuffer *b = (RogueCowBuffer*)calloc(1,sizeof(RogueCowBuffer));
    if(!b) return NULL;
    b->page_size = src->page_size;
    b->length = src->length;
    b->page_count = src->page_count;
    b->pages = (RogueCowPage**)calloc(b->page_count,sizeof(RogueCowPage*));
    if(!b->pages){ free(b); return NULL; }
    for(size_t i=0;i<b->page_count;i++){
        b->pages[i] = src->pages[i];
        if(b->pages[i]) rogue_rc_retain(b->pages[i]);
    }
    g_stats.buffers_created++;
    return b;
}

void rogue_cow_destroy(RogueCowBuffer *buf){
    if(!buf) return;
    for(size_t i=0;i<buf->page_count;i++) if(buf->pages[i]) rogue_rc_release(buf->pages[i]);
    free(buf->pages);
    free(buf);
}

size_t rogue_cow_size(const RogueCowBuffer *buf){ return buf? buf->length : 0; }

int rogue_cow_read(const RogueCowBuffer *buf, size_t offset, void *out, size_t len){
    if(!buf || !out) return -1;
    if(offset+len > buf->length) return -1;
    unsigned char *dst = (unsigned char*)out;
    size_t page_sz = buf->page_size;
    while(len){
        size_t page_index = offset / page_sz;
        size_t in_page_off = offset % page_sz;
        size_t can = page_sz - in_page_off;
        if(can > len) can = len;
        RogueCowPage *p = buf->pages[page_index];
        size_t avail = p->size - in_page_off;
        if(in_page_off >= p->size) memset(dst,0,can); else memcpy(dst,(unsigned char*)(p+1)+in_page_off, can>avail?avail:can);
        if(can>avail && can>0) memset(dst+avail,0,can-avail);
        offset += can; dst += can; len -= can;
    }
    return 0;
}

// Ensure page is unique before write (copy if refcount>1)
static int ensure_unique_page(RogueCowBuffer *buf, size_t page_index){
    RogueCowPage *p = buf->pages[page_index];
    if(!p) return -1;
    if(rogue_rc_get_strong(p) > 1){
        g_stats.cow_triggers++;
        RogueCowPage *n = alloc_page(buf->page_size, p+1, p->size);
        if(!n) return -1;
        buf->pages[page_index] = n;
        rogue_rc_release(p);
        g_stats.page_copies++;
    }
    return 0;
}

int rogue_cow_write(RogueCowBuffer *buf, size_t offset, const void *src, size_t len){
    if(!buf || !src) return -1;
    if(offset+len > buf->length) return -1; // no implicit growth for now
    const unsigned char *s = (const unsigned char*)src;
    size_t page_sz = buf->page_size;
    while(len){
        size_t page_index = offset / page_sz;
        size_t in_page_off = offset % page_sz;
        size_t can = page_sz - in_page_off; if(can>len) can=len;
        if(ensure_unique_page(buf,page_index)!=0) return -1;
        RogueCowPage *p = buf->pages[page_index];
        size_t end = in_page_off + can;
        if(end > p->size) p->size = end; // extend last page logical size (not buffer length)
        memcpy((unsigned char*)(p+1)+in_page_off, s, can);
        offset += can; s += can; len -= can;
    }
    return 0;
}

// Simple FNV1a hash per page for dedup key
static uint64_t hash_page(const unsigned char *data, size_t sz){
    uint64_t h=1469598103934665603ULL; for(size_t i=0;i<sz;i++){ h ^= data[i]; h *= 1099511628211ULL; } return h;
}

typedef struct DedupEntry { uint64_t hash; RogueCowPage *page; } DedupEntry;

void rogue_cow_dedup(RogueCowBuffer *buf){
    if(!buf || buf->page_count<2) return;
    size_t cap = 1; while(cap < buf->page_count*2) cap <<=1; if(cap<8) cap=8;
    DedupEntry *table = (DedupEntry*)calloc(cap,sizeof(DedupEntry)); if(!table) return;
    for(size_t i=0;i<buf->page_count;i++){
        RogueCowPage *p = buf->pages[i]; if(!p) continue;
        size_t eff = p->size; if(eff==0) eff=1; // treat empty distinct
        uint64_t h = hash_page((unsigned char*)(p+1), eff);
        size_t mask = cap-1; size_t pos = h & mask; size_t first_free = (size_t)-1;
        while(1){
            if(table[pos].page==NULL){
                if(first_free==(size_t)-1) first_free=pos;
                break;
            }
            if(table[pos].hash==h){
                // compare bytes
                RogueCowPage *existing = table[pos].page;
                if(existing->size==p->size && memcmp(existing+1,p+1,p->size)==0){
                    // replace with existing
                    rogue_rc_retain(existing);
                    rogue_rc_release(p);
                    buf->pages[i]=existing;
                    g_stats.dedup_hits++;
                    goto next_page;
                }
            }
            pos = (pos+1)&mask;
        }
        // insert
        table[first_free].hash = h; table[first_free].page = p;
    next_page:;
    }
    free(table);
}

size_t rogue_cow_serialize(const RogueCowBuffer *buf, void *out, size_t max){
    if(!buf) return 0;
    g_stats.serialize_linearizations++;
    size_t needed = buf->length;
    if(!out) return needed;
    if(max==0) return needed;
    unsigned char *dst = (unsigned char*)out;
    size_t page_sz=buf->page_size; size_t remaining=buf->length; size_t offset=0;
    for(size_t i=0;i<buf->page_count && remaining; i++){
        RogueCowPage *p = buf->pages[i]; size_t copy = remaining>page_sz?page_sz:remaining; if(copy>p->size) copy=p->size;
        size_t can = copy; if(offset+can>max) can = max-offset; if(can>0) memcpy(dst+offset,p+1,can);
        if(can < copy && offset+can < max){ size_t pad = copy-can; if(offset+can+pad>max) pad = max-(offset+can); memset(dst+offset+can,0,pad); can += pad; }
        offset += can; remaining -= copy; if(offset>=max) break;
    }
    return needed;
}

RogueCowBuffer *rogue_cow_deserialize(const void *data, size_t len, size_t chunk_size){
    return rogue_cow_create_from_bytes(data,len,chunk_size);
}

void rogue_cow_get_stats(RogueCowStats *out){ if(!out) return; out->buffers_created=g_stats.buffers_created; out->pages_created=g_stats.pages_created; out->cow_triggers=g_stats.cow_triggers; out->page_copies=g_stats.page_copies; out->dedup_hits=g_stats.dedup_hits; out->serialize_linearizations=g_stats.serialize_linearizations; }

void rogue_cow_dump(RogueCowBuffer *buf, void *fptr){ FILE *f = fptr? (FILE*)fptr : stdout; if(!buf){ fprintf(f,"[cow] null buffer\n"); return; } fprintf(f,"[cow] size=%zu pages=%zu page_size=%zu\n", buf->length, buf->page_count, buf->page_size); for(size_t i=0;i<buf->page_count;i++){ RogueCowPage *p=buf->pages[i]; if(!p){ fprintf(f," page %zu: null\n",i); continue;} fprintf(f," page %zu: size=%zu ref=%u\n", i, p->size, rogue_rc_get_strong(p)); } }

uint32_t rogue_cow_page_refcount(RogueCowBuffer *buf, size_t page_index){ if(!buf || page_index>=buf->page_count) return 0xFFFFFFFFu; RogueCowPage *p=buf->pages[page_index]; return p? rogue_rc_get_strong(p):0; }
