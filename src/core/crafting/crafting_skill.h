/* Crafting Skill & Proficiency (Phase 5.1–5.5)
 * Disciplines: smithing, alchemy, enchanting, cooking (placeholder)
 * Features:
 *  - XP & level curves per discipline
 *  - Perks: material cost reduction, craft speed bonus, duplicate output chance
 *  - Success modifiers hook (quality floor) – placeholder API
 *  - Recipe discovery bitset with dependency unlock (output -> other recipe input)
 */
#ifndef ROGUE_CRAFTING_SKILL_H
#define ROGUE_CRAFTING_SKILL_H

#ifdef __cplusplus
extern "C"
{
#endif

    enum RogueCraftDiscipline
    {
        ROGUE_CRAFT_DISC_SMITHING = 0,
        ROGUE_CRAFT_DISC_ALCHEMY = 1,
        ROGUE_CRAFT_DISC_ENCHANTING = 2,
        ROGUE_CRAFT_DISC_COOKING = 3,
        ROGUE_CRAFT_DISC__COUNT
    };

    void rogue_craft_skill_reset(void);
    void rogue_craft_skill_gain(enum RogueCraftDiscipline disc, int xp);
    int rogue_craft_skill_xp(enum RogueCraftDiscipline disc);
    int rogue_craft_skill_level(enum RogueCraftDiscipline disc);
    int rogue_craft_skill_xp_to_next(enum RogueCraftDiscipline disc);

    /* Perk query helpers (values in percent or scalar *100 to keep ints) */
    int rogue_craft_perk_material_cost_pct(
        enum RogueCraftDiscipline disc); /* e.g., 90 = 10% reduction */
    int rogue_craft_perk_speed_pct(
        enum RogueCraftDiscipline disc); /* e.g., 80 = 20% faster (time * 80/100) */
    int rogue_craft_perk_duplicate_chance_pct(
        enum RogueCraftDiscipline disc); /* 0..100 deterministic chance */

    /* Quality floor modifier placeholder (returns added floor percent 0..100) */
    int rogue_craft_quality_floor_bonus(enum RogueCraftDiscipline disc);

    /* Recipe discovery */
    void rogue_craft_discovery_reset(void);
    int rogue_craft_recipe_is_discovered(int recipe_index);
    void rogue_craft_recipe_mark_discovered(int recipe_index);
    /* After crafting a recipe, call to unlock dependent recipes whose inputs include the crafted
     * output. */
    void rogue_craft_discovery_unlock_dependencies(int crafted_recipe_index);

    /* Map station id to discipline (forge/workbench->smithing, alchemy_table->alchemy,
     * mystic_altar->enchanting) */
    enum RogueCraftDiscipline rogue_craft_station_discipline(int station_id);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CRAFTING_SKILL_H */
