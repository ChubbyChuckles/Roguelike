/**
 * @file json_io.c
 * @brief JSON I/O Utilities for File Operations.
 *
 * This module provides platform-independent utilities for reading JSON data from files, performing
 * atomic writes to avoid corruption during updates, and retrieving file modification times in
 * milliseconds since the Unix epoch. Supports error reporting via optional buffers. Handles
 * differences between Windows and Unix-like systems for atomic operations and file stats.
 * Includes a helper for setting error messages.
 *
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 */

#define _CRT_SECURE_NO_WARNINGS
#include "json_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/**
 * @brief Sets an error message in the provided buffer.
 *
 * Safely copies the error message into the buffer, truncating if necessary, or clears it if no
 * message. Used across all functions in this module for consistent error handling.
 *
 * @param err Buffer to store the error message (may be NULL).
 * @param cap Capacity of the error buffer (including null terminator).
 * @param msg Error message to set (may be NULL to clear).
 */
static void set_err(char* err, int cap, const char* msg)
{
    if (!err || cap <= 0)
        return;
    if (!msg)
    {
        err[0] = '\0';
        return;
    }
    snprintf(err, (size_t) cap, "%s", msg);
}

/**
 * @brief Reads the entire contents of a file into an allocated buffer.
 *
 * Opens the file in binary mode, determines size using fseek/ftell, allocates memory, reads the
 * content, null-terminates the buffer for string use. Caller must free the data. Supports error
 * reporting for various failure modes like file not found or read errors.
 *
 * @param path Path to the file to read (required, non-null).
 * @param[out] out_data Pointer to receive the allocated buffer (required, set to NULL on failure).
 * @param[out] out_len Pointer to receive the length of read data (required, set to 0 on failure).
 * @param err Buffer for error message (optional).
 * @param err_cap Capacity of error buffer.
 * @return 0 on success, 1 for invalid args, 2 for fopen failure, 3 for fseek end, 4 for ftell,
 *         5 for fseek set, 6 for out-of-memory, 7 for short fread.
 */
int json_io_read_file(const char* path, char** out_data, size_t* out_len, char* err, int err_cap)
{
    if (!path || !out_data || !out_len)
    {
        set_err(err, err_cap, "invalid args");
        return 1;
    }
    *out_data = NULL;
    *out_len = 0;
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        set_err(err, err_cap, "fopen failed");
        return 2;
    }
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        set_err(err, err_cap, "fseek end failed");
        return 3;
    }
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        set_err(err, err_cap, "ftell failed");
        return 4;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        set_err(err, err_cap, "fseek set failed");
        return 5;
    }
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        set_err(err, err_cap, "oom");
        return 6;
    }
    size_t n = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    if (n != (size_t) sz)
    {
        free(buf);
        set_err(err, err_cap, "fread short");
        return 7;
    }
    buf[n] = '\0';
    *out_data = buf;
    *out_len = n;
    return 0;
}

/**
 * @brief Writes data to a file atomically to prevent partial writes or corruption.
 *
 * Creates a temporary file with a unique name (using length XOR for suffix), writes the data to it,
 * then renames/moves it to the target path. On Windows, uses MoveFileExW with UTF-8 conversion and
 * replace existing; on Unix, uses rename(). Cleans up temp on failure. Ensures atomicity for JSON
 * updates.
 *
 * @param path Target file path (required, non-null).
 * @param data Data to write (required, non-null).
 * @param len Length of data to write.
 * @param err Buffer for error message (optional).
 * @param err_cap Capacity of error buffer.
 * @return 0 on success, 1 for invalid args, 2 for fopen temp failure, 3 for short fwrite,
 *         4 for fclose temp, 5 for rename/move failure.
 */
int json_io_write_atomic(const char* path, const char* data, size_t len, char* err, int err_cap)
{
    if (!path || !data)
    {
        set_err(err, err_cap, "invalid args");
        return 1;
    }

    /* Build temp path: path.tmpXXXX */
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp%lu", path, (unsigned long) (len ^ 0x9E3779B1u));

    FILE* f = fopen(tmp_path, "wb");
    if (!f)
    {
        set_err(err, err_cap, "fopen tmp failed");
        return 2;
    }
    size_t n = fwrite(data, 1, len, f);
    if (n != len)
    {
        fclose(f);
        set_err(err, err_cap, "fwrite short");
        return 3;
    }
    if (fclose(f) != 0)
    {
        set_err(err, err_cap, "fclose tmp failed");
        return 4;
    }

#ifdef _WIN32
    /* Replace target atomically: MoveFileEx with MOVEFILE_REPLACE_EXISTING */
    wchar_t wtmp[1024];
    wchar_t wdst[1024];
    MultiByteToWideChar(CP_UTF8, 0, tmp_path, -1, wtmp, 1024);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wdst, 1024);
    if (!MoveFileExW(wtmp, wdst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
    {
        /* Cleanup temp on failure */
        DeleteFileW(wtmp);
        set_err(err, err_cap, "MoveFileEx failed");
        return 5;
    }
#else
    if (rename(tmp_path, path) != 0)
    {
        set_err(err, err_cap, "rename failed");
        return 5;
    }
#endif
    return 0;
}

/**
 * @brief Retrieves the last modification time of a file in milliseconds since Unix epoch.
 *
 * Platform-specific: On Windows, uses GetFileAttributesExW with UTF-8 path conversion, converts
 * FILETIME to Unix time. On Unix, uses stat() and multiplies seconds by 1000. Returns 0 for files
 * dated before epoch. Supports error reporting.
 *
 * @param path Path to the file (required, non-null).
 * @param[out] out_ms Pointer to receive the modification time in ms (required).
 * @param err Buffer for error message (optional).
 * @param err_cap Capacity of error buffer.
 * @return 0 on success, 1 for invalid args, 2 for file attribute/stat failure.
 */
int json_io_get_mtime_ms(const char* path, uint64_t* out_ms, char* err, int err_cap)
{
    if (!path || !out_ms)
    {
        set_err(err, err_cap, "invalid args");
        return 1;
    }
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    wchar_t wpath[1024];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 1024);
    if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &fad))
    {
        set_err(err, err_cap, "GetFileAttributesEx failed");
        return 2;
    }
    /* Convert FILETIME (100-ns intervals since Jan 1, 1601) to ms since epoch */
    ULARGE_INTEGER uli;
    uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
    uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    uint64_t ft_100ns = uli.QuadPart;
    /* Difference between 1601-01-01 and 1970-01-01 in 100ns units */
    const uint64_t EPOCH_DIFF_100NS = 11644473600ULL * 10000000ULL;
    if (ft_100ns < EPOCH_DIFF_100NS)
    {
        *out_ms = 0;
        return 0;
    }
    uint64_t unix_100ns = ft_100ns - EPOCH_DIFF_100NS;
    *out_ms = unix_100ns / 10000ULL;
#else
    struct stat st;
    if (stat(path, &st) != 0)
    {
        set_err(err, err_cap, "stat failed");
        return 2;
    }
    *out_ms = (uint64_t) st.st_mtime * 1000ULL;
#endif
    return 0;
}
