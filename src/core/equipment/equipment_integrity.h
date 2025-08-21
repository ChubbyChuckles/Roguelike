/* Phase 15: Multiplayer Integrity & Anti-Cheat helpers (15.4–15.6) */
#ifndef ROGUE_EQUIPMENT_INTEGRITY_H
#define ROGUE_EQUIPMENT_INTEGRITY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RogueProcAnomaly {
    int proc_id;
    float triggers_per_min;
} RogueProcAnomaly;
int rogue_integrity_scan_proc_anomalies(RogueProcAnomaly* out, int max_out, float max_tpm);

void rogue_integrity_clear_banned_affix_pairs(void);
int  rogue_integrity_add_banned_affix_pair(int affix_a, int affix_b);
int  rogue_integrity_is_item_banned(int inst_index);

typedef struct RogueItemChainMismatch {
    int inst_index;
    unsigned long long stored_chain;
    unsigned long long expected_chain;
} RogueItemChainMismatch;
unsigned long long rogue_integrity_expected_item_equip_hash(int inst_index);
int rogue_integrity_scan_equip_chain_mismatches(RogueItemChainMismatch* out, int max_out);
int rogue_integrity_scan_duplicate_guids(int* out, int max_out);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_EQUIPMENT_INTEGRITY_H */
