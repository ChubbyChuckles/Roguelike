#include "core/analytics.h"
#include "core/app_state.h"
void rogue_analytics_add_damage(unsigned int amount)
{
    g_app.analytics_damage_dealt_total += (unsigned long long) amount;
}
void rogue_analytics_add_gold(unsigned int amount)
{
    g_app.analytics_gold_earned_total += (unsigned long long) amount;
}
