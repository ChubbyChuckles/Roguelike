/* XP award helper bridging to Event Bus (Phase 3.6.1) */
#ifndef ROGUE_PROGRESSION_AWARD_H
#define ROGUE_PROGRESSION_AWARD_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Awards player XP and publishes ROGUE_EVENT_XP_GAINED. Does not process level-up itself;
     * caller should invoke rogue_player_progress_update() in the main loop or test. */
    void rogue_award_xp(unsigned int xp_amount, unsigned int source_type, unsigned int source_id);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_PROGRESSION_AWARD_H */
