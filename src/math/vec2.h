#ifndef ROGUE_MATH_VEC2_H
#define ROGUE_MATH_VEC2_H

typedef struct RogueVec2
{
    float x, y;
} RogueVec2;

static inline RogueVec2 rogue_vec2(float x, float y)
{
    RogueVec2 v = {x, y};
    return v;
}
static inline RogueVec2 rogue_vec2_add(RogueVec2 a, RogueVec2 b)
{
    return rogue_vec2(a.x + b.x, a.y + b.y);
}
static inline RogueVec2 rogue_vec2_sub(RogueVec2 a, RogueVec2 b)
{
    return rogue_vec2(a.x - b.x, a.y - b.y);
}
static inline RogueVec2 rogue_vec2_scale(RogueVec2 a, float s)
{
    return rogue_vec2(a.x * s, a.y * s);
}

#endif
