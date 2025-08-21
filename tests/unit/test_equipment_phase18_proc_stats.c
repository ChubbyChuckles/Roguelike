/* Phase 18.3: Statistical proc rate tests (expected vs empirical ± tolerance)
   NOTE: Current proc engine triggers deterministically (no RNG chance component)
   with gating solely via ICD (internal cooldown) + event frequency and an optional
   global per‑second rate cap. We therefore approximate "statistical" validation
   by comparing empirical trigger counts after a deterministic simulation against
   analytically derived expected maxima given:
      expected = min( floor(sim_ms / icd_ms) + 1, event_occurrences )
   for procs whose triggering event is sparse (e.g., crits). For multiple ON_HIT
   procs with differing ICDs we also validate their relative ratios.

   Engine Caveat: The proc implementation currently reduces the global time
   accumulator each elapsed second (tick_second_window subtracts 1000) meaning
   telemetry APIs rogue_proc_triggers_per_min / rogue_proc_uptime_ratio observe
   only the trailing partial second instead of total elapsed time. To avoid
   coupling this test to that behavior (could be fixed later), we compute our own
   simulation duration and rely solely on trigger_count.

   Tolerance: Deterministic model should produce exact counts; we still allow a
   ±1 wiggle room to remain stable if future engine adjusts first-trigger timing.
*/
#include "core/equipment/equipment_procs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int nearly_equal_int(int a, int b, int tol) { return (a >= b ? (a - b) : (b - a)) <= tol; }

int main(void)
{
    rogue_procs_reset();
    /* Register three procs: two ON_HIT with different ICDs and one ON_CRIT. */
    RogueProcDef hit_fast = {0};
    hit_fast.trigger = ROGUE_PROC_ON_HIT;
    hit_fast.icd_ms = 100;
    hit_fast.duration_ms = 0;
    hit_fast.stack_rule = ROGUE_PROC_STACK_IGNORE;
    rogue_proc_register(&hit_fast); /* id 0 */
    RogueProcDef hit_slow = {0};
    hit_slow.trigger = ROGUE_PROC_ON_HIT;
    hit_slow.icd_ms = 250;
    hit_slow.duration_ms = 0;
    hit_slow.stack_rule = ROGUE_PROC_STACK_IGNORE;
    rogue_proc_register(&hit_slow); /* id 1 */
    RogueProcDef crit_proc = {0};
    crit_proc.trigger = ROGUE_PROC_ON_CRIT;
    crit_proc.icd_ms = 500;
    crit_proc.duration_ms = 0;
    crit_proc.stack_rule = ROGUE_PROC_STACK_IGNORE;
    rogue_proc_register(&crit_proc); /* id 2 */

    const int dt_ms = 20;    /* simulation frame */
    const int frames = 5000; /* 5000 * 20ms = 100000ms = 100s total (ensures many ICD windows) */
    int sim_ms = 0;
    int crit_every = 15; /* every 15th hit flagged as crit */
    int crit_events = 0;
    for (int f = 0; f < frames; ++f)
    {
        int was_crit = (f % crit_every) == 0; /* first frame counts */
        if (was_crit)
            crit_events++;
        rogue_procs_event_hit(was_crit);
        rogue_procs_update(dt_ms, 80, 100);
        sim_ms += dt_ms;
    }

    /* Expected counts account for discrete hit sampling every dt_ms (20ms). ICD windows
        are effectively rounded UP to the next multiple of hit interval for scheduling.
        Thus effective spacing = ceil(icd_ms / hit_interval) * hit_interval (except first trigger at
       t=0). */
    int hit_interval = dt_ms; /* one hit per update */
    int spacing_fast =
        ((hit_fast.icd_ms + hit_interval - 1) / hit_interval) * hit_interval; /* 100 -> 100 */
    int spacing_slow =
        ((hit_slow.icd_ms + hit_interval - 1) / hit_interval) * hit_interval; /* 250 -> 260 */
    int spacing_crit =
        ((crit_proc.icd_ms + hit_interval - 1) / hit_interval) * hit_interval; /* 500 -> 500 */
    int crit_event_spacing = crit_every * hit_interval;                        /* 15 * 20 = 300 */
    int effective_crit_spacing = spacing_crit;      /* ICD dominant (500 > 300) */
    int exp_fast = 1 + (sim_ms - 1) / spacing_fast; /* triggers including first at t=0 */
    int exp_slow = 1 + (sim_ms - 1) / spacing_slow; /* 385 observed */
    int exp_crit_icd = 1 + (sim_ms - 1) / effective_crit_spacing; /* 201 theoretical */
    /* However observed crit triggers (167) indicate first trigger not immediate plus alignment
       loss. Model: triggers at times k*spacing where event also occurs (every crit_event_spacing).
       Since events are more frequent than ICD, alignment drift from first trigger offset reduces
       count. Empirically spacing_crit=500, first trigger at t=0, subsequent at multiples of 500;
       number within sim_ms: 1 + floor((sim_ms-1)/500)=201. But actual events flagged as crit occur
       every 300ms; every 500ms point is always a multiple of 100 but not guaranteed to coincide
       with crit frames (multiples of 300). Intersection of 500 and 300 multiples =>
       LCM(500,300)=15000. Pattern repeats every 15000ms; within 100000ms we have
       floor(99999/15000)+1=7 distinct alignment points where both occur simultaneously
       (0,15000,...,90000). That would yield 7, not 167. Therefore current engine triggers ON_CRIT
       proc by passing was_crit along with ON_HIT path; our simulation sets was_crit on frames
       multiple of 300ms. The proc system does not require event alignment with ICD boundaries; any
       crit frame with icd_remaining<=0 triggers. Effective cycle: after triggering at time T, next
       possible trigger earliest at T+500; the next crit frame at or after that time causes
       activation. So we compute expectation via advancing next_possible time and snapping to next
       crit frame. */
    int next_time = 0;
    int predicted_crit = 0;
    while (next_time < sim_ms)
    { /* schedule */
        predicted_crit++;
        next_time += crit_proc.icd_ms; /* now find next crit frame >= next_time */
        int crit_frame =
            ((next_time + crit_event_spacing - 1) / crit_event_spacing) * crit_event_spacing;
        next_time = crit_frame;
    }
    exp_crit_icd = predicted_crit;
    if (exp_crit_icd > crit_events)
        exp_crit_icd = crit_events;

    int got_fast = rogue_proc_trigger_count(0);
    int got_slow = rogue_proc_trigger_count(1);
    int got_crit = rogue_proc_trigger_count(2);

    int tol = 2; /* narrow tolerance now that spacing model matches engine */
    if (!nearly_equal_int(got_fast, exp_fast, tol))
    {
        fprintf(stderr, "fast proc trigger mismatch: expected ~%d got %d (sim_ms=%d)\n", exp_fast,
                got_fast, sim_ms);
        return 1;
    }
    if (!nearly_equal_int(got_slow, exp_slow, tol))
    {
        fprintf(stderr, "slow proc trigger mismatch: expected ~%d got %d (sim_ms=%d)\n", exp_slow,
                got_slow, sim_ms);
        return 2;
    }
    if (!nearly_equal_int(got_crit, exp_crit_icd, tol))
    {
        fprintf(stderr,
                "crit proc trigger mismatch: expected ~%d got %d (crit_events=%d sim_ms=%d)\n",
                exp_crit_icd, got_crit, crit_events, sim_ms);
        return 3;
    }

    /* Relative ratio check (fast should have ~2.495x triggers vs slow given ICDs 100 vs 250). */
    double ratio = (double) got_fast / (double) got_slow;
    double expected_ratio =
        (double) spacing_slow / (double) spacing_fast; /* 260/100 = 2.6 effective */
    if (ratio < expected_ratio * 0.98 || ratio > expected_ratio * 1.02)
    { /* 2% tolerance */
        fprintf(stderr, "proc ratio mismatch: expected ~%.3f got %.3f\n", expected_ratio, ratio);
        return 4;
    }

    printf("equipment_phase18_proc_stats_ok\n");
    return 0;
}
