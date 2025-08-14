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

static inline RogueRarityColor rogue_rarity_color(RogueItemRarity r){
    RogueRarityColor c; c.r=240; c.g=210; c.b=60; c.a=255; /* default common */
    switch(r){
        default: break;
        case ROGUE_ITEM_RARITY_UNCOMMON: c.r=80; c.g=220; c.b=80; c.a=255; break;
        case ROGUE_ITEM_RARITY_RARE: c.r=80; c.g=120; c.b=255; c.a=255; break;
        case ROGUE_ITEM_RARITY_EPIC: c.r=180; c.g=70; c.b=220; c.a=255; break;
        case ROGUE_ITEM_RARITY_LEGENDARY: c.r=255; c.g=140; c.b=0; c.a=255; break;
    }
    return c;
}

#endif
