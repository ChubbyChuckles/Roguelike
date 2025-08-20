/* Phase 5 Implementation: Buyback Ring Buffer, Depreciation, Assimilation */
#include "core/vendor_buyback.h"
#include "core/vendor_pricing.h"
#include "core/vendor_registry.h"
#include "core/loot_instances.h"
#include <string.h>

#ifndef ROGUE_VENDOR_MAX
#define ROGUE_VENDOR_MAX 32
#endif

/* Simple per-vendor ring buffer */
typedef struct VendorBuybackState {
    RogueVendorBuybackEntry entries[ROGUE_VENDOR_BUYBACK_CAP];
    int head; /* next insert */
    int count;
} VendorBuybackState;

static VendorBuybackState g_states[ROGUE_VENDOR_MAX];
static int g_inited = 0;

static void ensure_init(void){ if(g_inited) return; memset(g_states,0,sizeof(g_states)); g_inited=1; }

void rogue_vendor_buyback_reset(void){ g_inited=0; ensure_init(); }

static unsigned int depreciation_interval_ms(void){ return 60u*1000u; } /* 1 minute step baseline */
static float depreciation_per_interval(void){ return 0.10f; } /* loses 10% of vendor resale price per interval */
static unsigned int assimilation_delay_ms(void){ return 5u*60u*1000u; } /* 5 minutes then assimilated */

/* Compute depreciated resale price (vendor -> player) starting from original_price with floor at 50% */
int rogue_vendor_buyback_current_price(const RogueVendorBuybackEntry* e, unsigned int now_ms){ if(!e||!e->active) return -1; unsigned int age_ms = now_ms - e->sell_time_ms; unsigned int intervals = depreciation_interval_ms()? (age_ms / depreciation_interval_ms()) : 0; float price = (float)e->original_price; for(unsigned int i=0;i<intervals;i++){ price *= (1.0f - depreciation_per_interval()); if(price < e->original_price * 0.5f) { price = e->original_price * 0.5f; break; } }
    int final_price = (int)price; if(final_price<1) final_price=1; return final_price; }

static void maybe_assimilate_entry(int vendor_def_index, RogueVendorBuybackEntry* e, unsigned int now_ms){ (void)vendor_def_index; if(!e->active) return; if(now_ms >= e->assimilate_time_ms){ /* for now we simply mark inactive (assimilation logic could insert into stock) */ e->active=0; /* TODO Phase 6+ could push into vendor inventory */ }
}

/* forward declare recent guid note to silence implicit warning (defined later) */
static void recent_guid_note(unsigned long long g);

void rogue_vendor_buyback_tick(int vendor_def_index, unsigned int now_ms){ ensure_init(); if(vendor_def_index<0||vendor_def_index>=ROGUE_VENDOR_MAX) return; VendorBuybackState* st=&g_states[vendor_def_index]; for(int i=0;i<ROGUE_VENDOR_BUYBACK_CAP;i++){ maybe_assimilate_entry(vendor_def_index, &st->entries[i], now_ms); } }

int rogue_vendor_buyback_record(int vendor_def_index, unsigned long long item_guid, int item_def_index,
                                int rarity, int category, int condition_pct, int price, unsigned int now_ms){ ensure_init(); if(vendor_def_index<0||vendor_def_index>=ROGUE_VENDOR_MAX) return -1; VendorBuybackState* st=&g_states[vendor_def_index]; if(st->head<0||st->head>=ROGUE_VENDOR_BUYBACK_CAP) st->head=0; int slot = st->head; st->head = (st->head+1)%ROGUE_VENDOR_BUYBACK_CAP; if(st->count<ROGUE_VENDOR_BUYBACK_CAP) st->count++; RogueVendorBuybackEntry* e=&st->entries[slot]; e->item_guid=item_guid; e->item_def_index=item_def_index; e->rarity=rarity; e->category=category; e->condition_pct=condition_pct; e->original_price=price; e->sell_time_ms=now_ms; e->assimilate_time_ms = now_ms + assimilation_delay_ms(); e->active=1; recent_guid_note(item_guid); return slot; }

int rogue_vendor_buyback_list(int vendor_def_index, RogueVendorBuybackEntry* out, int max_out, unsigned int now_ms){ ensure_init(); if(vendor_def_index<0||vendor_def_index>=ROGUE_VENDOR_MAX) return 0; VendorBuybackState* st=&g_states[vendor_def_index]; int written=0; for(int i=0;i<ROGUE_VENDOR_BUYBACK_CAP;i++){ RogueVendorBuybackEntry* e=&st->entries[i]; if(!e->active) continue; maybe_assimilate_entry(vendor_def_index,e,now_ms); if(!e->active) continue; if(out && written<max_out) out[written]=*e; written++; }
    return written; }

int rogue_vendor_buyback_purchase(int vendor_def_index, unsigned long long item_guid, unsigned int now_ms){ ensure_init(); if(vendor_def_index<0||vendor_def_index>=ROGUE_VENDOR_MAX) return -1; VendorBuybackState* st=&g_states[vendor_def_index]; for(int i=0;i<ROGUE_VENDOR_BUYBACK_CAP;i++){ RogueVendorBuybackEntry* e=&st->entries[i]; if(!e->active) continue; if(e->item_guid==item_guid){ maybe_assimilate_entry(vendor_def_index,e,now_ms); if(!e->active) return -1; int price = rogue_vendor_buyback_current_price(e, now_ms); e->active=0; return price; } } return -1; }

/* GUID recent set: simplistic fixed-size ring for exploitation detection */
#ifndef ROGUE_VENDOR_BUYBACK_GUID_RECENT_CAP
#define ROGUE_VENDOR_BUYBACK_GUID_RECENT_CAP 128
#endif
static unsigned long long g_recent_guids[ROGUE_VENDOR_BUYBACK_GUID_RECENT_CAP];
static int g_recent_head=0; static int g_recent_count=0;

static void recent_guid_note(unsigned long long g){ for(int i=0;i<g_recent_count;i++){ if(g_recent_guids[i]==g) return; } g_recent_guids[g_recent_head]=g; g_recent_head=(g_recent_head+1)%ROGUE_VENDOR_BUYBACK_GUID_RECENT_CAP; if(g_recent_count<ROGUE_VENDOR_BUYBACK_GUID_RECENT_CAP) g_recent_count++; }
int rogue_vendor_buyback_guid_recent(unsigned long long guid){ for(int i=0;i<g_recent_count;i++){ if(g_recent_guids[i]==guid) return 1; } return 0; }

/* Hook for transaction journal from this module (record sale and buyback) */
void rogue_vendor_tx_journal_record(int vendor_def_index, unsigned long long item_guid, int action_code, int price, int rep_delta, int discount_pct); /* forward (implemented in tx journal module) */

/* Helper API for external sale recording integrating journal */
int rogue_vendor_buyback_record_with_journal(int vendor_def_index, unsigned long long guid, int item_def_index, int rarity, int category, int condition_pct, int price, unsigned int now_ms, int rep_delta){ int slot = rogue_vendor_buyback_record(vendor_def_index,guid,item_def_index,rarity,category,condition_pct,price,now_ms); rogue_vendor_tx_journal_record(vendor_def_index,guid,1 /* sale code */, price, rep_delta, 0); return slot; }

int rogue_vendor_buyback_purchase_with_journal(int vendor_def_index, unsigned long long guid, unsigned int now_ms, int rep_delta){ int price = rogue_vendor_buyback_purchase(vendor_def_index,guid,now_ms); if(price>=0){ rogue_vendor_tx_journal_record(vendor_def_index,guid,2 /* buyback code */, price, rep_delta, 0); } return price; }
