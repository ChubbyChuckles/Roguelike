#include "world_gen_dungeon_objectives.h"
#include "world_gen_dungeon_kernel.h"
#include <stdlib.h>
#include <string.h>

static int compute_room_depths_obj(const RogueDungeonGraph* g, int* out_depths)
{
    if (!g || g->room_count <= 0)
        return 0;
    int n = g->room_count;
    for (int i = 0; i < n; ++i)
        out_depths[i] = -1;
    int* q = (int*) malloc((size_t) n * sizeof(int));
    if (!q)
        return 0;
    int qs = 0, qe = 0;
    out_depths[0] = 0;
    q[qe++] = 0;
    while (qs < qe)
    {
        int cur = q[qs++];
        int cd = out_depths[cur];
        for (int e = 0; e < g->edge_count; ++e)
        {
            int a = g->edges[e].a, b = g->edges[e].b;
            int nxt = -1;
            if (a == cur)
                nxt = b;
            else if (b == cur)
                nxt = a;
            if (nxt >= 0 && nxt < n && out_depths[nxt] == -1)
            {
                out_depths[nxt] = cd + 1;
                q[qe++] = nxt;
            }
        }
    }
    free(q);
    /* Validate connectivity */
    for (int i = 0; i < n; ++i)
        if (out_depths[i] < 0)
            return 0;
    return 1;
}

/* --- Phase 5.2: objective script management --- */
static char g_obj_script[128] = {0};

void rogue_dungeon_set_objective_script(const char* spec)
{
    if (!spec || !*spec)
    {
        g_obj_script[0] = '\0';
        return;
    }
    size_t n = strlen(spec);
    if (n >= sizeof(g_obj_script))
        n = sizeof(g_obj_script) - 1;
    memcpy(g_obj_script, spec, n);
    g_obj_script[n] = '\0';
}

typedef enum ObjTokenKind
{
    TOK_NONE = 0,
    TOK_ACTIVATE,
    TOK_CLEAR,
    TOK_PUZZLE,
    TOK_PUZZLE_OPTIONAL,
    TOK_BOSS,
    TOK_GATE
} ObjTokenKind;

/* ASCII-only case-insensitive equality on the first n chars */
static int ci_eq_ascii(const char* a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z')
            ca = (char) (ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (char) (cb - 'A' + 'a');
        if (ca != cb)
            return 0;
    }
    return 1;
}

static int parse_script_tokens(const char* s, ObjTokenKind* out, int cap)
{
    if (!s || !*s)
        return 0;
    int count = 0;
    const char* p = s;
    while (*p && count < cap)
    {
        while (*p == ' ' || *p == '\t' || *p == ',')
            ++p;
        if (!*p)
            break;
        int optional = 0;
        if (*p == '?')
        {
            optional = 1;
            ++p;
        }
        const char* start = p;
        while (*p && *p != ',' && *p != '(' && *p != ')')
            ++p;
        size_t len = (size_t) (p - start);
        ObjTokenKind kind = TOK_NONE;
        /* Case-insensitive compare for token name */
#define CMP(tok) (len == strlen(tok) && ci_eq_ascii(start, tok, len))
        if (CMP("ACTIVATE"))
            kind = TOK_ACTIVATE;
        else if (CMP("CLEAR"))
            kind = TOK_CLEAR;
        else if (CMP("PUZZLE"))
            kind = optional ? TOK_PUZZLE_OPTIONAL : TOK_PUZZLE;
        else if (CMP("BOSS"))
            kind = TOK_BOSS;
        else if (CMP("GATE"))
            kind = TOK_GATE; /* expect (KEYSTONE) optional */
#undef CMP
        /* Skip optional (...) for GATE */
        while (*p && *p != ',')
            ++p;
        if (kind != TOK_NONE)
            out[count++] = kind;
        if (*p == ',')
            ++p;
    }
    return count;
}

static int id_in_used(const int* used, int used_n, int id)
{
    for (int i = 0; i < used_n; ++i)
        if (used[i] == id)
            return 1;
    return 0;
}

int rogue_dungeon_build_objectives(RogueWorldGenContext* ctx, const RogueDungeonGraph* graph,
                                   int depth, RogueDungeonObjectiveStep* out, int max_steps)
{
    (void) depth;
    if (!ctx || !graph || !out || max_steps < 3 || graph->room_count <= 0)
        return 0;

    int n = graph->room_count;
    int* rdepths = (int*) malloc((size_t) n * sizeof(int));
    if (!rdepths)
        return 0;
    if (!compute_room_depths_obj(graph, rdepths))
    {
        free(rdepths);
        return 0;
    }

    /* Find shallowest non-start candidate for ACTIVATE/CLEAR */
    int shallow_idx = 0; /* start room as fallback */
    int shallow_d = rdepths[0];
    for (int i = 1; i < n; ++i)
    {
        if (rdepths[i] < shallow_d)
        {
            shallow_d = rdepths[i];
            shallow_idx = i;
        }
    }

    /* Optional mid-step: PUZZLE_COMPLETE if any puzzle-tagged room exists */
    int puzzle_idx = -1;
    for (int i = 0; i < n; ++i)
    {
        if ((graph->rooms[i].tag & ROGUE_DUNGEON_ROOM_PUZZLE) != 0)
        {
            puzzle_idx = i;
            break;
        }
    }

    /* Farthest room (by BFS depth) becomes BOSS */
    int far_idx = 0;
    int far_d = rdepths[0];
    for (int i = 1; i < n; ++i)
    {
        if (rdepths[i] > far_d)
        {
            far_d = rdepths[i];
            far_idx = i;
        }
    }

    int steps = 0;

/* Helper to append a step */
#define APPEND_STEP(kind, rid, p0)                                                                 \
    do                                                                                             \
    {                                                                                              \
        if (steps < max_steps)                                                                     \
        {                                                                                          \
            out[steps].step_index = steps;                                                         \
            out[steps].type = (kind);                                                              \
            out[steps].room_id = (rid);                                                            \
            out[steps].param0 = (p0);                                                              \
            steps++;                                                                               \
        }                                                                                          \
    } while (0)

    /* Choose mid-depth representative deterministically */
    int target_depth = (far_d > 1) ? (far_d / 2) : 1;
    int mid_idx = shallow_idx;
    int best_dist = 9999;
    for (int i = 0; i < n; ++i)
    {
        int dist =
            rdepths[i] > target_depth ? (rdepths[i] - target_depth) : (target_depth - rdepths[i]);
        if (dist < best_dist)
        {
            best_dist = dist;
            mid_idx = i;
        }
    }

    /* Dynamic substitution (Phase 5.4): if puzzle absent but PUZZLE requested, substitute CLEAR
     * at the mid node to preserve pacing. If CLEAR already used there, choose nearest distinct. */
    int subst_clear_idx = mid_idx;
    if (puzzle_idx < 0)
    {
        /* keep subst_clear_idx as mid_idx; selection logic below avoids duplicates */
    }

    /* Scripted sequence if provided, else default */
    ObjTokenKind seq[8];
    int tcount = 0;
    if (g_obj_script[0])
        tcount = parse_script_tokens(g_obj_script, seq, 8);
    if (tcount <= 0)
    {
        seq[0] = TOK_ACTIVATE;
        seq[1] = (puzzle_idx >= 0) ? TOK_PUZZLE : TOK_PUZZLE_OPTIONAL; /* treated as optional */
        seq[2] = TOK_CLEAR;
        seq[3] = TOK_BOSS;
        tcount = 4;
    }

    int used_room_ids[4] = {-1, -1, -1, -1};

    for (int ti = 0; ti < tcount; ++ti)
    {
        ObjTokenKind tk = seq[ti];
        switch (tk)
        {
        case TOK_ACTIVATE:
        {
            int id = graph->rooms[shallow_idx].id;
            if (id_in_used(used_room_ids, 4, id))
                id = -1;
            if (id < 0)
                id = graph->rooms[0].id; /* safe fallback */
            APPEND_STEP(ROGUE_OBJ_ACTIVATE, id, 0);
            used_room_ids[0] = id;
        }
        break;
        case TOK_PUZZLE:
            if (puzzle_idx >= 0)
            {
                int id = graph->rooms[puzzle_idx].id;
                if (id_in_used(used_room_ids, 4, id))
                    id = -1;
                if (id < 0)
                    break; /* already used, skip duplicate */
                APPEND_STEP(ROGUE_OBJ_PUZZLE_COMPLETE, id, 0);
                used_room_ids[1] = id;
            }
            else
            {
                /* No puzzle exists: substitute CLEAR at mid depth */
                int id = graph->rooms[subst_clear_idx].id;
                if (id_in_used(used_room_ids, 4, id))
                    id = -1;
                if (id >= 0)
                {
                    APPEND_STEP(ROGUE_OBJ_CLEAR, id, 0);
                    used_room_ids[1] = id;
                }
            }
            break;
        case TOK_PUZZLE_OPTIONAL:
            if (puzzle_idx >= 0)
            {
                int id = graph->rooms[puzzle_idx].id;
                if (id_in_used(used_room_ids, 4, id))
                    id = -1;
                if (id >= 0)
                {
                    APPEND_STEP(ROGUE_OBJ_PUZZLE_COMPLETE, id, 0);
                    used_room_ids[1] = id;
                }
            }
            /* else omit */
            break;
        case TOK_CLEAR:
        {
            int id = graph->rooms[mid_idx].id;
            if (id_in_used(used_room_ids, 4, id))
                id = -1;
            if (id < 0)
            {
                /* find nearest distinct room by depth */
                int best = -1, bestd = 9999;
                for (int i = 0; i < n; ++i)
                {
                    int cand = graph->rooms[i].id;
                    int dup = id_in_used(used_room_ids, 4, cand);
                    if (dup)
                        continue;
                    int d = rdepths[i] > target_depth ? (rdepths[i] - target_depth)
                                                      : (target_depth - rdepths[i]);
                    if (d < bestd)
                    {
                        bestd = d;
                        best = cand;
                    }
                }
                id = (best >= 0) ? best : graph->rooms[0].id;
            }
            APPEND_STEP(ROGUE_OBJ_CLEAR, id, 0);
            used_room_ids[2] = id;
        }
        break;
        case TOK_GATE:
        {
            /* Expand GATE(KEYSTONE) to COLLECT(KEYSTONE) then ACTIVATE(GATE). Use shallow for gate
             * and place keystone earlier (id from start or shallow distinct). */
            int gate_room = graph->rooms[shallow_idx].id;
            if (id_in_used(used_room_ids, 4, gate_room))
                gate_room = -1;
            if (gate_room < 0)
                gate_room = graph->rooms[0].id;
            int key_room = graph->rooms[0].id;
            if (key_room == gate_room && n > 1)
                key_room = graph->rooms[(shallow_idx + 1) % n].id;
            APPEND_STEP(ROGUE_OBJ_COLLECT, key_room, ROGUE_OBJ_PARAM_KEYSTONE);
            APPEND_STEP(ROGUE_OBJ_ACTIVATE, gate_room, ROGUE_OBJ_PARAM_GATE);
            used_room_ids[0] = gate_room; /* mark gate loc as used */
        }
        break;
        case TOK_BOSS:
        {
            int id = graph->rooms[far_idx].id;
            APPEND_STEP(ROGUE_OBJ_BOSS, id, 0);
        }
        break;
        default:
            break;
        }
        if (steps >= max_steps)
            break;
    }

#undef APPEND_STEP

    free(rdepths);
    return steps;
}
