/* encounter_composer.h - Enemy Difficulty Phase 3 (Encounter Composition & Spawn Budget)
 */
#ifndef ROGUE_CORE_ENCOUNTER_COMPOSER_H
#define ROGUE_CORE_ENCOUNTER_COMPOSER_H
#ifdef __cplusplus
extern "C"
{
#endif

#define ROGUE_MAX_ENCOUNTER_TEMPLATES 64

    typedef enum RogueEncounterType
    {
        ROGUE_ENCOUNTER_SWARM = 0,
        ROGUE_ENCOUNTER_MIXED,
        ROGUE_ENCOUNTER_CHAMPION_PACK,
        ROGUE_ENCOUNTER_BOSS_ROOM
    } RogueEncounterType;

    typedef struct RogueEncounterTemplate
    {
        int id;
        char name[48];
        RogueEncounterType type;
        int min_count;
        int max_count;
        int boss;
        int support_min;
        int support_max;
        int elite_spacing;
        float elite_chance;
    } RogueEncounterTemplate;

    int rogue_encounters_load_file(const char* path);
    int rogue_encounter_template_count(void);
    const RogueEncounterTemplate* rogue_encounter_template_at(int index);
    const RogueEncounterTemplate* rogue_encounter_template_by_id(int id);

    /* Composition result */
    typedef struct RogueEncounterUnit
    {
        int enemy_type_id;
        int level;
        int is_elite;
    } RogueEncounterUnit;

    typedef struct RogueEncounterComposition
    {
        int template_id;
        int unit_count;
        RogueEncounterUnit units[64];
        int elite_count;
        int boss_present;
        int support_count;
    } RogueEncounterComposition;

    /* difficulty_rating ~ target enemy level; player_level for ΔL.
     * seed deterministic; biome_id reserved for future weighting.
     */
    int rogue_encounter_compose(int template_id, int player_level, int difficulty_rating,
                                int biome_id, unsigned int seed, RogueEncounterComposition* out);

#ifdef __cplusplus
}
#endif
#endif
