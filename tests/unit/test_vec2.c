#include "../../src/math/vec2.h"
#include <math.h>
#include <stdio.h>

static int almost(float a, float b) { return fabsf(a - b) < 1e-6f; }

int main(void)
{
    RogueVec2 a = rogue_vec2(1.0f, 2.0f);
    RogueVec2 b = rogue_vec2(3.0f, 5.0f);
    RogueVec2 c = rogue_vec2_add(a, b);
    if (!almost(c.x, 4.0f) || !almost(c.y, 7.0f))
    {
        printf("add fail\n");
        return 1;
    }
    RogueVec2 d = rogue_vec2_scale(a, 2.0f);
    if (!almost(d.x, 2.0f) || !almost(d.y, 4.0f))
    {
        printf("scale fail\n");
        return 1;
    }
    return 0;
}
