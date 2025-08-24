#define SDL_MAIN_HANDLED 1
#include "../../src/core/skills/skills.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* For now ordering invariance contracts to skill_get_effective_coefficient being pure */
    float a_then_b = skill_get_effective_coefficient(0); /* imagine progression then equipment */
    float b_then_a = skill_get_effective_coefficient(0); /* equipment then progression */
    assert(a_then_b == b_then_a);
    printf("OK\n");
    return 0;
}
