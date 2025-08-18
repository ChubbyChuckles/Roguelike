/* Equipment content authoring tooling (Phase 16.2): set & runeword definition registry + JSON tooling */
#ifndef ROGUE_EQUIPMENT_CONTENT_H
#define ROGUE_EQUIPMENT_CONTENT_H

#include <stdint.h>

typedef struct RogueSetBonus { int pieces; int strength,dexterity,vitality,intelligence; int armor_flat; int resist_fire,resist_cold,resist_light,resist_poison,resist_status,resist_physical; } RogueSetBonus;
typedef struct RogueSetDef { int set_id; int bonus_count; RogueSetBonus bonuses[4]; } RogueSetDef;
int rogue_set_register(const RogueSetDef* def); /* returns index or <0 */
const RogueSetDef* rogue_set_at(int index);
int rogue_set_count(void);
const RogueSetDef* rogue_set_find(int set_id);

typedef struct RogueRuneword { char pattern[12]; int strength,dexterity,vitality,intelligence; int armor_flat; int resist_fire,resist_cold,resist_light,resist_poison,resist_status,resist_physical; } RogueRuneword;
int rogue_runeword_register(const RogueRuneword* rw);
const RogueRuneword* rogue_runeword_at(int index);
int rogue_runeword_count(void);
const RogueRuneword* rogue_runeword_find(const char* pattern);

/* Testing / tooling support: reset registries (non-thread-safe) */
void rogue_sets_reset(void);
void rogue_runewords_reset(void);

/* Phase 16.2: Live preview computation helper: given counts for a set id, compute aggregated bonus contribution (linear interpolation) into out stats. */
/* Preview applies interpolated stats (including partial toward first threshold). */
void rogue_set_preview_apply(int set_id, int equipped_count, int* strength,int* dexterity,int* vitality,int* intelligence,int* armor_flat,int* r_fire,int* r_cold,int* r_light,int* r_poison,int* r_status,int* r_phys);

/* Phase 16.2: JSON tooling for sets (array of { set_id, bonuses:[ { pieces,...stats } ] }) */
int rogue_sets_load_from_json(const char* path); /* returns sets added */
int rogue_sets_export_json(char* buf,int cap); /* returns chars written or -1 */

/* Basic validation: ensure strictly increasing pieces thresholds and non-negative stats. Returns 0 ok else <0. */
int rogue_set_validate(const RogueSetDef* def);

#endif
