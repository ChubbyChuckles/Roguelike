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
    struct RogueDialogueEffect* effects; /* Phase 3: optional small list (<=4) */
    unsigned char effect_count;          /* number of effect entries */
} RogueDialogueLine;

/* Phase 2 token flag bits */
#define ROGUE_DIALOGUE_LINE_HAS_TOKENS 0x1u

typedef struct RogueDialogueScript {
    int id;                      /* caller supplied id */
    int line_count;              /* number of populated lines */
    RogueDialogueLine* lines;    /* contiguous array (owned) */
    void* _blob;                 /* backing allocation for strings + lines (single free) */
    int _blob_size;              /* size in bytes of backing allocation */
    unsigned long long _executed_mask; /* Phase 3: bit per line (first 64 lines) */
} RogueDialogueScript;

/* Phase 3 effect kinds */
typedef enum RogueDialogueEffectKind {
    ROGUE_DIALOGUE_EFFECT_SET_FLAG = 1,
    ROGUE_DIALOGUE_EFFECT_GIVE_ITEM = 2
} RogueDialogueEffectKind;

typedef struct RogueDialogueEffect {
    unsigned char kind; /* RogueDialogueEffectKind */
    unsigned short a;   /* item id or unused */
    unsigned short b;   /* qty or unused */
    char name[24];      /* flag name (for SET_FLAG) */
} RogueDialogueEffect;

/* Phase 1 Runtime Playback State */
typedef struct RogueDialoguePlayback {
    int active;              /* 1 if a dialogue is currently open */
    int script_id;           /* active script id */
    int line_index;          /* current line index */
    float reveal_ms;         /* accumulated ms for typewriter (Phase 6 future) */
    int suspended_inputs;    /* 1 if combat inputs suspended while active */
} RogueDialoguePlayback;

/* Begin playback of a registered script (returns 0 on success, <0 on error/not found). */
int rogue_dialogue_start(int script_id);
/* Advance to next line or close if at end; returns 1 if advanced, 0 if closed, <0 on error. */
int rogue_dialogue_advance(void);
/* Get current playback (NULL if inactive). */
const RogueDialoguePlayback* rogue_dialogue_playback(void);
/* Internal update hook (dt_ms) - currently trivial placeholder for future typewriter. */
void rogue_dialogue_update(double dt_ms);
/* Forward declare UI context to avoid heavy include. */
struct RogueUIContext;
/* Render UI panel (no-op if inactive). Provided a UI context; returns 1 if drew. */
int rogue_dialogue_render_ui(struct RogueUIContext* ui);
/* Append entire transcript so far to log (used automatically on advance). */
void rogue_dialogue_log_current_line(void);

/* Phase 2 Token Context (simple global setters used by tests & game) */
void rogue_dialogue_set_player_name(const char* name);
void rogue_dialogue_set_run_seed(unsigned int seed);
/* Retrieve current line expanded into buffer (tokens replaced). Returns length written, or <0 on error/inactive. */
int rogue_dialogue_current_text(char* buffer, size_t cap);

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

/* Phase 3 Effects Introspection (test helpers) */
int rogue_dialogue_effect_flag_count(void);
const char* rogue_dialogue_effect_flag(int index);
int rogue_dialogue_effect_item_count(void);
int rogue_dialogue_effect_item(int index, int* out_item_id, int* out_qty);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_DIALOGUE_H */
