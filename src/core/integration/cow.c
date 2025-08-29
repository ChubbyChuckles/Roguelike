/**
 * @file cow.c
 * @brief Implementation of Copy-On-Write (COW) buffer system for efficient memory sharing and
 * incremental updates.
 *
 * This module provides a chunked, page-based copy-on-write buffer implementation that supports:
 * - Efficient cloning with shared pages (O(1) time)
 * - Automatic page duplication on first write (copy-on-write)
 * - Optional deduplication of identical page contents
 * - Serialization/deserialization to/from contiguous memory
 * - Comprehensive statistics and debugging utilities
 *
 * The system uses reference counting for page sharing and supports configurable page sizes.
 * Pages are allocated with a reference-counted header for automatic memory management.
 *
 * @author Rogue Game Development Team
 * @date 2025
 * @version 1.0
 *
 * @note This implementation is part of Phase 4.5 of the roguelike game development.
 * @see cow.h for public API declarations
 */

#include "cow.h"
#include "ref_count.h" // reuse intrusive rc for shared pages
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef COW_DEFAULT_PAGE
#define COW_DEFAULT_PAGE 4096
#endif

// Page struct managed via ref_count for sharing.
typedef struct RogueCowPage
{
    size_t size; // bytes used (<= page_size for last page)
    // followed by raw bytes
} RogueCowPage;

struct RogueCowBuffer
{
    size_t page_size;
    size_t length; // logical size
    size_t page_count;
    RogueCowPage** pages; // array of pointers to page payload (after rc header)
};

/**
 * @brief Global statistics structure for tracking COW buffer operations.
 *
 * This structure maintains counters for various operations performed by the COW system,
 * useful for performance monitoring, debugging, and optimization analysis.
 */
static struct
{
    uint64_t buffers_created;          /**< Total number of COW buffers created */
    uint64_t pages_created;            /**< Total number of pages allocated */
    uint64_t cow_triggers;             /**< Number of copy-on-write triggers (page duplications) */
    uint64_t page_copies;              /**< Total number of page copy operations performed */
    uint64_t dedup_hits;               /**< Number of successful page deduplications */
    uint64_t serialize_linearizations; /**< Number of buffer serialization operations */
} g_stats;

/**
 * @brief Allocates a new COW page with reference counting.
 *
 * Creates a new page structure with the specified size and optionally copies data
 * from the source buffer. The page is allocated with a reference count header
 * for automatic memory management.
 *
 * @param page_size The size of the page in bytes (capacity for data)
 * @param src Pointer to source data to copy (NULL for zero-initialized page)
 * @param bytes Number of bytes to copy from source (0 for empty page)
 * @return Pointer to the allocated page, or NULL on allocation failure
 *
 * @note The returned pointer points to the page structure, with data following immediately after
 */
static RogueCowPage* alloc_page(size_t page_size, const void* src, size_t bytes)
{
    RogueCowPage* p =
        (RogueCowPage*) rogue_rc_alloc(sizeof(RogueCowPage) + page_size, 0xC0A401u, NULL);
    if (!p)
        return NULL;
    p->size = bytes;
    if (src && bytes)
        memcpy((unsigned char*) (p + 1), src, bytes);
    return p;
}

/**
 * @brief Creates a new COW buffer from raw byte data.
 *
 * Allocates a new COW buffer and populates it with the provided data, splitting
 * the data into pages of the specified chunk size. If chunk_size is 0, uses
 * the default page size (4096 bytes).
 *
 * @param data Pointer to the source data to copy (NULL creates zero-initialized buffer)
 * @param len Length of the source data in bytes
 * @param chunk_size Size of each page in bytes (0 for default)
 * @return Pointer to the new COW buffer, or NULL on allocation failure
 *
 * @note The buffer supports empty data (len=0) and will create at least one page
 * @see COW_DEFAULT_PAGE for the default page size
 */
RogueCowBuffer* rogue_cow_create_from_bytes(const void* data, size_t len, size_t chunk_size)
{
    if (chunk_size == 0)
        chunk_size = COW_DEFAULT_PAGE;
    RogueCowBuffer* b = (RogueCowBuffer*) calloc(1, sizeof(RogueCowBuffer));
    if (!b)
        return NULL;
    b->page_size = chunk_size;
    b->length = len;
    b->page_count = (len + chunk_size - 1) / chunk_size;
    if (b->page_count == 0)
        b->page_count = 1; // support empty buffer with one zero page
    b->pages = (RogueCowPage**) calloc(b->page_count, sizeof(RogueCowPage*));
    if (!b->pages)
    {
        free(b);
        return NULL;
    }
    for (size_t i = 0; i < b->page_count; i++)
    {
        size_t off = i * chunk_size;
        size_t remain = (off < len) ? (len - off) : 0;
        size_t to_copy = remain > chunk_size ? chunk_size : remain;
        RogueCowPage* p =
            alloc_page(chunk_size, data ? (const unsigned char*) data + off : NULL, to_copy);
        if (!p)
        {
            rogue_cow_destroy(b);
            return NULL;
        }
        b->pages[i] = p;
        g_stats.pages_created++;
    }
    g_stats.buffers_created++;
    return b;
}

/**
 * @brief Creates a shallow clone of a COW buffer.
 *
 * Creates a new buffer structure that shares all pages with the source buffer.
 * This operation is O(1) and very efficient as no data is copied initially.
 * Reference counts are incremented for all shared pages.
 *
 * @param src Pointer to the source buffer to clone
 * @return Pointer to the new cloned buffer, or NULL on allocation failure or if src is NULL
 *
 * @note The cloned buffer will trigger copy-on-write when modified
 * @see rogue_cow_write() for how modifications trigger page duplication
 */
RogueCowBuffer* rogue_cow_clone(RogueCowBuffer* src)
{
    if (!src)
        return NULL;
    RogueCowBuffer* b = (RogueCowBuffer*) calloc(1, sizeof(RogueCowBuffer));
    if (!b)
        return NULL;
    b->page_size = src->page_size;
    b->length = src->length;
    b->page_count = src->page_count;
    b->pages = (RogueCowPage**) calloc(b->page_count, sizeof(RogueCowPage*));
    if (!b->pages)
    {
        free(b);
        return NULL;
    }
    for (size_t i = 0; i < b->page_count; i++)
    {
        b->pages[i] = src->pages[i];
        if (b->pages[i])
            rogue_rc_retain(b->pages[i]);
    }
    g_stats.buffers_created++;
    return b;
}

/**
 * @brief Destroys a COW buffer and releases all associated resources.
 *
 * Decrements reference counts for all pages in the buffer. Pages are automatically
 * freed when their reference count reaches zero. Also frees the buffer structure
 * and page array.
 *
 * @param buf Pointer to the buffer to destroy (NULL is safe - no operation)
 *
 * @note This function is safe to call multiple times on the same buffer
 */
void rogue_cow_destroy(RogueCowBuffer* buf)
{
    if (!buf)
        return;
    for (size_t i = 0; i < buf->page_count; i++)
        if (buf->pages[i])
            rogue_rc_release(buf->pages[i]);
    free(buf->pages);
    free(buf);
}

/**
 * @brief Returns the logical size of a COW buffer in bytes.
 *
 * @param buf Pointer to the buffer (NULL returns 0)
 * @return The size of the buffer in bytes, or 0 if buf is NULL
 */
size_t rogue_cow_size(const RogueCowBuffer* buf) { return buf ? buf->length : 0; }

/**
 * @brief Reads data from a COW buffer into a user-provided buffer.
 *
 * Copies len bytes starting at the specified offset from the COW buffer
 * into the output buffer. Handles reading across page boundaries automatically.
 *
 * @param buf Pointer to the source COW buffer
 * @param offset Byte offset to start reading from
 * @param out Pointer to the output buffer to write data into
 * @param len Number of bytes to read
 * @return 0 on success, -1 if buf or out is NULL, or if offset+len exceeds buffer size
 *
 * @note Reading from areas beyond the actual data in a page returns zeros
 */
int rogue_cow_read(const RogueCowBuffer* buf, size_t offset, void* out, size_t len)
{
    if (!buf || !out)
        return -1;
    if (offset + len > buf->length)
        return -1;
    unsigned char* dst = (unsigned char*) out;
    size_t page_sz = buf->page_size;
    while (len)
    {
        size_t page_index = offset / page_sz;
        size_t in_page_off = offset % page_sz;
        size_t can = page_sz - in_page_off;
        if (can > len)
            can = len;
        RogueCowPage* p = buf->pages[page_index];
        size_t avail = p->size - in_page_off;
        if (in_page_off >= p->size)
            memset(dst, 0, can);
        else
            memcpy(dst, (unsigned char*) (p + 1) + in_page_off, can > avail ? avail : can);
        if (can > avail && can > 0)
            memset(dst + avail, 0, can - avail);
        offset += can;
        dst += can;
        len -= can;
    }
    return 0;
}

/**
 * @brief Ensures a page is uniquely owned before modification (copy-on-write).
 *
 * Checks if the specified page is shared (reference count > 1). If so, creates
 * a private copy of the page and updates the buffer to use the new copy.
 * This implements the core copy-on-write mechanism.
 *
 * @param buf Pointer to the buffer containing the page
 * @param page_index Index of the page to ensure uniqueness for
 * @return 0 on success, -1 on allocation failure or invalid page
 *
 * @note This function increments the cow_triggers and page_copies statistics
 */
static int ensure_unique_page(RogueCowBuffer* buf, size_t page_index)
{
    RogueCowPage* p = buf->pages[page_index];
    if (!p)
        return -1;
    if (rogue_rc_get_strong(p) > 1)
    {
        g_stats.cow_triggers++;
        RogueCowPage* n = alloc_page(buf->page_size, p + 1, p->size);
        if (!n)
            return -1;
        buf->pages[page_index] = n;
        rogue_rc_release(p);
        g_stats.page_copies++;
    }
    return 0;
}

/**
 * @brief Writes data to a COW buffer, triggering copy-on-write as needed.
 *
 * Copies len bytes from the source buffer into the COW buffer starting at the
 * specified offset. Automatically handles page boundaries and triggers
 * copy-on-write for any shared pages that need modification.
 *
 * @param buf Pointer to the destination COW buffer
 * @param offset Byte offset to start writing at
 * @param src Pointer to the source data to copy
 * @param len Number of bytes to write
 * @return 0 on success, -1 if buf or src is NULL, offset+len exceeds buffer size, or allocation
 * fails
 *
 * @note Writing to shared pages will create private copies automatically
 * @note The buffer size cannot be extended - writes must stay within existing bounds
 */
int rogue_cow_write(RogueCowBuffer* buf, size_t offset, const void* src, size_t len)
{
    if (!buf || !src)
        return -1;
    if (offset + len > buf->length)
        return -1; // no implicit growth for now
    const unsigned char* s = (const unsigned char*) src;
    size_t page_sz = buf->page_size;
    while (len)
    {
        size_t page_index = offset / page_sz;
        size_t in_page_off = offset % page_sz;
        size_t can = page_sz - in_page_off;
        if (can > len)
            can = len;
        if (ensure_unique_page(buf, page_index) != 0)
            return -1;
        RogueCowPage* p = buf->pages[page_index];
        size_t end = in_page_off + can;
        if (end > p->size)
            p->size = end; // extend last page logical size (not buffer length)
        memcpy((unsigned char*) (p + 1) + in_page_off, s, can);
        offset += can;
        s += can;
        len -= can;
    }
    return 0;
}

/**
 * @brief Computes a 64-bit FNV-1a hash of page data for deduplication.
 *
 * Uses the FNV-1a hashing algorithm to generate a hash value from the page data.
 * This hash is used for efficient comparison during page deduplication.
 *
 * @param data Pointer to the data to hash
 * @param sz Size of the data in bytes
 * @return 64-bit hash value of the data
 *
 * @note FNV-1a is chosen for its good distribution and speed properties
 */
static uint64_t hash_page(const unsigned char* data, size_t sz)
{
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < sz; i++)
    {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/**
 * @brief Entry structure for the deduplication hash table.
 *
 * Used internally by the deduplication process to track page hashes and
 * their corresponding page pointers in a hash table.
 */
typedef struct DedupEntry
{
    uint64_t hash;      /**< Hash value of the page data */
    RogueCowPage* page; /**< Pointer to the page (NULL indicates empty slot) */
} DedupEntry;

/**
 * @brief Performs deduplication of identical pages within a buffer.
 *
 * Scans all pages in the buffer and identifies pages with identical content.
 * Replaces duplicate pages with references to a single shared page, reducing
 * memory usage. Uses an open-addressing hash table for efficient comparison.
 *
 * @param buf Pointer to the buffer to deduplicate
 *
 * @note This operation is O(n) where n is the number of pages
 * @note Not guaranteed to find all duplicates if the hash table becomes full
 * @note Updates the dedup_hits statistic for each successful deduplication
 */
void rogue_cow_dedup(RogueCowBuffer* buf)
{
    if (!buf || buf->page_count < 2)
        return;
    size_t cap = 1;
    while (cap < buf->page_count * 2)
        cap <<= 1;
    if (cap < 8)
        cap = 8;
    DedupEntry* table = (DedupEntry*) calloc(cap, sizeof(DedupEntry));
    if (!table)
        return;
    for (size_t i = 0; i < buf->page_count; i++)
    {
        RogueCowPage* p = buf->pages[i];
        if (!p)
            continue;
        size_t eff = p->size;
        if (eff == 0)
            eff = 1; // treat empty distinct
        uint64_t h = hash_page((unsigned char*) (p + 1), eff);
        size_t mask = cap - 1;
        size_t pos = h & mask;
        size_t first_free = (size_t) -1;
        while (1)
        {
            if (table[pos].page == NULL)
            {
                if (first_free == (size_t) -1)
                    first_free = pos;
                break;
            }
            if (table[pos].hash == h)
            {
                // compare bytes
                RogueCowPage* existing = table[pos].page;
                if (existing->size == p->size && memcmp(existing + 1, p + 1, p->size) == 0)
                {
                    // replace with existing
                    rogue_rc_retain(existing);
                    rogue_rc_release(p);
                    buf->pages[i] = existing;
                    g_stats.dedup_hits++;
                    goto next_page;
                }
            }
            pos = (pos + 1) & mask;
        }
        // insert
        table[first_free].hash = h;
        table[first_free].page = p;
    next_page:;
    }
    free(table);
}

/**
 * @brief Serializes a COW buffer into contiguous memory.
 *
 * Linearizes the chunked buffer into a single contiguous block of memory.
 * If out is NULL, returns the required buffer size without copying data.
 * If max is less than required, copies what fits and still returns the required size.
 *
 * @param buf Pointer to the buffer to serialize
 * @param out Pointer to output buffer (NULL to query required size)
 * @param max Maximum number of bytes to write to output buffer
 * @return The required buffer size in bytes (may be larger than max if truncated)
 *
 * @note This operation performs a full linearization and updates serialize_linearizations statistic
 * @note Zero-pads any unfilled portions of the last page
 */
size_t rogue_cow_serialize(const RogueCowBuffer* buf, void* out, size_t max)
{
    if (!buf)
        return 0;
    g_stats.serialize_linearizations++;
    size_t needed = buf->length;
    if (!out)
        return needed;
    if (max == 0)
        return needed;
    unsigned char* dst = (unsigned char*) out;
    size_t page_sz = buf->page_size;
    size_t remaining = buf->length;
    size_t offset = 0;
    for (size_t i = 0; i < buf->page_count && remaining; i++)
    {
        RogueCowPage* p = buf->pages[i];
        size_t copy = remaining > page_sz ? page_sz : remaining;
        if (copy > p->size)
            copy = p->size;
        size_t can = copy;
        if (offset + can > max)
            can = max - offset;
        if (can > 0)
            memcpy(dst + offset, p + 1, can);
        if (can < copy && offset + can < max)
        {
            size_t pad = copy - can;
            if (offset + can + pad > max)
                pad = max - (offset + can);
            memset(dst + offset + can, 0, pad);
            can += pad;
        }
        offset += can;
        remaining -= copy;
        if (offset >= max)
            break;
    }
    return needed;
}

/**
 * @brief Deserializes raw bytes into a new COW buffer.
 *
 * Creates a new COW buffer from contiguous serialized data. This is essentially
 * a wrapper around rogue_cow_create_from_bytes for consistency with the
 * serialization interface.
 *
 * @param data Pointer to the serialized data
 * @param len Length of the data in bytes
 * @param chunk_size Page size for the new buffer (0 for default)
 * @return Pointer to the new COW buffer, or NULL on allocation failure
 *
 * @see rogue_cow_create_from_bytes for implementation details
 * @see rogue_cow_serialize for the complementary serialization function
 */
RogueCowBuffer* rogue_cow_deserialize(const void* data, size_t len, size_t chunk_size)
{
    return rogue_cow_create_from_bytes(data, len, chunk_size);
}

/**
 * @brief Retrieves a snapshot of the global COW statistics.
 *
 * Copies the current values of all global statistics into the provided
 * structure for monitoring and debugging purposes.
 *
 * @param out Pointer to the output statistics structure (must not be NULL)
 *
 * @note Statistics are global across all COW buffers in the process
 * @see RogueCowStats for the structure definition
 */
void rogue_cow_get_stats(RogueCowStats* out)
{
    if (!out)
        return;
    out->buffers_created = g_stats.buffers_created;
    out->pages_created = g_stats.pages_created;
    out->cow_triggers = g_stats.cow_triggers;
    out->page_copies = g_stats.page_copies;
    out->dedup_hits = g_stats.dedup_hits;
    out->serialize_linearizations = g_stats.serialize_linearizations;
}

/**
 * @brief Dumps debug information about a COW buffer to a file stream.
 *
 * Outputs detailed information about the buffer structure, including size,
 * page count, page size, and per-page details like size and reference count.
 * Useful for debugging and monitoring buffer state.
 *
 * @param buf Pointer to the buffer to dump (NULL prints "null buffer")
 * @param fptr Pointer to FILE stream (cast to void* for C compatibility), NULL uses stdout
 *
 * @note Output format is human-readable for debugging purposes
 */
void rogue_cow_dump(RogueCowBuffer* buf, void* fptr)
{
    FILE* f = fptr ? (FILE*) fptr : stdout;
    if (!buf)
    {
        fprintf(f, "[cow] null buffer\n");
        return;
    }
    fprintf(f, "[cow] size=%zu pages=%zu page_size=%zu\n", buf->length, buf->page_count,
            buf->page_size);
    for (size_t i = 0; i < buf->page_count; i++)
    {
        RogueCowPage* p = buf->pages[i];
        if (!p)
        {
            fprintf(f, " page %zu: null\n", i);
            continue;
        }
        fprintf(f, " page %zu: size=%zu ref=%u\n", i, p->size, rogue_rc_get_strong(p));
    }
}

/**
 * @brief Returns the reference count of a specific page (internal/test helper).
 *
 * Retrieves the strong reference count for the page at the specified index.
 * This is primarily intended for testing and debugging purposes.
 *
 * @param buf Pointer to the buffer containing the page
 * @param page_index Index of the page to query
 * @return Strong reference count of the page, or 0xFFFFFFFFu if buf is NULL or page_index is out of
 * bounds
 *
 * @note This function is marked as internal/test helper in the header
 * @see rogue_rc_get_strong for the underlying reference counting implementation
 */
uint32_t rogue_cow_page_refcount(RogueCowBuffer* buf, size_t page_index)
{
    if (!buf || page_index >= buf->page_count)
        return 0xFFFFFFFFu;
    RogueCowPage* p = buf->pages[page_index];
    return p ? rogue_rc_get_strong(p) : 0;
}
