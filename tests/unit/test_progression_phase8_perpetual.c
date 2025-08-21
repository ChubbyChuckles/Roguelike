#include "core/progression/progression_perpetual.h"
#include <math.h>
#include <stdio.h>

/* Phase 8 tests: verify sublinear growth, allowance pacing, diminishing increments */
int main(void)
{
    if (rogue_perpetual_init() != 0)
    {
        printf("init_fail\n");
        return 1;
    }
    int level = 100;
    int allowed = rogue_perpetual_micro_nodes_allowed(level);
    if (allowed <= 0)
    {
        printf("allow_fail %d\n", allowed);
        return 2;
    }
    /* Spend all allowed nodes */
    int spent = 0;
    while (rogue_perpetual_spend_node(level))
        spent++;
    if (spent != allowed)
    {
        printf("spent_mismatch %d %d\n", spent, allowed);
        return 3;
    }
    /* Further spend should fail */
    if (rogue_perpetual_spend_node(level))
    {
        printf("overspend\n");
        return 4;
    }
    double p_raw = rogue_perpetual_raw_power();
    if (p_raw <= 0.0)
    {
        printf("zero_power\n");
        return 5;
    }
    /* Check diminishing: first vs last increment difference */
    rogue_perpetual_reset();
    double first_inc;
    {
        rogue_perpetual_spend_node(level);
        first_inc = rogue_perpetual_raw_power();
    }
    double prev = first_inc;
    double last_inc = first_inc;
    int safety = 0;
    while (rogue_perpetual_spend_node(level) && safety < 10000)
    {
        double cur = rogue_perpetual_raw_power();
        last_inc = cur - prev;
        prev = cur;
        safety++;
    }
    if (!(first_inc > last_inc))
    {
        printf("no_diminish %f %f\n", first_inc, last_inc);
        return 6;
    }
    double eff = rogue_perpetual_effective_power(level);
    if (eff <= 0.0)
    {
        printf("eff_zero\n");
        return 7;
    }
    /* Sublinear: compare level 200 vs 100 scaling ratio */
    double eff100 = eff;
    rogue_perpetual_reset();
    int level2 = 200;
    while (rogue_perpetual_spend_node(level2))
        ;
    double eff200 = rogue_perpetual_effective_power(level2);
    double ratio = eff200 / eff100;
    if (ratio > 1.9)
    { /* expect less than 2x */
        printf("not_sublinear ratio=%f\n", ratio);
        return 8;
    }
    printf("progression_phase8_perpetual: OK level100=%f level200=%f ratio=%f nodes100=%d "
           "nodes200=%d\n",
           eff100, eff200, ratio, allowed, rogue_perpetual_micro_nodes_allowed(level2));
    return 0;
}
