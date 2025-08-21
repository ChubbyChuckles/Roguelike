#ifndef ROGUE_GAME_ARMOR_H
#define ROGUE_GAME_ARMOR_H

/* Phase 7.2 Armor Weight Classes */

typedef enum RogueArmorSlot
{
    ROGUE_ARMOR_HEAD = 0,
    ROGUE_ARMOR_CHEST = 1,
    ROGUE_ARMOR_LEGS = 2,
    ROGUE_ARMOR_HANDS = 3,
    ROGUE_ARMOR_FEET = 4,
    ROGUE_ARMOR_SLOT_COUNT
} RogueArmorSlot;

typedef struct RogueArmorDef
{
    int id;
    const char* name;
    int weight_class; /* 0=light,1=medium,2=heavy */
    float weight;     /* contributes to encumbrance */
    int armor;        /* flat */
    int resist_physical;
    int resist_fire;
    int resist_frost;
    int resist_arcane;
    float poise_bonus;        /* added to poise_max */
    float stamina_regen_mult; /* multiplicative regen modifier */
} RogueArmorDef;

const RogueArmorDef* rogue_armor_get(int id);
void rogue_armor_equip_slot(int slot, int armor_id);
int rogue_armor_get_slot(int slot);
struct RoguePlayer; /* forward declare to avoid full include */
void rogue_armor_recalc_player(struct RoguePlayer* p);

#endif
