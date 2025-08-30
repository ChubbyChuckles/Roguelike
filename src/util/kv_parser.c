/**
 * @file kv_parser.c
 * @brief Unified key-value file parser implementation.
 * @details This module provides parsing functionality for simple key=value configuration
 * files, supporting comments, whitespace trimming, and error reporting.
 */

/* kv_parser.c - Phase M3.1 unified key/value parser implementation */
#include "kv_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Loads and parses a key-value file from disk.
 * @param path Path to the file to load.
 * @param kv Pointer to the KV file structure to populate.
 * @return 1 on success, 0 on failure.
 * @details Reads the entire file into memory and prepares it for parsing.
 * Uses platform-specific file operations for MSVC vs POSIX compatibility.
 */
int rogue_kv_load_file(const char* path, RogueKVFile* kv)
{
    if (!path || !kv)
        return 0;
    memset(kv, 0, sizeof *kv);
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0)
        return 0;
#else
    f = fopen(path, "rb");
    if (!f)
        return 0;
#endif
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return 0;
    }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        return 0;
    }
    size_t rd = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    buf[rd] = '\0';
    kv->data = buf;
    kv->length = (int) rd;
    return 1;
}

/**
 * @brief Frees resources associated with a key-value file.
 * @param kv Pointer to the KV file structure to free.
 * @details Releases the memory allocated for the file data.
 */
void rogue_kv_free(RogueKVFile* kv)
{
    if (kv && kv->data)
    {
        free((void*) kv->data);
        kv->data = NULL;
        kv->length = 0;
    }
}

/**
 * @brief Checks if a character is whitespace.
 * @param c Character to check.
 * @return 1 if whitespace, 0 otherwise.
 * @details Considers space, tab, carriage return, and newline as whitespace.
 */
static int is_space(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

/**
 * @brief Parses the next key-value entry from the file.
 * @param kv Pointer to the loaded KV file.
 * @param cursor Pointer to the current parsing position (updated on return).
 * @param out_entry Pointer to store the parsed entry (can be NULL).
 * @param out_error Pointer to store error information (can be NULL).
 * @return 1 if an entry was parsed, 0 if end of file or error.
 * @details Parses one key=value line at a time, handling comments, whitespace,
 * and inline comments. Updates cursor position for subsequent calls.
 */
int rogue_kv_next(const RogueKVFile* kv, int* cursor, RogueKVEntry* out_entry,
                  RogueKVError* out_error)
{
    if (!kv || !cursor)
        return 0;
    const char* s = kv->data;
    int i = *cursor;
    int line = 1; /* compute line from start to i (cheap for small files) */
    for (int j = 0; j < i && j < kv->length; j++)
        if (s[j] == '\n')
            line++;
    while (i < kv->length)
    {
        int start_line = line;
        const char* line_start = &s[i];
        /* find end of line */
        int j = i;
        while (j < kv->length && s[j] != '\n')
            j++;
        int line_len = j - i;
        const char* line_end = &s[j];
        (void) line_end;
        /* trim leading */
        int k = 0;
        while (k < line_len && is_space(line_start[k]) && line_start[k] != '\n')
            k++;
        if (k == line_len || line_start[k] == '#' || line_start[k] == ';')
        { /* blank/comment */
            i = (s[j] == '\n') ? j + 1 : j;
            if (j < kv->length)
                line++;
            continue;
        }
        /* find '=' */
        int eq = -1;
        for (int p = k; p < line_len; p++)
        {
            if (line_start[p] == '=')
            {
                eq = p;
                break;
            }
        }
        if (eq < 0)
        {
            if (out_error)
            {
                out_error->line = start_line;
                out_error->message = "Missing '=' delimiter";
            }
            *cursor = (s[j] == '\n') ? j + 1 : j;
            return 0;
        }
        int key_end = eq - 1;
        while (key_end >= k && (line_start[key_end] == ' ' || line_start[key_end] == '\t'))
            key_end--;
        int val_start = eq + 1;
        while (val_start < line_len &&
               (line_start[val_start] == ' ' || line_start[val_start] == '\t'))
            val_start++;
        int val_end = line_len - 1;
        while (val_end >= val_start && (line_start[val_end] == ' ' || line_start[val_end] == '\t'))
            val_end--;
        /* allow inline comment starting with # or ; */
        for (int p = val_start; p <= val_end; p++)
        {
            if (line_start[p] == '#' || line_start[p] == ';')
            {
                val_end = p - 1;
                break;
            }
        }
        while (val_end >= val_start && (line_start[val_end] == ' ' || line_start[val_end] == '\t'))
            val_end--;
        if (key_end < k)
        {
            if (out_error)
            {
                out_error->line = start_line;
                out_error->message = "Empty key";
            }
            *cursor = (s[j] == '\n') ? j + 1 : j;
            return 0;
        }
        static char key_buf[128];
        static char val_buf[256];
        int key_len = key_end - k + 1;
        if (key_len > 127)
            key_len = 127;
        memcpy(key_buf, &line_start[k], key_len);
        key_buf[key_len] = '\0';
        int value_len = (val_end >= val_start) ? (val_end - val_start + 1) : 0;
        if (value_len > 255)
            value_len = 255;
        memcpy(val_buf, &line_start[val_start], value_len);
        val_buf[value_len] = '\0';
        if (out_entry)
        {
            out_entry->key = key_buf;
            out_entry->value = val_buf;
            out_entry->line = start_line;
        }
        *cursor = (s[j] == '\n') ? j + 1 : j;
        return 1;
    }
    return 0;
}
