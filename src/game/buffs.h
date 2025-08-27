/* Timed buff system */
#ifndef ROGUE_CORE_BUFFS_H
#define ROGUE_CORE_BUFFS_H
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>

    typedef enum RogueBuffType
    {
        ROGUE_BUFF_POWER_STRIKE = 0,
        ROGUE_BUFF_STAT_STRENGTH = 1,
        ROGUE_BUFF_MAX
    } RogueBuffType;

    /* Stacking behavior (Phase 10.3) */
    typedef enum RogueBuffStackRule
    {
        ROGUE_BUFF_STACK_UNIQUE = 0,  /* if active: reject */
        ROGUE_BUFF_STACK_REFRESH = 1, /* reset duration, keep highest magnitude */
        ROGUE_BUFF_STACK_EXTEND = 2,  /* add duration (clamped by max) */
        ROGUE_BUFF_STACK_ADD = 3,     /* additive magnitude + extend if longer */
        ROGUE_BUFF_STACK_MULTIPLY =
            4, /* multiply magnitude by incoming percent (100 = no change) */
        ROGUE_BUFF_STACK_REPLACE_IF_STRONGER =
            5 /* replace magnitude if stronger; refresh duration if longer */
    } RogueBuffStackRule;

    typedef struct RogueBuff
    {
        int active;
        RogueBuffType type;
        double end_ms;
        int magnitude;
        int snapshot; /* 1 if magnitude snapshot (does not change after apply even if base stats
                         change) */
        RogueBuffStackRule stack_rule;
        double last_apply_ms; /* for dampening */
        /* Handle pool internals (Phase 4.1): generation for ABA-safety. */
        uint16_t _gen;
        int _next_free;
    } RogueBuff;

    /* Phase 4.1–4.2: Handle-based pool API */
    typedef uint32_t RogueBuffHandle;
    enum
    {
        ROGUE_BUFF_INVALID_HANDLE = 0
    };

    void rogue_buffs_init(void);
    void rogue_buffs_update(double now_ms);
    int rogue_buffs_apply(RogueBuffType type, int magnitude, double duration_ms, double now_ms,
                          RogueBuffStackRule rule, int snapshot);
    /* Handle variants (return the affected/newly-created buff handle, or INVALID on failure). */
    RogueBuffHandle rogue_buffs_apply_h(RogueBuffType type, int magnitude, double duration_ms,
                                        double now_ms, RogueBuffStackRule rule, int snapshot);
    int rogue_buffs_refresh_h(RogueBuffHandle h, int magnitude, double duration_ms, double now_ms,
                              RogueBuffStackRule rule, int snapshot);
    int rogue_buffs_remove_h(RogueBuffHandle h, double now_ms);
    int rogue_buffs_query_h(RogueBuffHandle h, RogueBuff* out);
    /* Combined scalar effect for strength buffs (Phase 10.1 integration). */
    int rogue_buffs_strength_bonus(void);
    /* Anti-oscillation dampening: minimum ms gap between same-type applications (Phase 10.4). */
    void rogue_buffs_set_dampening(double min_interval_ms);
    int rogue_buffs_get_total(RogueBuffType type);
    /* New Phase 7.3 helpers for persistence (non-mutating):
            rogue_buffs_active_count returns number of active buff records.
            rogue_buffs_get_active copies the i-th active buff (0..count-1) into *out and returns 1
       on success. */
    int rogue_buffs_active_count(void);
    int rogue_buffs_get_active(int index, RogueBuff* out);
    /* Phase 6.3 snapshot: copy up to max active buffs into out array, returns count. now_ms used to
     * prune expired. */
    int rogue_buffs_snapshot(RogueBuff* out, int max, double now_ms);

    /* Phase 4.4: Optional expiration callback (on natural expiry or manual remove). */
    typedef void (*RogueBuffExpireFn)(RogueBuffType type, int magnitude);
    void rogue_buffs_set_on_expire(RogueBuffExpireFn cb);

#ifdef __cplusplus
}
#endif
#endif
