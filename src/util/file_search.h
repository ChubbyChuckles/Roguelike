/* Recursive project-wide filename search utility for fallback asset discovery. */
#ifndef ROGUE_FILE_SEARCH_H
#define ROGUE_FILE_SEARCH_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Search from candidate roots (., .., ../.., ../../..) recursively for a file whose basename
       matches target_name exactly (case-insensitive on Windows, case-sensitive elsewhere).
       On success writes full path into 'out' (null-terminated) and returns 1; otherwise 0. */
    int rogue_file_search_project(const char* target_name, char* out, int out_sz);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_FILE_SEARCH_H */
