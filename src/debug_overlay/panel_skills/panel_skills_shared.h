#ifndef PANEL_SKILLS_SHARED_H
#define PANEL_SKILLS_SHARED_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

    /* Shared helpers for the Skills overlay panel. */

    /* Recompute and cache validation status text (uses core validator). */
    void panel_skills_refresh_validation(void);

    /* Get last cached validation status. */
    int panel_skills_last_valid_ok(void);
    const char* panel_skills_last_valid_msg(void);

    /* Default paths used by the panel for JSON overrides and base skills. */
    const char* panel_skills_overrides_path(void);
    const char* panel_skills_base_skills_path(void);

    /* Save overrides to default path and refresh validation. */
    void panel_skills_save_overrides_and_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* PANEL_SKILLS_SHARED_H */
