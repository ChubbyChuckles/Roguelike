#ifndef ROGUE_CORE_PLAYER_DEBUG_H
#define ROGUE_CORE_PLAYER_DEBUG_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RoguePlayerStatKey
    {
        ROGUE_STAT_STRENGTH = 0,
        ROGUE_STAT_DEXTERITY = 1,
        ROGUE_STAT_VITALITY = 2,
        ROGUE_STAT_INTELLIGENCE = 3,
    } RoguePlayerStatKey;

    /* Basic resource getters/setters */
    int rogue_player_debug_get_health(void);
    int rogue_player_debug_get_max_health(void);
    void rogue_player_debug_set_health(int v);
    int rogue_player_debug_get_mana(void);
    int rogue_player_debug_get_max_mana(void);
    void rogue_player_debug_set_mana(int v);
    int rogue_player_debug_get_ap(void);
    int rogue_player_debug_get_max_ap(void);
    void rogue_player_debug_set_ap(int v);

    /* Core stats */
    int rogue_player_debug_get_stat(RoguePlayerStatKey k);
    void rogue_player_debug_set_stat(RoguePlayerStatKey k, int v);

    /* Player flags */
    int rogue_player_debug_get_god_mode(void);
    void rogue_player_debug_set_god_mode(int enabled);
    int rogue_player_debug_get_noclip(void);
    void rogue_player_debug_set_noclip(int enabled);

    /* Teleport */
    void rogue_player_debug_teleport(float x, float y);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_PLAYER_DEBUG_H */
