/* skills_api_doc.h - Skillsystem Phase 10.4 (Auto-doc generator for skill sheets)
 *
 * Provides a lightweight, curated documentation generator for skill sheets and
 * related data-driven inputs (coefficients JSON). The output is a stable,
 * human-readable reference suitable for CI diffs and quick editor previews.
 */
#ifndef ROGUE_CORE_SKILLS_API_DOC_H
#define ROGUE_CORE_SKILLS_API_DOC_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Generate skills API/doc text into caller buffer. Returns number of bytes written (excluding
     * terminating NUL) or -1 if cap too small (<128) or arguments invalid. Always NUL-terminates
     * when cap>0. Ordering is stable for regression tests. */
    int rogue_skills_generate_api_doc(char* buf, int cap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_SKILLS_API_DOC_H */
