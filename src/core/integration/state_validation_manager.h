// State Validation & Integrity Checking (Phase 5.5)
#ifndef ROGUE_STATE_VALIDATION_MANAGER_H
#define ROGUE_STATE_VALIDATION_MANAGER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RogueValidationSeverity
    {
        ROGUE_VALID_OK = 0,
        ROGUE_VALID_WARN = 1,
        ROGUE_VALID_CORRUPT = 2
    } RogueValidationSeverity;

    typedef struct RogueValidationResult
    {
        RogueValidationSeverity severity;
        uint32_t code;       // user-defined code
        const char* message; // optional static message pointer (copied into log)
    } RogueValidationResult;

    typedef RogueValidationResult (*RogueSystemValidateFn)(void* user);
    typedef int (*RogueSystemRepairFn)(void* user, uint32_t code);
    typedef RogueValidationResult (*RogueCrossRuleFn)(void* user);

    int rogue_validation_register_system(int system_id, RogueSystemValidateFn fn,
                                         RogueSystemRepairFn repair, void* user);
    int rogue_validation_register_cross_rule(const char* name, RogueCrossRuleFn fn, void* user);

    // Scheduling
    void rogue_validation_set_interval(uint32_t ticks); // 0 disables automatic scheduling
    void rogue_validation_tick(
        uint64_t current_tick); // call each frame with monotonically increasing tick
    int rogue_validation_run_now(int force_all); // force_all=1 ignores incremental hash skip
    void
    rogue_validation_trigger(void); // mark pending run next tick (even if interval not reached)

    typedef struct RogueValidationStats
    {
        uint64_t system_validations_run;
        uint64_t system_validations_skipped_unchanged;
        uint64_t cross_rule_runs;
        uint64_t warnings;
        uint64_t corruptions_detected;
        uint64_t repairs_attempted;
        uint64_t repairs_succeeded;
        uint64_t total_ns_spent; // coarse (may be 0 on some platforms)
        uint64_t runs_initiated;
        uint64_t runs_completed;
    } RogueValidationStats;

    typedef struct RogueValidationEvent
    {
        uint64_t seq;
        uint64_t tick;
        int system_id; // -1 for cross rule
        RogueValidationSeverity severity;
        uint32_t code;
        char message[96];
        uint8_t repair_attempted;
        uint8_t repair_success;
    } RogueValidationEvent;

    void rogue_validation_get_stats(RogueValidationStats* out);
    int rogue_validation_events_get(const RogueValidationEvent** out_events, size_t* count);
    void rogue_validation_dump(void* fptr);
    void rogue_validation_reset_all(void); // tests

#ifdef __cplusplus
}
#endif
#endif
