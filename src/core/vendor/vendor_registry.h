/* Vendor System Phase 1 data registries: vendor definitions, price policies, reputation tiers.
    Updated: JSON support (prefers vendors/*.json over legacy .cfg flat files). */
#ifndef ROGUE_VENDOR_REGISTRY_H
#define ROGUE_VENDOR_REGISTRY_H
#ifdef __cplusplus
extern "C"
{
#endif

#define ROGUE_MAX_VENDOR_DEFS 32
#define ROGUE_MAX_PRICE_POLICIES 16
#define ROGUE_MAX_REP_TIERS 16
#define ROGUE_MAX_NEGOTIATION_RULES 16

    typedef struct RogueVendorDef
    {
        char id[32];
        char archetype[32];
        char biome_tags[64];
        int refresh_interval_ms;
        int price_policy_index; /* index into policy array (-1 if none) */
    } RogueVendorDef;

    typedef struct RoguePricePolicy
    {
        char id[32];
        int base_buy_margin;  /* percent e.g. 120 means 1.2x baseline when buying from vendor */
        int base_sell_margin; /* percent of baseline paid to player */
        int rarity_mods[5];   /* percent scaling each rarity */
        int category_mods[6]; /* percent scaling each category 0..5 */
    } RoguePricePolicy;

    typedef struct RogueRepTier
    {
        char id[32];
        int rep_min;
        int buy_discount_pct; /* % reduction in buy price */
        int sell_bonus_pct;   /* % increase in sell price */
        char unlock_tags[64];
    } RogueRepTier;

    typedef struct RogueNegotiationRule
    {
        char id[32];
        char skill_checks[64]; /* space separated skill tags */
        int min_roll;          /* threshold to succeed */
        int discount_min_pct;
        int discount_max_pct; /* inclusive range; future may add variable scaling */
    } RogueNegotiationRule;

    int
    rogue_vendor_registry_load_all(void); /* loads JSON (vendors.json, price_policies.json,
                                             reputation_tiers.json) or falls back to legacy .cfg */

    int rogue_vendor_def_count(void);
    const RogueVendorDef* rogue_vendor_def_at(int idx);
    const RogueVendorDef* rogue_vendor_def_find(const char* id);

    int rogue_price_policy_count(void);
    const RoguePricePolicy* rogue_price_policy_at(int idx);
    const RoguePricePolicy* rogue_price_policy_find(const char* id);

    int rogue_rep_tier_count(void);
    const RogueRepTier* rogue_rep_tier_at(int idx);
    const RogueRepTier* rogue_rep_tier_find(const char* id);

    int rogue_negotiation_rule_count(void);
    const RogueNegotiationRule* rogue_negotiation_rule_at(int idx);
    const RogueNegotiationRule* rogue_negotiation_rule_find(const char* id);

#ifdef __cplusplus
}
#endif
#endif /* ROGUE_VENDOR_REGISTRY_H */
