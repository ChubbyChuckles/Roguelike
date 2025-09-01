#include "overlay_prefs.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../content/json_io.h"
#include <stdlib.h>
#include <string.h>

typedef struct PrefKV
{
    char key[64];
    int value;
} PrefKV;
#define PREF_MAX 128
static PrefKV g_kv[PREF_MAX];
static int g_kv_count = 0;

static const char* prefs_path(void) { return "build/overlay_prefs.json"; }

static int kv_find(const char* key)
{
    if (!key)
        return -1;
    for (int i = 0; i < g_kv_count; ++i)
        if (strcmp(g_kv[i].key, key) == 0)
            return i;
    return -1;
}

static void prefs_save(void)
{
    char buf[4096];
    int n = 0;
    n += snprintf(buf + n, sizeof buf - n, "{\n  \"ints\": {\n");
    for (int i = 0; i < g_kv_count; ++i)
        n += snprintf(buf + n, sizeof buf - n, "    \"%s\": %d%s\n", g_kv[i].key, g_kv[i].value,
                      (i + 1 < g_kv_count) ? "," : "");
    n += snprintf(buf + n, sizeof buf - n, "  }\n}\n");
    char err[128];
    (void) json_io_write_atomic(prefs_path(), buf, (size_t) n, err, (int) sizeof err);
}

static void prefs_load(void)
{
    char* data = NULL;
    size_t len = 0;
    char err[128];
    if (json_io_read_file(prefs_path(), &data, &len, err, (int) sizeof err) != 0 || !data)
        return;
    const char* p = data;
    /* naive parse: find "key": number pairs */
    while ((p = strchr(p, '"')) != NULL)
    {
        const char* k0 = p + 1;
        const char* k1 = strchr(k0, '"');
        if (!k1)
            break;
        size_t kl = (size_t) (k1 - k0);
        if (kl == 0 || kl >= 64)
        {
            p = k1 ? k1 + 1 : k0;
            continue;
        }
        const char* colon = strchr(k1, ':');
        if (!colon)
            break;
        int v = 0;
        if (sscanf(colon + 1, "%d", &v) != 1)
        {
            p = k1 + 1;
            continue;
        }
        if (g_kv_count < PREF_MAX)
        {
            memcpy(g_kv[g_kv_count].key, k0, kl);
            g_kv[g_kv_count].key[kl] = '\0';
            g_kv[g_kv_count].value = v;
            g_kv_count++;
        }
        p = k1 + 1;
    }
    free(data);
}

void overlay_prefs_init(void)
{
    g_kv_count = 0;
    prefs_load();
}

int overlay_prefs_get_int(const char* key, int def)
{
    int i = kv_find(key);
    return (i >= 0) ? g_kv[i].value : def;
}

void overlay_prefs_set_int(const char* key, int value)
{
    if (!key)
        return;
    int i = kv_find(key);
    if (i < 0 && g_kv_count < PREF_MAX)
    {
        i = g_kv_count++;
        strncpy(g_kv[i].key, key, 63);
        g_kv[i].key[63] = '\0';
    }
    if (i >= 0)
    {
        g_kv[i].value = value;
        prefs_save();
    }
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
