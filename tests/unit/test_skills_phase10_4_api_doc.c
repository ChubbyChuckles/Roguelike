#include "../../src/core/skills/skills_api_doc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char buf[4096];
    int n = rogue_skills_generate_api_doc(buf, (int) sizeof buf);
    assert(n > 0);
    assert(strstr(buf, "SKILLS DOC (Phase 10.4)") != NULL);
    assert(strstr(buf, "SKILL_SHEET_COLUMNS") != NULL);
    assert(strstr(buf, "COEFFS_JSON_FIELDS") != NULL);
    assert(strstr(buf, "VALIDATION_TOOLING") != NULL);
    /* Ordering sanity: columns section should appear before coeffs section */
    const char* p_cols = strstr(buf, "SKILL_SHEET_COLUMNS");
    const char* p_coeffs = strstr(buf, "COEFFS_JSON_FIELDS");
    assert(p_cols && p_coeffs && p_cols < p_coeffs);
    /* Small buffer failure path */
    char tiny[8];
    int r = rogue_skills_generate_api_doc(tiny, (int) sizeof tiny);
    assert(r == -1);
    printf("PH10.4 skills api doc OK (%d bytes)\n", n);
    return 0;
}
