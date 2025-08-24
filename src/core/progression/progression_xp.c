#include "progression_xp.h"
#include <math.h>

/* Curve rationale:
 * Early levels smooth ramp: quadratic-ish small increments.
 * Mid levels exponential influence: level^2.1 term.
 * Late levels compression: log component dampens runaway growth keeping progress asymptotic but
 * unbounded. xp_to_next(level) = base + a*level + b*level^2 + c*level^2.1 + d*log(level+1)
 * Constants tuned for moderate pace; can be data-driven later.
 */
static const double A_lin = 8.0;
static const double B_quad = 0.75; /* mild quadratic */
static const double C_pow = 0.055; /* exponential-ish factor */
static const double P_exp = 2.10;
static const double D_log = 18.0;
static const double BASE = 35.0;

unsigned int rogue_xp_to_next_for_level(int level)
{
    if (level < 1)
        level = 1;
    double lv = (double) level;
    double val =
        BASE + A_lin * lv + B_quad * lv * lv + C_pow * pow(lv, P_exp) + D_log * log(lv + 1.0);
    if (val < 1.0)
        val = 1.0;
    if (val > (double) 0xFFFFFFFFu)
        val = (double) 0xFFFFFFFFu;
    return (unsigned int) (val + 0.5);
}

unsigned long long rogue_xp_total_required_for_level(int level)
{
    if (level <= 1)
        return 0ULL;
    unsigned long long total = 0ULL;
    for (int l = 1; l < level; ++l)
    {
        total += (unsigned long long) rogue_xp_to_next_for_level(l);
    }
    return total;
}

float rogue_xp_catchup_multiplier(int player_level, int median_level)
{
    if (median_level < 1)
        median_level = 1;
    if (player_level >= median_level)
        return 1.0f;
    int diff = median_level - player_level; /* positive */
    /* Multiplier grows with deficit but with diminishing returns using tanh curve. */
    double scaled = (double) diff / 10.0; /* 10 level deficit ~ moderate boost */
    double t = tanh(scaled);              /* 0..~1 */
    double mult = 1.0 + t * 0.75;         /* cap 1.75x */
    return (float) mult;
}

int rogue_xp_safe_add(unsigned long long* total, unsigned long long add)
{
    if (!total)
        return -1;
    unsigned long long before = *total;
    unsigned long long after = before + add;
    if (after < before)
    { /* overflow wrap */
        *total = ULLONG_MAX;
        return -1;
    }
    *total = after;
    return (*total == ULLONG_MAX) ? -1 : 0;
}
