/* loot_api_doc.h - Itemsystem Phase 23.5 (API doc generation)
 *
 * Provides a lightweight runtime generator that emits a stable, human readable
 * summary of selected public loot module APIs (subset – focused on frequently used
 * integration points). This avoids adding heavy parsing dependencies while
 * still giving contributors an always up-to-date reference they can diff in CI.
 *
 * The generator purposefully keeps the list curated (manually updated when new
 * APIs land) so that descriptions stay meaningful instead of raw signatures.
 */
#ifndef ROGUE_LOOT_API_DOC_H
#define ROGUE_LOOT_API_DOC_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Generate API doc into caller buffer. Returns number of bytes written (excluding
     * terminating NUL) or -1 if cap too small (<128) or arguments invalid. Always
     * terminates NUL if cap>0. Ordering is stable for regression tests. */
    int rogue_loot_generate_api_doc(char* buf, int cap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_LOOT_API_DOC_H */
