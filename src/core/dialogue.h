/* dialogue.h - Dialogue System Phase 0 (data model + loader + registry)
 * Linear-only conversations. Future phases will extend tokens, effects, persistence.
 */
#ifndef ROGUE_CORE_DIALOGUE_H
#define ROGUE_CORE_DIALOGUE_H

#ifdef __cplusplus
extern "C" {
#endif

/* A single dialogue line (speaker + raw text). Effect & token fields reserved for later phases. */
typedef struct RogueDialogueLine {
    const char* speaker_id;      /* interned/owned by script allocation */
    const char* text;            /* raw text (may contain token syntax in later phases) */
    unsigned int effect_mask;    /* Phase 3 placeholder (always 0 in Phase 0) */
    unsigned int token_flags;    /* Phase 2 placeholder (always 0 in Phase 0) */
    unsigned char _reserved[8];  /* forward compatibility (branch targets, conditions) */
} RogueDialogueLine;

typedef struct RogueDialogueScript {
    int id;                      /* caller supplied id */
    int line_count;              /* number of populated lines */
    RogueDialogueLine* lines;    /* contiguous array (owned) */
    void* _blob;                 /* backing allocation for strings + lines (single free) */
    int _blob_size;              /* size in bytes of backing allocation */
} RogueDialogueScript;

/* Load a plaintext script from file path. Format per line: speaker_id|text
 * Empty lines or lines beginning with '#' are ignored. Whitespace around speaker_id is trimmed.
 * Returns 0 on success, <0 on error (duplicate id, parse, IO, capacity).
 */
int rogue_dialogue_load_script_from_file(int id, const char* path);

/* Register a script from an in-memory buffer (used by unit tests). Buffer need not be null-terminated. */
int rogue_dialogue_register_from_buffer(int id, const char* buffer, int length);

/* Retrieve a registered script (NULL if not found). */
const RogueDialogueScript* rogue_dialogue_get(int id);

/* Reset registry (frees all scripts). Intended for test harness teardown. */
void rogue_dialogue_reset(void);

/* Introspect current registered script count (Phase 0 test helper). */
int rogue_dialogue_script_count(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_DIALOGUE_H */
