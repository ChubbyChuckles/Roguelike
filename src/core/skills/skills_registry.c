/* Skill registry & ranking management (extracted from monolithic skills.c) */
#include "../../util/file_search.h"
#include "../../util/log.h"
#include "../app_state.h"
#include "../buffs.h"
#include "../integration/event_bus.h"
#include "skills_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward (effect_spec.c) */
void rogue_effect_apply(int effect_spec_id, double now_ms);
/* Forward (persistence.c) */
void rogue_persistence_save_player_stats(void);

/* Former static globals */
struct RogueSkillDef* g_skill_defs_internal = NULL;
struct RogueSkillState* g_skill_states_internal = NULL;
/* Canary instrumentation (Release too) to detect buffer overruns writing past last element. */
unsigned int g_skill_defs_canary = 0xABCD1234u;
unsigned int g_skill_states_canary = 0xBEEF5678u;
int g_skill_capacity_internal = 0;
int g_skill_count_internal = 0;

#define ROGUE_MAX_SYNERGIES 16
static int g_synergy_totals[ROGUE_MAX_SYNERGIES];

void rogue_skills_recompute_synergies(void)
{
    for (int i = 0; i < ROGUE_MAX_SYNERGIES; i++)
        g_synergy_totals[i] = 0;
    for (int i = 0; i < g_skill_count_internal; i++)
    {
        const RogueSkillDef* d = &g_skill_defs_internal[i];
        const RogueSkillState* st = &g_skill_states_internal[i];
        if (d->is_passive && d->synergy_id >= 0 && d->synergy_id < ROGUE_MAX_SYNERGIES)
        {
            g_synergy_totals[d->synergy_id] += st->rank * d->synergy_value_per_rank;
        }
    }
}

void rogue_skills_ensure_capacity(int min_cap)
{
    if (g_skill_capacity_internal >= min_cap)
        return;
    int new_cap = g_skill_capacity_internal ? g_skill_capacity_internal * 2 : 8;
    if (new_cap < min_cap)
        new_cap = min_cap;
    RogueSkillDef* nd =
        (RogueSkillDef*) realloc(g_skill_defs_internal, (size_t) new_cap * sizeof(RogueSkillDef));
    if (!nd)
        return;
    g_skill_defs_internal = nd;
    g_skill_defs_canary = 0xABCD1234u;
    RogueSkillState* ns = (RogueSkillState*) realloc(g_skill_states_internal,
                                                     (size_t) new_cap * sizeof(RogueSkillState));
    if (!ns)
        return;
    g_skill_states_internal = ns;
    g_skill_states_canary = 0xBEEF5678u;
    for (int i = g_skill_capacity_internal; i < new_cap; i++)
    {
        RogueSkillState* s = &g_skill_states_internal[i];
        memset(s, 0, sizeof *s);
    }
    g_skill_capacity_internal = new_cap;
}

void rogue_skills_init(void)
{
    g_skill_defs_internal = NULL;
    g_skill_states_internal = NULL;
    g_skill_capacity_internal = 0;
    g_skill_count_internal = 0;
    /* Reset canary sentinels on each init so repeated init/shutdown cycles in a single
        process (unit tests) don't falsely report corruption. These are set to 0 at the end of
        shutdown to help catch use-after-free and buffer overruns; re-arm them here. */
    g_skill_defs_canary = 0xABCD1234u;
    g_skill_states_canary = 0xBEEF5678u;
    g_app.skill_defs = NULL;
    g_app.skill_states = NULL;
    g_app.skill_count = 0;
#ifdef ROGUE_HAVE_SDL
    g_app.skill_icon_textures = NULL;
#endif
    for (int i = 0; i < 10; i++)
        g_app.skill_bar[i] = -1;
    g_app.talent_points = 0;
    for (int i = 0; i < ROGUE_MAX_SYNERGIES; i++)
        g_synergy_totals[i] = 0;
}

void rogue_skills_shutdown(void)
{
    if (g_skill_defs_canary != 0xABCD1234u || g_skill_states_canary != 0xBEEF5678u)
    {
        fprintf(stderr, "SKILL CANARY CORRUPTION AT SHUTDOWN\n");
    }
#ifdef ROGUE_HAVE_SDL
    if (g_app.skill_icon_textures)
    {
        for (int i = 0; i < g_skill_count_internal; i++)
        {
            rogue_texture_destroy(&g_app.skill_icon_textures[i]);
        }
        free(g_app.skill_icon_textures);
        g_app.skill_icon_textures = NULL;
    }
#endif
    for (int i = 0; i < g_skill_count_internal; i++)
    {
        if (g_skill_defs_internal)
        {
            if (g_skill_defs_internal[i].name)
            {
                free((char*) g_skill_defs_internal[i].name);
                g_skill_defs_internal[i].name = NULL;
            }
            if (g_skill_defs_internal[i].icon)
            {
                free((char*) g_skill_defs_internal[i].icon);
                g_skill_defs_internal[i].icon = NULL;
            }
        }
    }
    if (g_skill_defs_internal)
    {
        memset(g_skill_defs_internal, 0,
               sizeof(RogueSkillDef) * (size_t) g_skill_capacity_internal);
        free(g_skill_defs_internal);
        g_skill_defs_internal = NULL;
    }
    if (g_skill_states_internal)
    {
        memset(g_skill_states_internal, 0,
               sizeof(RogueSkillState) * (size_t) g_skill_capacity_internal);
        free(g_skill_states_internal);
        g_skill_states_internal = NULL;
    }
    g_skill_capacity_internal = 0;
    g_skill_count_internal = 0;
    g_app.skill_defs = NULL;
    g_app.skill_states = NULL;
    g_app.skill_count = 0;
    g_skill_defs_canary = 0;
    g_skill_states_canary = 0;
}

int rogue_skill_register(const RogueSkillDef* def)
{
    if (!def)
        return -1;
    rogue_skills_ensure_capacity(g_skill_count_internal + 1);
    RogueSkillDef* d = &g_skill_defs_internal[g_skill_count_internal];
    /* Copy value fields then deep-copy owned strings to avoid freeing string literals later. */
    *d = *def;
    d->id = g_skill_count_internal;
    if (def->name)
    {
#if defined(_MSC_VER)
        char* dup = _strdup(def->name);
#else
        char* dup = strdup(def->name);
#endif
        if (dup)
            d->name = dup; /* if alloc fails retain original (will not be freed) */
    }
    if (def->icon)
    {
#if defined(_MSC_VER)
        char* dup2 = _strdup(def->icon);
#else
        char* dup2 = strdup(def->icon);
#endif
        if (dup2)
            d->icon = dup2;
    }
    RogueSkillState* st = &g_skill_states_internal[g_skill_count_internal];
    memset(st, 0, sizeof *st);
    st->charges_cur = d->max_charges > 0 ? d->max_charges : 0;
    g_skill_count_internal++;
    g_app.skill_defs = g_skill_defs_internal;
    g_app.skill_states = g_skill_states_internal;
    g_app.skill_count = g_skill_count_internal;
#ifdef ROGUE_HAVE_SDL
    /* Track icon texture array size separately so we never index beyond allocated memory on realloc
     * failure. */
    static int s_skill_icon_tex_count = 0; /* number of texture slots currently allocated */
    if (g_skill_count_internal > s_skill_icon_tex_count)
    {
        RogueTexture* nt = (RogueTexture*) realloc(g_app.skill_icon_textures,
                                                   sizeof(RogueTexture) * g_skill_count_internal);
        if (nt)
        {
            g_app.skill_icon_textures = nt;
            /* Initialize new slots (could be more than one if we grow elsewhere) */
            for (int i = s_skill_icon_tex_count; i < g_skill_count_internal; i++)
            {
                g_app.skill_icon_textures[i].handle = NULL;
                g_app.skill_icon_textures[i].w = 0;
                g_app.skill_icon_textures[i].h = 0;
            }
            s_skill_icon_tex_count = g_skill_count_internal;
        }
    }
#endif
    return d->id;
}

int rogue_skill_rank_up(int id)
{
    if (g_skill_defs_canary != 0xABCD1234u || g_skill_states_canary != 0xBEEF5678u)
    {
        fprintf(stderr, "SKILL CANARY CORRUPTION BEFORE RANK_UP id=%d\n", id);
        abort();
    }
    if (id < 0 || id >= g_skill_count_internal)
        return -1;
    RogueSkillState* st = &g_skill_states_internal[id];
    const RogueSkillDef* def = &g_skill_defs_internal[id];
    if (st->rank >= def->max_rank)
        return st->rank;
    if (g_app.talent_points <= 0)
        return -1;
    /* Phase 3.6.2: prerequisite gating with progression level gates.
       Use data-driven ring strength as a proxy for minimum level: require lvl >= 5*skill_strength.
       Default strength (0) implies no extra gate beyond talent points. */
    if (st->rank == 0)
    {
        int required_level = (def->skill_strength > 0) ? (def->skill_strength * 5) : 1;
        /* Some unit tests call into skills without initializing the full app/player; treat
           non-positive player levels as level 1 for gating to preserve backwards compatibility. */
        int player_level = g_app.player.level;
        if (player_level < 1)
            player_level = 1;
        if (player_level < required_level)
        {
            ROGUE_LOG_INFO("Skill unlock gated: id=%d name=%s player_lvl=%d required=%d", id,
                           def->name ? def->name : "<noname>", player_level, required_level);
            return -1;
        }
    }
    st->rank++;
    g_app.talent_points--;
    g_app.stats_dirty = 1;
    rogue_skills_recompute_synergies();
    rogue_persistence_save_player_stats();
    /* Emit SKILL_UNLOCKED event on first unlock (rank 1). Reuse xp_gained.source_id to carry
       skill_id per existing UI/persistence bridges. */
    if (st->rank == 1)
    {
        RogueEventPayload p;
        memset(&p, 0, sizeof p);
        p.xp_gained.player_id = 0;
        p.xp_gained.xp_amount = 0;
        p.xp_gained.source_type = 0;
        p.xp_gained.source_id = (uint32_t) id; /* skill id */
        rogue_event_publish(ROGUE_EVENT_SKILL_UNLOCKED, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0x534B494C,
                            "skills");
    }
    return st->rank;
}

int rogue_skill_synergy_total(int synergy_id)
{
    if (synergy_id < 0 || synergy_id >= ROGUE_MAX_SYNERGIES)
        return 0;
    return g_synergy_totals[synergy_id];
}

/* Query passthroughs */
const RogueSkillDef* rogue_skill_get_def(int id)
{
    if (id < 0 || id >= g_skill_count_internal)
        return NULL;
    return &g_skill_defs_internal[id];
}
const RogueSkillState* rogue_skill_get_state(int id)
{
    if (id < 0 || id >= g_skill_count_internal)
        return NULL;
    return &g_skill_states_internal[id];
}

/* Data-driven loader: supports legacy CSV (.cfg) and JSON (.json) formats.
   CSV columns:
   name,icon,max_rank,base_cd,cd_red,is_passive,tags,synergy_id,synergy_per_rank,mana,ap,max_charges,charge_ms,cast_ms,input_buf,min_weave,early_cancel_pct,cast_type,combo_builder,combo_spender,effect_spec_id
   JSON shape: [ { "name":..., "icon":..., "max_rank":N, ... } ] matching RogueSkillDef fields by
   key.
*/
static int rogue__ends_with(const char* s, const char* suf)
{
    if (!s || !suf)
        return 0;
    size_t ls = strlen(s), lf = strlen(suf);
    if (lf > ls)
        return 0;
    return _stricmp ? (int) (_stricmp(s + ls - lf, suf) == 0)
                    : (int) (strcmp(s + ls - lf, suf) == 0);
}

/* Minimal JSON value skipper / parser (expects well-formed input generated by our tools). */
static const char* rogue__skip_ws(const char* s)
{
    while (*s && (unsigned char) *s <= 32)
        ++s;
    return s;
}
static const char* rogue__parse_string(const char* s, char* out, int out_sz)
{
    s = rogue__skip_ws(s);
    if (*s != '"')
        return NULL;
    ++s;
    int i = 0;
    while (*s && *s != '"')
    {
        if (*s == '\\' && s[1])
            s++;
        if (i + 1 < out_sz)
            out[i++] = *s;
        ++s;
    }
    if (*s != '"')
        return NULL;
    out[i] = '\0';
    return s + 1;
}
static const char* rogue__parse_number(const char* s, double* out)
{
    s = rogue__skip_ws(s);
    char* end = NULL;
    double v = strtod(s, &end);
    if (end == s)
        return NULL;
    *out = v;
    return end;
}
/* Portable strdup wrapper */
#if defined(_MSC_VER)
#define ROGUE_STRDUP _strdup
#else
#define ROGUE_STRDUP strdup
#endif

static int rogue__json_load(const char* buf)
{
    const char* s = rogue__skip_ws(buf);
    if (*s != '[')
        return 0;
    ++s;
    int loaded = 0;
    char key[64];
    char sval[512];
    while (1)
    {
        s = rogue__skip_ws(s);
        if (*s == ']')
        {
            ++s;
            break;
        }
        if (*s != '{')
            return loaded;
        ++s;
        RogueSkillDef def;
        memset(&def, 0, sizeof def);
        def.id = -1;
        def.max_rank = 1;
        def.synergy_id = -1;
        def.skill_strength = 0;
        int done_obj = 0;
        while (!done_obj)
        {
            s = rogue__skip_ws(s);
            if (*s == '}')
            {
                ++s;
                break;
            }
            const char* ns = rogue__parse_string(s, key, sizeof key);
            if (!ns)
                return loaded;
            s = rogue__skip_ws(ns);
            if (*s != ':')
                return loaded;
            s++;
            s = rogue__skip_ws(s);
            if (*s == '"')
            {
                const char* vs = rogue__parse_string(s, sval, sizeof sval);
                if (!vs)
                    return loaded;
                s = rogue__skip_ws(vs);
                if (strcmp(key, "name") == 0)
                {
                    def.name = ROGUE_STRDUP(sval);
                }
                else if (strcmp(key, "icon") == 0)
                {
                    def.icon = ROGUE_STRDUP(sval);
                }
            }
            else if ((*s >= '0' && *s <= '9') || *s == '-')
            {
                double num;
                const char* vs = rogue__parse_number(s, &num);
                if (!vs)
                    return loaded;
                s = rogue__skip_ws(vs);
                if (strcmp(key, "max_rank") == 0)
                    def.max_rank = (int) num;
                else if (strcmp(key, "skill_strength") == 0)
                    def.skill_strength = (int) num;
                else if (strcmp(key, "base_cooldown_ms") == 0)
                    def.base_cooldown_ms = (float) num;
                else if (strcmp(key, "cooldown_reduction_ms_per_rank") == 0)
                    def.cooldown_reduction_ms_per_rank = (float) num;
                else if (strcmp(key, "is_passive") == 0)
                    def.is_passive = (int) num;
                else if (strcmp(key, "tags") == 0)
                    def.tags = (int) num;
                else if (strcmp(key, "synergy_id") == 0)
                    def.synergy_id = (int) num;
                else if (strcmp(key, "synergy_value_per_rank") == 0)
                    def.synergy_value_per_rank = (int) num;
                else if (strcmp(key, "resource_cost_mana") == 0)
                    def.resource_cost_mana = (int) num;
                else if (strcmp(key, "action_point_cost") == 0)
                    def.action_point_cost = (int) num;
                else if (strcmp(key, "max_charges") == 0)
                    def.max_charges = (int) num;
                else if (strcmp(key, "charge_recharge_ms") == 0)
                    def.charge_recharge_ms = (float) num;
                else if (strcmp(key, "cast_time_ms") == 0)
                    def.cast_time_ms = (float) num;
                else if (strcmp(key, "input_buffer_ms") == 0)
                    def.input_buffer_ms = (unsigned short) num;
                else if (strcmp(key, "min_weave_ms") == 0)
                    def.min_weave_ms = (unsigned short) num;
                else if (strcmp(key, "early_cancel_min_pct") == 0)
                    def.early_cancel_min_pct = (unsigned char) num;
                else if (strcmp(key, "cast_type") == 0)
                    def.cast_type = (unsigned char) num;
                else if (strcmp(key, "combo_builder") == 0)
                    def.combo_builder = (unsigned char) num;
                else if (strcmp(key, "combo_spender") == 0)
                    def.combo_spender = (unsigned char) num;
                else if (strcmp(key, "effect_spec_id") == 0)
                    def.effect_spec_id = (int) num;
            }
            s = rogue__skip_ws(s);
            if (*s == ',')
            {
                s++;
                continue;
            }
        }
        int new_id = rogue_skill_register(&def);
        if (new_id >= 0)
        {
#ifdef ROGUE_HAVE_SDL
            /* Only load if texture slot definitely allocated (implicit: slot handle NULL). */
            extern void*
                rogue__skill_icon_slot_guard; /* no symbol; silence unused warnings if none */
            if (def.icon && g_app.skill_icon_textures && new_id < g_skill_count_internal)
            {
                char attempt[512];
                if (strncmp(def.icon, "assets/", 7) == 0 ||
                    strncmp(def.icon, "../assets/", 10) == 0)
                    snprintf(attempt, sizeof attempt, "%s", def.icon);
                else
                    snprintf(attempt, sizeof attempt, "assets/%s", def.icon);
                if (!rogue_texture_load(&g_app.skill_icon_textures[new_id], attempt))
                {
                    const char* s1 = strrchr(attempt, '/');
                    const char* s2 = strrchr(attempt, '\\');
                    const char* last = s2 ? ((s1 && s1 > s2) ? s1 : s2) : (s1 ? s1 : NULL);
                    const char* fname = last ? last + 1 : attempt;
                    char icon_res[640];
                    if (rogue_file_search_project(fname, icon_res, sizeof icon_res) &&
                        rogue_texture_load(&g_app.skill_icon_textures[new_id], icon_res))
                    {
                        ROGUE_LOG_WARN("Icon fallback(json): '%s' -> '%s'", attempt, icon_res);
                    }
                }
            }
#endif
            loaded++;
        }
        /* We always duplicate on registration; free temporary strings allocated during parse */
        if (def.name)
            free((char*) def.name);
        if (def.icon)
            free((char*) def.icon);
        s = rogue__skip_ws(s);
        if (*s == ',')
        {
            s++;
            continue;
        }
    }
    return loaded;
}
int rogue_skills_load_from_cfg(const char* path)
{
    if (!path)
        return 0;
    ROGUE_LOG_INFO("Loading skills cfg: %s", path);
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0)
        f = NULL;
#else
    f = fopen(path, "rb");
#endif
    char resolved[640];
    resolved[0] = '\0';
    if (!f)
    {
        const char* slash1 = strrchr(path, '/');
        const char* slash2 = strrchr(path, '\\');
        const char* last =
            slash2 ? ((slash1 && slash1 > slash2) ? slash1 : slash2) : (slash1 ? slash1 : NULL);
        const char* base = last ? last + 1 : path;
        if (rogue_file_search_project(base, resolved, sizeof resolved))
        {
            ROGUE_LOG_WARN("skills cfg not found at '%s' – fallback located '%s'", path, resolved);
#if defined(_MSC_VER)
            if (fopen_s(&f, resolved, "rb") != 0)
                f = NULL;
#else
            f = fopen(resolved, "rb");
#endif
        }
        if (!f)
        {
            ROGUE_LOG_ERROR("Failed to open skills cfg: %s (fallback search failed)", path);
            return 0;
        }
    }
    /* If JSON extension, load whole file then parse JSON */
    if (strstr(path, ".json") != NULL)
    {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        if (sz < 0)
        {
            fclose(f);
            return 0;
        }
        fseek(f, 0, SEEK_SET);
        char* buf = (char*) malloc((size_t) sz + 1);
        if (!buf)
        {
            fclose(f);
            return 0;
        }
        size_t rd = fread(buf, 1, (size_t) sz, f);
        buf[rd] = '\0';
        fclose(f);
        int loaded_json = rogue__json_load(buf);
        free(buf);
        return loaded_json;
    }
    char line[1024];
    int loaded = 0;
    while (fgets(line, sizeof line, f))
    {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;
        /* tokenize by comma */
        char* parts[24];
        int pc = 0;
        char* p = line;
        while (pc < 24)
        {
            char* comma = strchr(p, ',');
            if (comma)
            {
                *comma = '\0';
                parts[pc++] = p;
                p = comma + 1;
            }
            else
            { /* trim newline */
                char* nl = strchr(p, '\n');
                if (nl)
                    *nl = '\0';
                parts[pc++] = p;
                break;
            }
        }
        if (pc < 3)
            continue; /* need at least name/icon/max_rank */
        RogueSkillDef def;
        memset(&def, 0, sizeof def);
        def.id = -1;
        /* duplicate strings safely */
#if defined(_MSC_VER)
        size_t nlen = strlen(parts[0]);
        char* ndup = (char*) malloc(nlen + 1);
        if (ndup)
        {
            memcpy(ndup, parts[0], nlen + 1);
        }
        else
        {
            continue;
        }
        size_t ilen = strlen(parts[1]);
        char* idup = (char*) malloc(ilen + 1);
        if (idup)
        {
            memcpy(idup, parts[1], ilen + 1);
        }
        else
        {
            free(ndup);
            continue;
        }
        def.name = ndup;
        def.icon = idup;
#else
        def.name = strdup(parts[0]);
        def.icon = strdup(parts[1]);
#endif
        int idx = 2;
        def.max_rank = (idx < pc) ? atoi(parts[idx++]) : 1;
        def.base_cooldown_ms = (float) ((idx < pc) ? atof(parts[idx++]) : 0.0);
        def.cooldown_reduction_ms_per_rank = (float) ((idx < pc) ? atof(parts[idx++]) : 0.0);
        def.is_passive = (idx < pc) ? atoi(parts[idx++]) : 0;
        def.tags = (idx < pc) ? atoi(parts[idx++]) : 0;
        def.synergy_id = (idx < pc) ? atoi(parts[idx++]) : -1;
        def.synergy_value_per_rank = (idx < pc) ? atoi(parts[idx++]) : 0;
        def.resource_cost_mana = (idx < pc) ? atoi(parts[idx++]) : 0;
        def.action_point_cost = (idx < pc) ? atoi(parts[idx++]) : 0;
        def.max_charges = (idx < pc) ? atoi(parts[idx++]) : 0;
        def.charge_recharge_ms = (float) ((idx < pc) ? atof(parts[idx++]) : 0.0);
        def.cast_time_ms = (float) ((idx < pc) ? atof(parts[idx++]) : 0.0);
        def.input_buffer_ms = (unsigned short) ((idx < pc) ? atoi(parts[idx++]) : 0);
        def.min_weave_ms = (unsigned short) ((idx < pc) ? atoi(parts[idx++]) : 0);
        def.early_cancel_min_pct = (unsigned char) ((idx < pc) ? atoi(parts[idx++]) : 0);
        def.cast_type = (unsigned char) ((idx < pc) ? atoi(parts[idx++]) : 0);
        def.combo_builder = (unsigned char) ((idx < pc) ? atoi(parts[idx++]) : 0);
        def.combo_spender = (unsigned char) ((idx < pc) ? atoi(parts[idx++]) : 0);
        def.effect_spec_id = (idx < pc) ? atoi(parts[idx++]) : 0;
        int new_id = rogue_skill_register(&def);
        if (new_id >= 0)
        {
#ifdef ROGUE_HAVE_SDL
            if (def.icon && g_app.skill_icon_textures)
            {
                char attempt[512];
                if (strncmp(def.icon, "assets/", 7) == 0 ||
                    strncmp(def.icon, "../assets/", 10) == 0)
                    snprintf(attempt, sizeof attempt, "%s", def.icon);
                else
                    snprintf(attempt, sizeof attempt, "assets/%s", def.icon);
                if (rogue_texture_load(&g_app.skill_icon_textures[new_id], attempt))
                {
                    ROGUE_LOG_INFO("Skill icon loaded id=%d name=%s path=%s", new_id, def.name,
                                   attempt);
                }
                else
                {
                    const char* s1 = strrchr(attempt, '/');
                    const char* s2 = strrchr(attempt, '\\');
                    const char* last = s2 ? ((s1 && s1 > s2) ? s1 : s2) : (s1 ? s1 : NULL);
                    const char* fname = last ? last + 1 : attempt;
                    char icon_res[640];
                    if (rogue_file_search_project(fname, icon_res, sizeof icon_res))
                    {
                        ROGUE_LOG_WARN("Icon fallback: '%s' -> '%s'", attempt, icon_res);
                        if (rogue_texture_load(&g_app.skill_icon_textures[new_id], icon_res))
                        {
                            ROGUE_LOG_INFO("Skill icon loaded via fallback id=%d name=%s path=%s",
                                           new_id, def.name, icon_res);
                        }
                        else
                        {
                            ROGUE_LOG_WARN("Skill icon failed after fallback id=%d name=%s path=%s",
                                           new_id, def.name, icon_res);
                        }
                    }
                    else
                    {
                        ROGUE_LOG_WARN(
                            "Skill icon missing (no fallback match) id=%d name=%s original=%s",
                            new_id, def.name, attempt);
                    }
                }
            }
#endif
            loaded++;
        }
        /* Free temporary strings (registration duplicates) */
        if (def.name)
            free((char*) def.name);
        if (def.icon)
            free((char*) def.icon);
    }
    fclose(f);
    return loaded;
}
