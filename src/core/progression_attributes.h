/* Attribute Allocation Layer (Phase 2)
 * Provides point spend / refund (re-spec) mechanics with deterministic journal hashing placeholder.
 */
#ifndef ROGUE_PROGRESSION_ATTRIBUTES_H
#define ROGUE_PROGRESSION_ATTRIBUTES_H

#include <stddef.h>

typedef struct RogueAttributeState {
    int strength;
    int dexterity;
    int vitality;
    int intelligence;
    int spent_points; /* total spent so far (not counting refunds) */
    int respec_tokens; /* available refund tokens */
    unsigned long long journal_hash; /* simplistic rolling hash over operations */
    /* Phase 12.3: operation journal for audit & migration replay (spend/respec). */
    struct RogueAttrOp* ops; /* dynamic array (allocated) */
    int op_count;
    int op_cap;
} RogueAttributeState;

/* Global singleton attribute state (Phase 12 persistence relies on this symbol). */
extern RogueAttributeState g_attr_state;

/* Initialize state from player base stats. */
void rogue_attr_state_init(RogueAttributeState* st, int str,int dex,int vit,int intl);

/* Spend one point into attribute by code: 'S','D','V','I'. Returns 0 success, <0 error (no points). Updates hash. */
int rogue_attr_spend(RogueAttributeState* st, char code);

/* Grant unspent allocation points (e.g., on level up). */
void rogue_attr_grant_points(int points);

/* Request re-spec (single point refund from attribute code) using a token. Returns 0 success. */
int rogue_attr_respec(RogueAttributeState* st, char code);

/* Global query for remaining unspent points (mirrors g_app.unspent_stat_points). */
int rogue_attr_unspent_points(void);

/* Deterministic fingerprint of attribute state (for tests). */
unsigned long long rogue_attr_fingerprint(const RogueAttributeState* st);

/* Attribute journal (Phase 12.3) accessors */
int rogue_attr_journal_count(void);
/* Returns 0 success; outputs code ('S','D','V','I') and kind (0=spend,1=respec). */
int rogue_attr_journal_entry(int index, char* code_out, int* kind_out);
/* Internal append helpers (exposed for persistence replay) */
void rogue_attr__journal_append(char code, int kind);

#endif
