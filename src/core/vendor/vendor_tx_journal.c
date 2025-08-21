#include "core/vendor/vendor_tx_journal.h"
#include <string.h>

static RogueVendorTxEntry g_entries[ROGUE_VENDOR_TX_JOURNAL_CAP];
static int g_entry_count=0; static unsigned int g_accum_hash=0x811C9DC5u;

static unsigned int fnv1a_step(unsigned int h, unsigned int v){ h ^= v; h *= 0x01000193u; return h; }

void rogue_vendor_tx_journal_reset(void){ g_entry_count=0; g_accum_hash=0x811C9DC5u; }

int rogue_vendor_tx_journal_append(unsigned int timestamp_ms, int vendor_def_index, int action_code, unsigned long long item_guid, unsigned int price, int rep_delta, int discount_pct){ if(g_entry_count>=ROGUE_VENDOR_TX_JOURNAL_CAP) return -1; RogueVendorTxEntry e; e.op_id=(unsigned int)g_entry_count; e.timestamp_ms=timestamp_ms; e.vendor_id=(unsigned int)(vendor_def_index<0?0u:(unsigned int)vendor_def_index); e.action_code=(unsigned int)action_code; e.item_guid_low=(unsigned int)(item_guid & 0xFFFFFFFFu); e.price=price; e.rep_delta=(unsigned int)(rep_delta<0? (unsigned int)(-rep_delta) : (unsigned int)rep_delta); e.discount_pct=(unsigned int)(discount_pct<0?0:discount_pct>100?100:discount_pct); g_entries[g_entry_count++]=e; unsigned int h=g_accum_hash; h=fnv1a_step(h,e.op_id); h=fnv1a_step(h,e.timestamp_ms); h=fnv1a_step(h,e.vendor_id); h=fnv1a_step(h,e.action_code); h=fnv1a_step(h,e.item_guid_low); h=fnv1a_step(h,e.price); h=fnv1a_step(h,e.rep_delta); h=fnv1a_step(h,e.discount_pct); g_accum_hash=h; return (int)e.op_id; }

int rogue_vendor_tx_journal_count(void){ return g_entry_count; }
const RogueVendorTxEntry* rogue_vendor_tx_journal_entry(int index){ if(index<0||index>=g_entry_count) return NULL; return &g_entries[index]; }
unsigned int rogue_vendor_tx_journal_accum_hash(void){ return g_accum_hash; }

void rogue_vendor_tx_journal_record(int vendor_def_index, unsigned long long item_guid, int action_code, int price, int rep_delta, int discount_pct){ /* timestamp not integrated with real clock yet; using op_id*10 as synthetic placeholder */ unsigned int ts = (unsigned int)(g_entry_count*10u); rogue_vendor_tx_journal_append(ts,vendor_def_index,action_code,item_guid,(unsigned int)price,rep_delta,discount_pct); }

int rogue_vendor_tx_journal_compact_summary(RogueVendorTxCompactionSummary* out_summary) {
	if (!out_summary || g_entry_count == 0) return -1;
	memset(out_summary, 0, sizeof(RogueVendorTxCompactionSummary));
	out_summary->first_timestamp_ms = g_entries[0].timestamp_ms;
	out_summary->last_timestamp_ms = g_entries[g_entry_count-1].timestamp_ms;
	for (int i = 0; i < g_entry_count; ++i) {
		const RogueVendorTxEntry* e = &g_entries[i];
		if (e->action_code == 1) { // sale
			out_summary->total_sales++;
			out_summary->total_gold_sold += e->price;
		} else if (e->action_code == 2) { // buyback
			out_summary->total_buybacks++;
			out_summary->total_gold_bought += e->price;
		} else if (e->action_code == 3) {
			out_summary->total_assimilated++;
		}
		out_summary->total_rep_delta += e->rep_delta;
	}
	return 0;
}
