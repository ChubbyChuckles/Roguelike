#include "ui_animation.h"
#include "ui_context.h"
#include <math.h>
#include <string.h>

/* Local animation state formerly static in monolith */
typedef struct UIAnimEntry
{
    uint32_t id;
    float t;
    float duration;
    int kind;
    RogueUIEaseType ease;
    float extra;
} UIAnimEntry; /* kind:0 entrance 1 exit 2 pulse */
typedef struct UIAnimExitDone
{
    uint32_t id;
    int ttl;
} UIAnimExitDone;
static UIAnimEntry g_ui_anims[128];
static int g_ui_anim_count = 0;
static UIAnimExitDone g_ui_exit_done[64];
static int g_ui_exit_done_count = 0;

typedef struct UITimelineEntry
{
    uint32_t id;
    float t;
    float duration;
    int keyframe_count;
    RogueUITimelineKeyframe kf[6];
    int active;
} UITimelineEntry;
static UITimelineEntry g_ui_timelines[64];
static int g_ui_timeline_count = 0;
static UITimelineEntry* ui_tl_find(uint32_t id)
{
    for (int i = 0; i < g_ui_timeline_count; i++)
        if (g_ui_timelines[i].id == id)
            return &g_ui_timelines[i];
    return NULL;
}
static UITimelineEntry* ui_tl_alloc(uint32_t id)
{
    UITimelineEntry* e = ui_tl_find(id);
    if (e)
        return e;
    if (g_ui_timeline_count >= (int) (sizeof(g_ui_timelines) / sizeof(g_ui_timelines[0])))
        return NULL;
    e = &g_ui_timelines[g_ui_timeline_count++];
    memset(e, 0, sizeof *e);
    e->id = id;
    e->active = 1;
    return e;
}

void rogue_ui_timeline_play(RogueUIContext* ctx, uint32_t id_hash,
                            const RogueUITimelineKeyframe* kfs, int count, float duration_ms,
                            RogueUITimelinePolicy policy)
{
    (void) ctx;
    if (!kfs || count < 2)
        return;
    if (duration_ms <= 0)
        duration_ms = 1.0f;
    if (count > 6)
        count = 6;
    if (policy == ROGUE_UI_TIMELINE_REPLACE)
    {
        UITimelineEntry* ex = ui_tl_find(id_hash);
        if (ex)
        {
            *ex = (UITimelineEntry){0};
            ex->id = id_hash;
            ex->active = 1;
        }
    }
    if (policy == ROGUE_UI_TIMELINE_IGNORE && ui_tl_find(id_hash))
        return;
    UITimelineEntry* e = ui_tl_alloc(id_hash);
    if (!e)
        return;
    e->t = 0;
    e->duration = duration_ms;
    e->keyframe_count = count;
    for (int i = 0; i < count; i++)
    {
        e->kf[i] = kfs[i];
        if (e->kf[i].at < 0)
            e->kf[i].at = 0;
        if (e->kf[i].at > 1)
            e->kf[i].at = 1;
    }
    e->active = 1;
}
static void ui_timeline_step(double dt_ms)
{
    for (int i = 0; i < g_ui_timeline_count;)
    {
        UITimelineEntry* e = &g_ui_timelines[i];
        if (!e->active && e->t > e->duration)
        {
            g_ui_timelines[i] = g_ui_timelines[--g_ui_timeline_count];
            continue;
        }
        e->t += (float) dt_ms;
        if (e->active && e->t >= e->duration)
        {
            e->t = e->duration;
            e->active = 0;
        }
        i++;
    }
}
static float ui_timeline_sample(uint32_t id, int mode_scale, int* active_out)
{
    UITimelineEntry* e = ui_tl_find(id);
    if (active_out)
        *active_out = (e && e->active) ? 1 : 0;
    if (!e)
        return 1.0f;
    float norm = e->duration > 0 ? e->t / e->duration : 1.0f;
    if (norm < 0)
        norm = 0;
    if (norm > 1)
        norm = 1;
    RogueUITimelineKeyframe a = e->kf[0];
    RogueUITimelineKeyframe b = e->kf[e->keyframe_count - 1];
    for (int i = 1; i < e->keyframe_count; i++)
    {
        if (norm <= e->kf[i].at)
        {
            a = e->kf[i - 1];
            b = e->kf[i];
            break;
        }
    }
    float span = b.at - a.at;
    float lt = span > 1e-6f ? (norm - a.at) / span : 1.0f;
    float eased = rogue_ui_ease(b.ease, lt);
    float scale = a.scale + (b.scale - a.scale) * eased;
    float alpha = a.alpha + (b.alpha - a.alpha) * eased;
    return mode_scale ? scale : alpha;
}
float rogue_ui_timeline_scale(const RogueUIContext* ctx, uint32_t id_hash, int* active_out)
{
    (void) ctx;
    return ui_timeline_sample(id_hash, 1, active_out);
}
float rogue_ui_timeline_alpha(const RogueUIContext* ctx, uint32_t id_hash, int* active_out)
{
    (void) ctx;
    return ui_timeline_sample(id_hash, 0, active_out);
}

static UIAnimEntry* ui_anim_find(uint32_t id)
{
    for (int i = 0; i < g_ui_anim_count; i++)
        if (g_ui_anims[i].id == id)
            return &g_ui_anims[i];
    return NULL;
}
static UIAnimEntry* ui_anim_alloc(uint32_t id)
{
    UIAnimEntry* e = ui_anim_find(id);
    if (e)
        return e;
    if (g_ui_anim_count >= (int) (sizeof(g_ui_anims) / sizeof(g_ui_anims[0])))
        return NULL;
    e = &g_ui_anims[g_ui_anim_count++];
    memset(e, 0, sizeof *e);
    e->id = id;
    return e;
}

void rogue_ui_set_time_scale(RogueUIContext* ctx, float scale)
{
    if (ctx)
        ctx->anim_time_scale = scale;
}
static float ease_cubic_in(float x) { return x * x * x; }
static float ease_cubic_out(float x)
{
    float inv = 1.0f - x;
    return 1.0f - inv * inv * inv;
}
static float ease_cubic_in_out(float x)
{
    if (x < 0.5f)
    {
        return 4.0f * x * x * x;
    }
    float f = (2.0f * x - 2.0f);
    return 0.5f * f * f * f + 1.0f;
}
static float ease_spring(float x)
{
    float d = 1.0f - x;
    return 1.0f - (d * d * (1.0f + 2.2f * d));
}
static float ease_elastic_out(float x)
{
    if (x <= 0)
        return 0;
    if (x >= 1)
        return 1;
    float p = 0.3f;
    return powf(2.0f, -10.0f * x) * sinf((x - p / 4.0f) * (2.0f * 3.14159265f) / p) + 1.0f;
}
float rogue_ui_ease(RogueUIEaseType t, float x)
{
    if (x < 0)
        x = 0;
    if (x > 1)
        x = 1;
    switch (t)
    {
    case ROGUE_EASE_CUBIC_IN:
        return ease_cubic_in(x);
    case ROGUE_EASE_CUBIC_OUT:
        return ease_cubic_out(x);
    case ROGUE_EASE_CUBIC_IN_OUT:
        return ease_cubic_in_out(x);
    case ROGUE_EASE_SPRING:
        return ease_spring(x);
    case ROGUE_EASE_ELASTIC_OUT:
        return ease_elastic_out(x);
    default:
        return x;
    }
}

void rogue_ui_entrance(RogueUIContext* ctx, uint32_t id_hash, float duration_ms,
                       RogueUIEaseType ease)
{
    if (duration_ms <= 0)
        duration_ms = 1.0f;
    if (ctx && ctx->reduced_motion)
        duration_ms *= 0.25f;
    UIAnimEntry* e = ui_anim_alloc(id_hash);
    if (!e)
        return;
    e->t = 0;
    e->duration = duration_ms;
    e->kind = 0;
    e->ease = ease;
}
void rogue_ui_exit(RogueUIContext* ctx, uint32_t id_hash, float duration_ms, RogueUIEaseType ease)
{
    if (duration_ms <= 0)
        duration_ms = 1.0f;
    if (ctx && ctx->reduced_motion)
        duration_ms *= 0.25f;
    UIAnimEntry* e = ui_anim_alloc(id_hash);
    if (!e)
        return;
    e->t = 0;
    e->duration = duration_ms;
    e->kind = 1;
    e->ease = ease;
}
void rogue_ui_button_press_pulse(RogueUIContext* ctx, uint32_t id_hash)
{
    (void) ctx;
    UIAnimEntry* e = ui_anim_alloc(id_hash ^ 0xB00B135u);
    if (!e)
        return;
    e->t = 0;
    e->duration = 180.0f;
    e->kind = 2;
    e->ease = ROGUE_EASE_SPRING;
}

static void ui_anim_step(double dt_ms)
{
    for (int i = 0; i < g_ui_anim_count;)
    {
        UIAnimEntry* e = &g_ui_anims[i];
        e->t += (float) dt_ms;
        if (e->t >= e->duration)
        {
            if (e->kind == 1)
            {
                if (g_ui_exit_done_count <
                    (int) (sizeof(g_ui_exit_done) / sizeof(g_ui_exit_done[0])))
                {
                    g_ui_exit_done[g_ui_exit_done_count].id = e->id;
                    g_ui_exit_done[g_ui_exit_done_count].ttl = 30;
                    g_ui_exit_done_count++;
                }
            }
            g_ui_anims[i] = g_ui_anims[--g_ui_anim_count];
            continue;
        }
        i++;
    }
    for (int i = 0; i < g_ui_exit_done_count;)
    {
        if (--g_ui_exit_done[i].ttl <= 0)
        {
            g_ui_exit_done[i] = g_ui_exit_done[--g_ui_exit_done_count];
            continue;
        }
        i++;
    }
}
void ui_animation_master_step(double dt_ms)
{
    ui_anim_step(dt_ms);
    ui_timeline_step(dt_ms);
}

float rogue_ui_anim_scale(const RogueUIContext* ctx, uint32_t id_hash)
{
    (void) ctx;
    UIAnimEntry* e_ent = ui_anim_find(id_hash);
    UIAnimEntry* e_pulse = ui_anim_find(id_hash ^ 0xB00B135u);
    float base_scale = 1.0f;
    if (e_ent)
    {
        float x = e_ent->t / e_ent->duration;
        if (x < 0)
            x = 0;
        if (x > 1)
            x = 1;
        float v = rogue_ui_ease(e_ent->ease, x);
        if (e_ent->kind == 0)
            base_scale = 0.85f + 0.15f * v;
        else if (e_ent->kind == 1)
            base_scale = 1.0f - 0.15f * v;
    }
    float pulse_scale = 1.0f;
    if (e_pulse && e_pulse->kind == 2)
    {
        float x = e_pulse->t / e_pulse->duration;
        if (x < 0)
            x = 0;
        if (x > 1)
            x = 1;
        float v = rogue_ui_ease(e_pulse->ease, x);
        pulse_scale = 1.0f + (1.0f - v) * 0.15f;
    }
    return base_scale * pulse_scale;
}
float rogue_ui_anim_alpha(const RogueUIContext* ctx, uint32_t id_hash)
{
    (void) ctx;
    UIAnimEntry* e = ui_anim_find(id_hash);
    if (!e)
    {
        for (int i = 0; i < g_ui_exit_done_count; i++)
            if (g_ui_exit_done[i].id == id_hash)
                return 0.0f;
        return 1.0f;
    }
    float x = e->t / e->duration;
    if (x < 0)
        x = 0;
    if (x > 1)
        x = 1;
    float v = rogue_ui_ease(e->ease, x);
    if (e->kind == 0)
        return v;
    else if (e->kind == 1)
        return 1.0f - x;
    return 1.0f;
}
