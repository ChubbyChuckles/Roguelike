#include "core/inventory_entries.h"
#include "core/save_manager.h" /* mark component dirty for incremental saves */
#include <string.h>
#include <stdlib.h>

typedef struct InvEntry { int def_index; uint64_t qty; unsigned labels; } InvEntry; /* labels metadata (Phase 1.3) */
static InvEntry* g_entries = NULL;
static unsigned g_entry_count = 0;
static unsigned g_entry_cap_soft = 1024; /* default soft cap (Phase 1.2) */
static unsigned g_entry_capacity = 0; /* allocated */
static RogueInventoryCapHandler g_cap_handler = NULL; /* Phase 1.7 */
/* Dirty tracking (Phase 1.6): when an entry's quantity changes or is created/removed we record its def_index in a bitmap-like sparse list (dedup). */
static int* g_dirty_indices = NULL; /* dynamic array of def_index */
static unsigned g_dirty_count = 0;
static unsigned g_dirty_cap = 0;

static void dirty_clear(void){ if(g_dirty_indices){ free(g_dirty_indices); g_dirty_indices=NULL; } g_dirty_count=0; g_dirty_cap=0; }
static void dirty_mark(int def_index){ if(def_index<0) return; /* dedup linear (entry count small typical); could optimize with hash later */
    for(unsigned i=0;i<g_dirty_count;i++) if(g_dirty_indices[i]==def_index) return; if(g_dirty_count==g_dirty_cap){ unsigned nc = g_dirty_cap? g_dirty_cap*2:64; int* nr=(int*)realloc(g_dirty_indices,sizeof(int)*nc); if(!nr) return; g_dirty_indices=nr; g_dirty_cap=nc; }
    g_dirty_indices[g_dirty_count++] = def_index;
    /* Mark save component dirty (Phase 1.6 persistence integration) */
    rogue_save_mark_component_dirty(ROGUE_SAVE_COMP_INV_ENTRIES);
}

int rogue_inventory_entries_init(void){ free(g_entries); g_entries=NULL; g_entry_count=0; g_entry_capacity=0; g_entry_cap_soft=1024; g_cap_handler=NULL; dirty_clear(); return 0; }
int rogue_inventory_set_unique_cap(unsigned cap){ g_entry_cap_soft = cap; return 0; }
unsigned rogue_inventory_unique_cap(void){ return g_entry_cap_soft; }
unsigned rogue_inventory_unique_count(void){ return g_entry_count; }

static int ensure_capacity(unsigned need){ if(need <= g_entry_capacity) return 0; unsigned new_cap = g_entry_capacity? g_entry_capacity*2:64; if(new_cap < need) new_cap = need; if(new_cap > ROGUE_INV_MAX_ENTRIES) new_cap = ROGUE_INV_MAX_ENTRIES; InvEntry* nr = (InvEntry*)realloc(g_entries, sizeof(InvEntry)*new_cap); if(!nr) return -1; g_entries = nr; g_entry_capacity = new_cap; return 0; }

static int find_entry(int def_index){ for(unsigned i=0;i<g_entry_count;i++) if(g_entries[i].def_index==def_index) return (int)i; return -1; }

uint64_t rogue_inventory_quantity(int def_index){ int idx = find_entry(def_index); return (idx>=0)? g_entries[idx].qty : 0; }

double rogue_inventory_entry_pressure(void){ if(g_entry_cap_soft==0 || g_entry_count==0) return 0.0; if(g_entry_count >= g_entry_cap_soft) return 1.0; return (double)g_entry_count / (double)g_entry_cap_soft; }

int rogue_inventory_can_accept(int def_index, uint64_t add_qty){ if(add_qty==0) return 0; int idx=find_entry(def_index); if(idx<0){ /* would create new distinct entry */ if(g_entry_count >= ROGUE_INV_MAX_ENTRIES) return ROGUE_INV_ERR_UNIQUE_CAP; if(g_entry_count >= g_entry_cap_soft) return ROGUE_INV_ERR_UNIQUE_CAP; }
    uint64_t cur = (idx>=0)? g_entries[idx].qty : 0; if(add_qty > UINT64_MAX - cur) return ROGUE_INV_ERR_OVERFLOW; return 0; }

int rogue_inventory_register_pickup(int def_index, uint64_t add_qty){ if(add_qty==0) return 0; int rc = rogue_inventory_can_accept(def_index, add_qty); if(rc==ROGUE_INV_ERR_UNIQUE_CAP && g_cap_handler){ /* attempt mitigation */
        int h = g_cap_handler(def_index, add_qty); if(h==0){ rc = rogue_inventory_can_accept(def_index, add_qty); }
    }
    if(rc!=0) return rc; int idx=find_entry(def_index); if(idx<0){ if(ensure_capacity(g_entry_count+1)!=0) return -1; g_entries[g_entry_count++] = (InvEntry){ def_index, add_qty, 0u }; dirty_mark(def_index); return 0; } uint64_t before=g_entries[idx].qty; g_entries[idx].qty += add_qty; if(g_entries[idx].qty!=before) dirty_mark(def_index); return 0; }

int rogue_inventory_register_remove(int def_index, uint64_t remove_qty){ if(remove_qty==0) return 0; int idx=find_entry(def_index); if(idx<0) return -1; if(remove_qty > g_entries[idx].qty) return -1; uint64_t before = g_entries[idx].qty; g_entries[idx].qty -= remove_qty; if(g_entries[idx].qty!=before) dirty_mark(def_index); if(g_entries[idx].qty==0){ /* remove by swap-back */ unsigned last = g_entry_count-1; if(idx != (int)last) g_entries[idx] = g_entries[last]; g_entry_count--; dirty_mark(def_index); }
    return 0; }

int rogue_inventory_entry_set_labels(int def_index, unsigned labels){ int idx=find_entry(def_index); if(idx<0) return -1; g_entries[idx].labels = labels; return 0; }
unsigned rogue_inventory_entry_labels(int def_index){ int idx=find_entry(def_index); if(idx<0) return 0; return g_entries[idx].labels; }
int rogue_inventory_set_cap_handler(RogueInventoryCapHandler fn){ g_cap_handler = fn; return 0; }
unsigned rogue_inventory_entries_dirty_pairs(int* out_def_indices, uint64_t* out_quantities, unsigned cap){ if(!out_def_indices || !out_quantities){ /* reset baseline */ dirty_clear(); return 0; }
    unsigned n = (g_dirty_count<cap)? g_dirty_count:cap; for(unsigned i=0;i<n;i++){ int def_index = g_dirty_indices[i]; out_def_indices[i]=def_index; /* find quantity (may be absent -> 0) */ int idx=find_entry(def_index); out_quantities[i] = (idx>=0)? g_entries[idx].qty : 0ull; }
    /* After enumeration we clear to treat current state as baseline. */ dirty_clear(); return n; }
void rogue_inventory_entries_clear_dirty(void){ dirty_clear(); }
