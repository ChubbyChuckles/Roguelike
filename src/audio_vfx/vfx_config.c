#include "vfx_config.h"
#include "../util/cfg_parser.h"
#include "../util/hot_reload.h"
#include "../util/log.h"
#include "effects.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Simple, portable case-insensitive equality check (ASCII only)
static int ci_equal(const char* a, const char* b)
{
    if (!a || !b)
        return 0;
    while (*a && *b)
    {
        unsigned char ca = (unsigned char) *a++;
        unsigned char cb = (unsigned char) *b++;
        if (toupper(ca) != toupper(cb))
            return 0;
    }
    return *a == '\0' && *b == '\0';
}

static int parse_layer(const char* s)
{
    if (!s || !*s)
        return -1;
    if (isdigit((unsigned char) s[0]))
    {
        int v = atoi(s);
        if (v >= 0 && v <= 3)
            return v;
        return -1;
    }
    char buf[8] = {0};
    // Safe copy to upper-case buffer (avoid MSVC strncpy warning)
    size_t n = 0;
    while (s[n] && n < sizeof(buf) - 1)
    {
        buf[n] = (char) toupper((unsigned char) s[n]);
        ++n;
    }
    buf[n] = '\0';
    if (strcmp(buf, "BG") == 0)
        return (int) ROGUE_VFX_LAYER_BG;
    if (strcmp(buf, "MID") == 0)
        return (int) ROGUE_VFX_LAYER_MID;
    if (strcmp(buf, "FG") == 0)
        return (int) ROGUE_VFX_LAYER_FG;
    if (strcmp(buf, "UI") == 0)
        return (int) ROGUE_VFX_LAYER_UI;
    return -1;
}

/* ---- Validation error tracking (Phase 4.2) ---- */
#ifndef ROGUE_VFX_CFG_ERR_CAP
#define ROGUE_VFX_CFG_ERR_CAP 32
#endif
static char g_cfg_errors[ROGUE_VFX_CFG_ERR_CAP][256];
static int g_cfg_err_count = 0;

static void cfg_err_clear(void) { g_cfg_err_count = 0; }

static void cfg_err_push(const char* fmt, const char* a)
{
    if (g_cfg_err_count >= ROGUE_VFX_CFG_ERR_CAP)
        return;
    if (!fmt)
        return;
#if defined(_MSC_VER)
    _snprintf_s(g_cfg_errors[g_cfg_err_count], sizeof g_cfg_errors[g_cfg_err_count], _TRUNCATE, fmt,
                a ? a : "");
#else
    snprintf(g_cfg_errors[g_cfg_err_count], sizeof g_cfg_errors[g_cfg_err_count], fmt, a ? a : "");
#endif
    g_cfg_err_count++;
}

int rogue_vfx_load_cfg(const char* filename, int* out_loaded_count)
{
    cfg_err_clear();
    if (out_loaded_count)
        *out_loaded_count = 0;
    RogueCfgParseResult* pr = rogue_cfg_parse_file(filename);
    if (!pr || !pr->parse_success)
    {
        ROGUE_LOG_ERROR("VFX cfg parse failed: %s", filename ? filename : "<null>");
        cfg_err_push("parse failed: %s", filename ? filename : "<null>");
        if (pr)
            rogue_cfg_free_parse_result(pr);
        return -1;
    }
    if (pr->detected_format != ROGUE_CFG_FORMAT_CSV)
    {
        ROGUE_LOG_ERROR("VFX cfg expected CSV format: %s", filename);
        cfg_err_push("expected CSV format: %s", filename ? filename : "<null>");
        rogue_cfg_free_parse_result(pr);
        return -2;
    }
    int loaded = 0;
    for (int i = 0; i < pr->data.csv.record_count; ++i)
    {
        RogueCfgRecord* rec = &pr->data.csv.records[i];
        if (rec->count < 7)
        {
            ROGUE_LOG_WARN("Skipping VFX row (need 7 fields, got %d)", rec->count);
            cfg_err_push("row %d: wrong field count", "");
            continue;
        }
        const char* id = rec->values[0];
        int layer = parse_layer(rec->values[1]);
        uint32_t lifetime_ms = (uint32_t) strtoul(rec->values[2], NULL, 10);
        int world_space = (strcmp(rec->values[3], "1") == 0 || ci_equal(rec->values[3], "true"));
        float emit_hz = (float) atof(rec->values[4]);
        uint32_t p_life = (uint32_t) strtoul(rec->values[5], NULL, 10);
        int p_max = atoi(rec->values[6]);
        if (!id[0] || layer < 0)
        {
            ROGUE_LOG_WARN("Skipping VFX row due to bad id/layer");
            cfg_err_push("row %d: bad id/layer", "");
            continue;
        }
        if (rogue_vfx_registry_register(id, (RogueVfxLayer) layer, lifetime_ms, world_space) != 0)
        {
            ROGUE_LOG_WARN("VFX register failed for id=%s", id);
            cfg_err_push("register failed for id=%s", id);
            continue;
        }
        (void) rogue_vfx_registry_set_emitter(id, emit_hz, p_life, p_max);
        ++loaded;
    }
    rogue_cfg_free_parse_result(pr);
    if (out_loaded_count)
        *out_loaded_count = loaded;
    return 0;
}

/* ---- Hot reload wiring (Phase 4.2) ---- */
static void vfx_cfg_reload_cb(const char* path, void* user_data)
{
    (void) user_data;
    int n = 0;
    int rc = rogue_vfx_load_cfg(path, &n);
    if (rc == 0)
    {
        ROGUE_LOG_INFO("VFX cfg hot-reloaded: %s (loaded %d entries)", path, n);
    }
    else
    {
        ROGUE_LOG_ERROR("VFX cfg hot-reload failed rc=%d: %s", rc, path);
    }
}

int rogue_vfx_config_watch(const char* filename)
{
    if (!filename || !*filename)
        return -1;
    /* Use filename as id; acceptable for single-file watch in tests/tools. */
    int rc = rogue_hot_reload_register(filename, filename, vfx_cfg_reload_cb, NULL);
    return rc;
}

int rogue_vfx_last_cfg_error_count(void) { return g_cfg_err_count; }

int rogue_vfx_last_cfg_error_get(int index, char* out, unsigned out_sz)
{
    if (!out || out_sz == 0)
        return -1;
    if (index < 0 || index >= g_cfg_err_count)
        return -1;
#if defined(_MSC_VER)
    strncpy_s(out, out_sz, g_cfg_errors[index], _TRUNCATE);
#else
    strncpy(out, g_cfg_errors[index], out_sz - 1);
    out[out_sz - 1] = '\0';
#endif
    return 0;
}
