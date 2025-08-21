/* Loot console utilities: histogram & telemetry export (6.4, 6.5) */
#ifndef ROGUE_LOOT_CONSOLE_H
#define ROGUE_LOOT_CONSOLE_H

/* Format rarity histogram into buffer. Returns written length or -1. */
int rogue_loot_histogram_format(char* out, int out_sz);
/* Export telemetry snapshot (rarity counts + dynamic rarity factors) as JSON to path. Returns 0 on success. */
int rogue_loot_export_telemetry(const char* path);

#endif
