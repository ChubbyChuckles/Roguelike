/* Tooling Phase 21.1: Simple tab-separated (TSV) spreadsheet -> game CSV converter.
   Allows designers to maintain a TSV (export from spreadsheet) where columns match item definition order.
   Converter trims whitespace, ignores blank/comment lines (#), and rewrites as comma-separated with same field count.
   Returns number of converted data lines or <0 on error. */
#ifndef ROGUE_LOOT_ITEM_DEFS_CONVERT_H
#define ROGUE_LOOT_ITEM_DEFS_CONVERT_H
int rogue_item_defs_convert_tsv_to_csv(const char* tsv_path, const char* out_csv_path);
#endif
