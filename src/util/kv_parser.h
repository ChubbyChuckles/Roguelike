/* kv_parser.h - Unified key/value config parser (Phase M3.1)
 * Supports simple files of lines: key = value  (value up to end-of-line)
 * Comments start with '#' or ';'. Blank lines ignored.
 * Provides iteration over entries with error reporting for malformed lines.
 */
#ifndef ROGUE_UTIL_KV_PARSER_H
#define ROGUE_UTIL_KV_PARSER_H

typedef struct RogueKVEntry
{
    const char* key;
    const char* value;
    int line;
} RogueKVEntry;
typedef struct RogueKVError
{
    int line;
    const char* message;
} RogueKVError;

typedef struct RogueKVFile
{
    const char* data; /* owning buffer (null-terminated) */
    int length;
} RogueKVFile;

/* Load entire file into memory (returns 0 on failure). On success, kv->data points to malloc'd
 * buffer. */
int rogue_kv_load_file(const char* path, RogueKVFile* kv);
void rogue_kv_free(RogueKVFile* kv);

/* Iterate entries: pass cursor=0 to start; returns 1 if entry filled, 0 when done. */
int rogue_kv_next(const RogueKVFile* kv, int* cursor, RogueKVEntry* out_entry,
                  RogueKVError* out_error);

#endif
