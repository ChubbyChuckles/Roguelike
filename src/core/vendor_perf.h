/* Phase 11 Performance & Memory: SoA state + batch refresh scheduler */
#ifndef ROGUE_VENDOR_PERF_H
#define ROGUE_VENDOR_PERF_H
#ifdef __cplusplus
extern "C" { 
#endif

#include <stddef.h>

void rogue_vendor_perf_reset(void);
void rogue_vendor_perf_init(int vendor_count);
int  rogue_vendor_perf_vendor_count(void);
size_t rogue_vendor_perf_memory_bytes(void);
void rogue_vendor_perf_note_sale(int vendor_index);
void rogue_vendor_perf_note_buyback(int vendor_index);
float rogue_vendor_perf_demand_score(int vendor_index);
float rogue_vendor_perf_scarcity_score(int vendor_index);
void rogue_vendor_perf_scheduler_config(int slice_size);
int  rogue_vendor_perf_scheduler_tick(int tick_id);
int  rogue_vendor_perf_last_refresh_tick(int vendor_index);

#ifdef __cplusplus
}
#endif
#endif /* ROGUE_VENDOR_PERF_H */
