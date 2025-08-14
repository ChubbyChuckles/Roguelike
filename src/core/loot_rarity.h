#ifndef ROGUE_LOOT_RARITY_H
#define ROGUE_LOOT_RARITY_H

typedef enum RogueItemRarity {
    ROGUE_ITEM_RARITY_COMMON = 0,
    ROGUE_ITEM_RARITY_UNCOMMON,
    ROGUE_ITEM_RARITY_RARE,
    ROGUE_ITEM_RARITY_EPIC,
    ROGUE_ITEM_RARITY_LEGENDARY,
    ROGUE_ITEM_RARITY__COUNT
} RogueItemRarity;

typedef struct RogueRarityColor { unsigned char r,g,b,a; } RogueRarityColor;
/* Returns color for rarity (0-4). Out of range yields common. */
RogueRarityColor rogue_rarity_color(RogueItemRarity r);

#endif
