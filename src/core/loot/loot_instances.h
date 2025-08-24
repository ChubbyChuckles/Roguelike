#ifndef ROGUE_LOOT_INSTANCES_H
#define ROGUE_LOOT_INSTANCES_H
#include "loot_item_defs.h"
#include <stdint.h>

#ifndef ROGUE_ITEM_INSTANCE_CAP
#define ROGUE_ITEM_INSTANCE_CAP 256
#endif

#ifndef ROGUE_ITEM_DESPAWN_MS
#define ROGUE_ITEM_DESPAWN_MS 60000 /* 60s default */
#endif

#ifndef ROGUE_ITEM_STACK_MERGE_RADIUS
#define ROGUE_ITEM_STACK_MERGE_RADIUS 0.45f
#endif

typedef struct RogueItemInstance
{
    int def_index; /* base item definition index */
    int quantity;  /* stack size */
    float x, y;    /* world tile position */
    float life_ms; /* for future despawn */
    int active;
    int rarity;     /* cached from definition for render color */
    int item_level; /* Phase 3.1: governs affix budget */
    /* Affix attachments (7.5): one prefix + one suffix for initial implementation */
    int prefix_index; /* -1 if none */
    int prefix_value; /* rolled stat value */
    int suffix_index; /* -1 if none */
    int suffix_value; /* rolled stat value */
    /* Durability (10.4 currency sink hook). Only meaningful for weapons/armor; others leave at 0.
     */
    int durability_cur;
    int durability_max;
    int fractured;     /* Phase 8.5: set to 1 when durability reaches 0; cleared on repair */
    int hidden_filter; /* 12.x loot filter: 1 if hidden by active filter */
    /* Phase 15.2 forward-compatible affix extension placeholder (enchant level). Legacy saves omit
     * this; reader defaults to 0. */
    int enchant_level;
    /* Phase 16.x personal loot ownership (-1 = unowned/shared) */
    int owner_player_id;
    /* Phase 5.1: Socketing – current socket count & occupied gem indices (-1 empty). For initial
     * slice, cap small (6). */
    int socket_count;
    int sockets[6];
    /* Phase 5.6: Protective seal flags (bitfield simplified to two ints). */
    int prefix_locked;
    int suffix_locked;
    /* Phase 10.4: Quality metric (0-20). Influences base damage/armor scaling. */
    int quality;
    /* Phase 10.2: Affix transfer orbs – store a single extracted affix temporarily. */
    int stored_affix_index; /* -1 none */
    int stored_affix_value; /* value of stored affix */
    int stored_affix_used;  /* 1 if orb already applied to a target */
    /* Phase 15.3: Anti-duplication GUID (unique per spawn). 64-bit for low collision probability.
     */
    unsigned long long guid;
    /* Phase 15.2: Hash chain of equipment state transitions (slot equips/unequips). Stored locally
     * for audit; server would recompute. */
    unsigned long long equip_hash_chain;
} RogueItemInstance;

void rogue_items_init_runtime(void);
void rogue_items_shutdown_runtime(void);
int rogue_items_spawn(int def_index, int quantity, float x, float y);
/* Runtime suppression flag for high-volume spawn logging (e.g., fuzz tests). 0=log, 1=suppress. */
extern int g_rogue_loot_suppress_spawn_log;
int rogue_items_active_count(void);
int rogue_items_visible_count(void); /* excludes hidden_filter instances */
void rogue_items_reapply_filter(
    void); /* re-evaluate all instances against current loot filter state */
void rogue_items_update(float dt_ms);

/* Affix APIs (7.5 / 7.6) */
int rogue_item_instance_generate_affixes(int inst_index, unsigned int* rng_state, int rarity);
const RogueItemInstance* rogue_item_instance_at(int index);
/* Phase 15.3: GUID helper */
unsigned long long rogue_item_instance_guid(int inst_index);
/* Phase 15.2: Hash chain getter (for audits) */
unsigned long long rogue_item_instance_equip_chain(int inst_index);
/* Derived damage range including affix flat damage bonuses */
int rogue_item_instance_damage_min(int inst_index);
int rogue_item_instance_damage_max(int inst_index);
/* Apply affix + rarity data to existing active instance (used for persistence load). */
int rogue_item_instance_apply_affixes(int inst_index, int rarity, int prefix_index,
                                      int prefix_value, int suffix_index, int suffix_value);

/* Phase 3: Power budget governance */
int rogue_budget_max(int item_level, int rarity); /* returns max allowed total affix weight */
int rogue_item_instance_total_affix_weight(int inst_index); /* sum of affix values */
int rogue_item_instance_validate_budget(int inst_index);    /* 0 ok, <0 over budget */
int rogue_item_instance_upgrade_level(
    int inst_index, int levels,
    unsigned int* rng_state); /* raises item_level and attempts to elevate affix values within new
                                 budget deterministically */

/* Durability APIs (currency sink for repair costs) */
int rogue_item_instance_get_durability(int inst_index, int* cur, int* max);
int rogue_item_instance_damage_durability(int inst_index, int amount); /* returns remaining */
int rogue_item_instance_repair_full(int inst_index);

/* Quality (Phase 10.4): query and adjust quality value (0-20). Returns new quality or <0 on error.
 */
int rogue_item_instance_get_quality(int inst_index);
int rogue_item_instance_set_quality(int inst_index, int quality);
int rogue_item_instance_improve_quality(int inst_index, int delta); /* clamps in range */

/* Phase 10.1 / 10.2 / 10.3 Crafting & Upgrade pipelines */
/* Apply an upgrade stone: increases item level by tiers and gently elevates affix values within new
 * budget. */
int rogue_item_instance_apply_upgrade_stone(int inst_index, int tiers, unsigned int* rng_state);
/* Extract an affix (prefix if is_prefix!=0 else suffix) into an empty orb instance; clears affix
 * from source. */
int rogue_item_instance_affix_extract(int inst_index, int is_prefix, int orb_inst_index);
/* Apply an affix orb to a target item (consumes orb one-time); fails if slot occupied or orb used.
 */
int rogue_item_instance_affix_orb_apply(int orb_inst_index, int target_inst_index);
/* Fuse (sacrifice) one item into target, transferring its highest-value affix if budget allows;
 * deactivates sacrifice. */
int rogue_item_instance_fusion(int target_inst_index, int sacrifice_inst_index);

/* Phase 5.1 Socket API (initial): query & set gem (simple index id). Returns 0 on success. */
int rogue_item_instance_socket_count(int inst_index);
int rogue_item_instance_get_socket(int inst_index, int slot); /* returns gem def index or -1 */
int rogue_item_instance_socket_insert(int inst_index, int slot,
                                      int gem_def_index);        /* fails if slot OOB or occupied */
int rogue_item_instance_socket_remove(int inst_index, int slot); /* clears slot */

#endif
