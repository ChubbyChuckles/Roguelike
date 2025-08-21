#include "core/loot/loot_rarity.h"
RogueRarityColor rogue_rarity_color(RogueItemRarity r)
{
    RogueRarityColor c;
    c.r = 240;
    c.g = 210;
    c.b = 60;
    c.a = 255;
    switch (r)
    {
    case ROGUE_ITEM_RARITY_UNCOMMON:
        c.r = 80;
        c.g = 220;
        c.b = 80;
        break;
    case ROGUE_ITEM_RARITY_RARE:
        c.r = 80;
        c.g = 120;
        c.b = 255;
        break;
    case ROGUE_ITEM_RARITY_EPIC:
        c.r = 180;
        c.g = 70;
        c.b = 220;
        break;
    case ROGUE_ITEM_RARITY_LEGENDARY:
        c.r = 255;
        c.g = 140;
        c.b = 0;
        break;
    default:
        break;
    }
    return c;
}
