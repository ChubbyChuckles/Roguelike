#include "ui_context.h"
#include <math.h>
#include <stdio.h>

static int nav_key_down(const RogueUIInputState* in, int key_index)
{
    switch (key_index)
    {
    case 0:
        return in->key_left;
    case 1:
        return in->key_right;
    case 2:
        return in->key_up;
    case 3:
        return in->key_down;
    case 4:
        return in->key_tab;
    case 5:
        return in->key_activate;
    default:
        return 0;
    }
}
static void rogue_apply_repeat(RogueUIContext* c, int idx, int* move_h, int* move_v,
                               int* activate_ptr)
{
    if (c->key_repeat_state[idx])
    {
        double acc = c->key_repeat_accum[idx];
        if (acc >= c->key_repeat_initial_ms)
        {
            double over = acc - c->key_repeat_initial_ms;
            int pulses = (int) (over / c->key_repeat_interval_ms);
            if (pulses > 0)
            {
                c->key_repeat_accum[idx] =
                    c->key_repeat_initial_ms + over - pulses * c->key_repeat_interval_ms;
                if (idx == 0)
                    *move_h = -1;
                else if (idx == 1)
                    *move_h = 1;
                else if (idx == 2)
                    *move_v = -1;
                else if (idx == 3)
                    *move_v = 1;
                else if (idx == 4)
                    *move_h = 1;
                else if (idx == 5)
                    *activate_ptr = 1;
            }
        }
    }
}

void rogue_ui_navigation_update(RogueUIContext* ctx)
{
    if (!ctx || !ctx->frame_active)
        return;
    if (ctx->replay_playing)
    {
        if (!rogue_ui_replay_step(ctx))
        {
        }
    }
    for (int i = 0; i < 6; i++)
    {
        if (nav_key_down(&ctx->input, i))
        {
            if (ctx->key_repeat_state[i] == 0)
            {
                ctx->key_repeat_state[i] = 1;
                ctx->key_repeat_accum[i] = 0.0;
            }
        }
        else
            ctx->key_repeat_state[i] = 0;
    }
    int focusable_count = 0;
    for (int i = 0; i < ctx->node_count; i++)
    {
        int k = ctx->nodes[i].kind;
        if (k >= 5 && k <= 8)
            focusable_count++;
    }
    if (!focusable_count)
        return;
    int axis_move_h = 0, axis_move_v = 0;
    float threshold = 0.55f;
    if (ctx->controller.axis_x > threshold)
        axis_move_h = 1;
    else if (ctx->controller.axis_x < -threshold)
        axis_move_h = -1;
    if (ctx->controller.axis_y > threshold)
        axis_move_v = 1;
    else if (ctx->controller.axis_y < -threshold)
        axis_move_v = -1;
    if (axis_move_h)
    {
        if (ctx->key_repeat_state[6] == 0)
        {
            ctx->key_repeat_state[6] = 1;
            ctx->key_repeat_accum[6] = 0.0;
        }
    }
    else
        ctx->key_repeat_state[6] = 0;
    if (axis_move_v)
    {
        if (ctx->key_repeat_state[7] == 0)
        {
            ctx->key_repeat_state[7] = 1;
            ctx->key_repeat_accum[7] = 0.0;
        }
    }
    else
        ctx->key_repeat_state[7] = 0;
    int move_h = 0, move_v = 0, activate = 0;
    if (ctx->input.key_left)
        move_h = -1;
    else if (ctx->input.key_right)
        move_h = 1;
    if (ctx->input.key_up)
        move_v = -1;
    else if (ctx->input.key_down)
        move_v = 1;
    if (ctx->input.key_tab)
        move_h = 1;
    if (ctx->input.key_activate)
        activate = 1;
    if (!move_h && axis_move_h)
        move_h = axis_move_h;
    if (!move_v && axis_move_v)
        move_v = axis_move_v;
    for (int i = 0; i < 6; i++)
        if (ctx->key_repeat_state[i])
            ctx->key_repeat_accum[i] += ctx->frame_dt_ms;
    for (int i = 6; i < 8; i++)
        if (ctx->key_repeat_state[i])
            ctx->key_repeat_accum[i] += ctx->frame_dt_ms;
    rogue_apply_repeat(ctx, 0, &move_h, &move_v, &activate);
    rogue_apply_repeat(ctx, 1, &move_h, &move_v, &activate);
    rogue_apply_repeat(ctx, 2, &move_h, &move_v, &activate);
    rogue_apply_repeat(ctx, 3, &move_h, &move_v, &activate);
    rogue_apply_repeat(ctx, 4, &move_h, &move_v, &activate);
    rogue_apply_repeat(ctx, 5, &move_h, &move_v, &activate);
    if (axis_move_h)
        rogue_apply_repeat(ctx, axis_move_h > 0 ? 1 : 0, &move_h, &move_v, &activate);
    if (axis_move_v)
        rogue_apply_repeat(ctx, axis_move_v > 0 ? 3 : 2, &move_h, &move_v, &activate);
    if (ctx->focus_index < 0 || ctx->focus_index >= ctx->node_count)
    {
        for (int i = 0; i < ctx->node_count; i++)
        {
            int k = ctx->nodes[i].kind;
            if (k >= 5 && k <= 8)
            {
                ctx->focus_index = i;
                break;
            }
        }
    }
    if (ctx->modal_index >= 0)
        ctx->focus_index = ctx->modal_index;
    if (ctx->focus_index < 0)
        return;
    if (ctx->input.key_tab && ctx->node_count > 1)
    {
        int start = ctx->focus_index < 0 ? 0 : ctx->focus_index;
        int curi = start;
        for (int tries = 0; tries < ctx->node_count; ++tries)
        {
            curi = (curi + 1 < ctx->node_count) ? (curi + 1) : 0;
            int k = ctx->nodes[curi].kind;
            if (k >= 5 && k <= 8)
            {
                ctx->focus_index = curi;
                break;
            }
        }
        if (ctx->pending_chord &&
            (ctx->time_ms - ctx->pending_chord_time_ms) > ctx->chord_timeout_ms)
            ctx->pending_chord = 0;
        return;
    }
    if (activate)
    {
        RogueUINode* cur = &ctx->nodes[ctx->focus_index];
        if (cur->kind == 5)
            cur->value = 1.0f;
        else if (cur->kind == 6)
            cur->value = (cur->value == 0.0f) ? 1.0f : 0.0f;
    }
    if (!move_h && !move_v)
        return;
    RogueUINode* cur = &ctx->nodes[ctx->focus_index];
    float cx = cur->rect.x + cur->rect.w * 0.5f;
    float cy = cur->rect.y + cur->rect.h * 0.5f;
    int best = -1;
    float best_score = 1e9f;
    for (int i = 0; i < ctx->node_count; i++)
    {
        if (i == ctx->focus_index)
            continue;
        int k = ctx->nodes[i].kind;
        if (k < 5 || k > 8)
            continue;
        if (ctx->modal_index >= 0 && i != ctx->modal_index)
            continue;
        RogueUINode* n = &ctx->nodes[i];
        float nx = n->rect.x + n->rect.w * 0.5f;
        float ny = n->rect.y + n->rect.h * 0.5f;
        float dx = nx - cx;
        float dy = ny - cy;
        if (move_h)
        {
            if (move_h < 0 && dx >= -1e-3f)
                continue;
            if (move_h > 0 && dx <= 1e-3f)
                continue;
        }
        if (move_v)
        {
            if (move_v < 0 && dy >= -1e-3f)
                continue;
            if (move_v > 0 && dy <= 1e-3f)
                continue;
        }
        float primary = move_h ? fabsf(dx) : fabsf(dy);
        float secondary = move_h ? fabsf(dy) : fabsf(dx);
        if (secondary > primary * 2.5f)
            continue;
        float dist = (float) sqrt(dx * dx + dy * dy);
        float score = dist + secondary * 0.25f + primary * 0.1f;
        if (score < best_score)
        {
            best_score = score;
            best = i;
        }
    }
    if (best >= 0)
    {
        ctx->focus_index = best;
        return;
    }
    int dir = (move_h > 0 || move_v > 0) ? 1 : -1;
    int start = ctx->focus_index;
    int curi = start;
    for (;;)
    {
        curi += dir;
        if (curi >= ctx->node_count)
            curi = 0;
        if (curi < 0)
            curi = ctx->node_count - 1;
        if (curi == start)
            break;
        int k = ctx->nodes[curi].kind;
        if (k >= 5 && k <= 8)
        {
            ctx->focus_index = curi;
            break;
        }
    }
    if (ctx->pending_chord && (ctx->time_ms - ctx->pending_chord_time_ms) > ctx->chord_timeout_ms)
        ctx->pending_chord = 0;
}
