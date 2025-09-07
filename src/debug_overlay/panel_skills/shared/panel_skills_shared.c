#include "panel_skills_shared.h"
#include "../../../core/skills/skills_validate.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static int g_last_valid_ok = 1;
static char g_last_valid_msg[128] = "OK";

void panel_skills_refresh_validation(void)
{
    char emsg[192] = {0};
    int ok = (rogue_skills_validate_all(emsg, (int) sizeof emsg) == 0);
    g_last_valid_ok = ok;
    if (!ok)
    {
        const char* src = (emsg[0] ? emsg : "error");
        size_t i = 0;
        while (src[i] && i + 1 < sizeof g_last_valid_msg)
        {
            g_last_valid_msg[i] = src[i];
            ++i;
        }
        g_last_valid_msg[i] = '\0';
    }
    else
    {
        g_last_valid_msg[0] = 'O';
        g_last_valid_msg[1] = 'K';
        g_last_valid_msg[2] = '\0';
    }
}

int panel_skills_last_valid_ok(void) { return g_last_valid_ok; }

const char* panel_skills_last_valid_msg(void) { return g_last_valid_msg; }

const char* panel_skills_overrides_path(void) { return "build/skills_overrides.json"; }

const char* panel_skills_base_skills_path(void) { return "assets/skills_uhf87f.json"; }

/* Forward decls from debug API */
int rogue_skill_debug_validate(char* err, int err_cap);
int rogue_skill_debug_save_overrides(const char* path);

void panel_skills_save_overrides_and_refresh(void)
{
    (void) rogue_skill_debug_save_overrides(panel_skills_overrides_path());
    panel_skills_refresh_validation();
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
