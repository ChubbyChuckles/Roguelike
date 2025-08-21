/* Equipment Phase 4.2: Unique item registry & stat hook layer */
#ifndef ROGUE_EQUIPMENT_UNIQUES_H
#define ROGUE_EQUIPMENT_UNIQUES_H

#define ROGUE_UNIQUE_ID_MAX 32
#define ROGUE_UNIQUE_CAP 64

typedef struct RogueUniqueDef {
	char id[ROGUE_UNIQUE_ID_MAX];          /* unique identifier (for tooling) */
	char base_item_id[ROGUE_UNIQUE_ID_MAX];/* base item definition id this unique augments (must exist) */
	/* Fixed stat bonuses applied to unique_* layer (primary attributes) */
	int strength, dexterity, vitality, intelligence;
	int armor_flat; /* extra flat armor */
	int resist_physical,resist_fire,resist_cold,resist_lightning,resist_poison,resist_status;
	int hook_id; /* future behavior hook dispatch id (0 = none) */
} RogueUniqueDef;

int rogue_unique_register(const RogueUniqueDef* def); /* returns index or <0 */
int rogue_unique_count(void);
const RogueUniqueDef* rogue_unique_at(int index);
/* Lookup by base item definition index (fast path uses base id match) */
int rogue_unique_find_by_base_def(int def_index);

#endif
