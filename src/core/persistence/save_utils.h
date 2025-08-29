#ifndef ROGUE_SAVE_UTILS_H
#define ROGUE_SAVE_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Compact varint (LEB128-style) helpers used inside section payloads */
int rogue_write_varuint(FILE* f, uint32_t v);   /* returns 0 on success */
int rogue_read_varuint(FILE* f, uint32_t* out); /* returns 0 on success */

/* CRC32 (polynomial 0xEDB88320) */
uint32_t rogue_crc32(const void* data, size_t len);

/* Minimal SHA256 context (public domain style implementation) */
typedef struct RogueSHA256Ctx
{
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    size_t buf_len;
} RogueSHA256Ctx;

void rogue_sha256_init(RogueSHA256Ctx* c);
void rogue_sha256_update(RogueSHA256Ctx* c, const void* data, size_t len);
void rogue_sha256_final(RogueSHA256Ctx* c, unsigned char out[32]);

/* Endianness helper */
int rogue_save_format_endianness_is_le(void);

#endif /* ROGUE_SAVE_UTILS_H */
