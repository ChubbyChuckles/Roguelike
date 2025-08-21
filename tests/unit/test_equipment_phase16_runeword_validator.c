/* Phase 16.4: Runeword recipe validator test */
#include "core/equipment/equipment_content.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect_fail(const char* pattern)
{
    int rc = rogue_runeword_validate_pattern(pattern);
    if (rc == 0)
    {
        fprintf(stderr, "expected failure for pattern '%s'\n", pattern ? pattern : "(null)");
        assert(0);
    }
}
static void expect_ok(const char* pattern)
{
    int rc = rogue_runeword_validate_pattern(pattern);
    if (rc != 0)
    {
        fprintf(stderr, "expected ok for pattern '%s' rc=%d\n", pattern, rc);
        assert(0);
    }
}

int main(void)
{
    /* Invalid cases */
    expect_fail(NULL);
    expect_fail("");
    expect_fail("UPPER"); /* uppercase not allowed */
    expect_fail("bad-char!");
    expect_fail("too_many_segments_for_rule_enforced"); /* underscores will exceed segment limit */
    expect_fail("segment__double"); /* empty segment between __ treated as invalid char rule (since
                                       second '_' increments segment count with no content) */

    /* Valid simple */
    expect_ok("fire");
    expect_ok("ice3");
    expect_ok("abc_def");
    /* Max segments (5) with short components */
    expect_ok("a_b_c_d_e");

    /* Registration uses validator */
    RogueRuneword rw = {0};
    strcpy(rw.pattern, "fire_ice");
    rw.strength = 2;
    assert(rogue_runeword_register(&rw) >= 0);
    RogueRuneword bad = {0};
    strcpy(bad.pattern, "BAD");
    assert(rogue_runeword_register(&bad) < 0);

    printf("Phase16.4 runeword validator OK\n");
    return 0;
}
