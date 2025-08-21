#include "core/progression/progression_xp.h"
#include <assert.h>
#include <stdio.h>

static void test_monotonicity(void)
{
    unsigned int prev = rogue_xp_to_next_for_level(1);
    for (int lvl = 2; lvl < 250; ++lvl)
    {
        unsigned int cur = rogue_xp_to_next_for_level(lvl);
        assert(cur >= prev); /* non-decreasing */
        prev = cur;
    }
}

static void test_total_consistency(void)
{
    unsigned long long total_prev = 0ULL;
    for (int lvl = 2; lvl < 150; ++lvl)
    {
        unsigned long long total = rogue_xp_total_required_for_level(lvl);
        unsigned long long step = rogue_xp_to_next_for_level(lvl - 1);
        assert(total == total_prev + step);
        total_prev = total;
    }
}

static void test_catchup(void)
{
    int median = 80;
    float m_equal = rogue_xp_catchup_multiplier(80, median);
    assert(m_equal == 1.0f);
    float m_lower = rogue_xp_catchup_multiplier(60, median);
    assert(m_lower > 1.0f);
    float m_far = rogue_xp_catchup_multiplier(10, median);
    assert(m_far > m_lower);
    float m_cap = rogue_xp_catchup_multiplier(-100, median);
    (void) m_cap; /* function normalizes */
}

static void test_overflow_protection(void)
{
    unsigned long long acc = 0ULL;
    assert(rogue_xp_safe_add(&acc, 1000ULL) == 0);
    acc = ~0ULL - 10ULL;
    int r = rogue_xp_safe_add(&acc, 1000ULL);
    assert(r == -1); /* saturated */
}

int main(void)
{
    test_monotonicity();
    test_total_consistency();
    test_catchup();
    test_overflow_protection();
    printf("progression_phase1_xp_curve: OK\n");
    return 0;
}
