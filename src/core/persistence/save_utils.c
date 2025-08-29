#include "save_utils.h"
#include <string.h>

/* Unsigned LEB128 style varint (7 bits per byte) */
int rogue_write_varuint(FILE* f, uint32_t v)
{
    while (v >= 0x80)
    {
        unsigned char b = (unsigned char) ((v & 0x7Fu) | 0x80u);
        if (fwrite(&b, 1, 1, f) != 1)
            return -1;
        v >>= 7;
    }
    unsigned char b = (unsigned char) (v & 0x7Fu);
    if (fwrite(&b, 1, 1, f) != 1)
        return -1;
    return 0;
}

int rogue_read_varuint(FILE* f, uint32_t* out)
{
    uint32_t result = 0;
    int shift = 0;
    for (int i = 0; i < 5; i++)
    {
        int c = fgetc(f);
        if (c == EOF)
            return -1;
        unsigned char bv = (unsigned char) c;
        result |= (uint32_t) (bv & 0x7F) << shift;
        if (!(bv & 0x80))
        {
            if (out)
                *out = result;
            return 0;
        }
        shift += 7;
    }
    return -1;
}

/* CRC32 (polynomial 0xEDB88320) */
uint32_t rogue_crc32(const void* data, size_t len)
{
    static uint32_t table[256];
    static int have = 0;
    if (!have)
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : (c >> 1);
            table[i] = c;
        }
        have = 1;
    }
    uint32_t crc = 0xFFFFFFFFu;
    const unsigned char* p = (const unsigned char*) data;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static uint32_t rs_rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

void rogue_sha256_init(RogueSHA256Ctx* c)
{
    static const uint32_t iv[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    memcpy(c->h, iv, sizeof iv);
    c->len = 0;
    c->buf_len = 0;
}

static void rogue_sha256_block(RogueSHA256Ctx* c, const unsigned char* p)
{
    static const uint32_t K[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
        0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
        0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
        0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
        0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
        0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
        0xc67178f2u};
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
    {
        w[i] = (uint32_t) p[4 * i] << 24 | (uint32_t) p[4 * i + 1] << 16 |
               (uint32_t) p[4 * i + 2] << 8 | (uint32_t) p[4 * i + 3];
    }
    for (int i = 16; i < 64; i++)
    {
        uint32_t s0 = rs_rotr(w[i - 15], 7) ^ rs_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rs_rotr(w[i - 2], 17) ^ rs_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = c->h[0], b = c->h[1], d = c->h[3], e = c->h[4], f = c->h[5], g = c->h[6],
             h = c->h[7], cc = c->h[2];
    for (int i = 0; i < 64; i++)
    {
        uint32_t S1 = rs_rotr(e, 6) ^ rs_rotr(e, 11) ^ rs_rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rs_rotr(a, 2) ^ rs_rotr(a, 13) ^ rs_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t temp2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = cc;
        cc = b;
        b = a;
        a = temp1 + temp2;
    }
    c->h[0] += a;
    c->h[1] += b;
    c->h[2] += cc;
    c->h[3] += d;
    c->h[4] += e;
    c->h[5] += f;
    c->h[6] += g;
    c->h[7] += h;
}

void rogue_sha256_update(RogueSHA256Ctx* c, const void* data, size_t len)
{
    const unsigned char* p = (const unsigned char*) data;
    c->len += len;
    while (len > 0)
    {
        size_t space = 64 - c->buf_len;
        size_t take = len < space ? len : space;
        memcpy(c->buf + c->buf_len, p, take);
        c->buf_len += take;
        p += take;
        len -= take;
        if (c->buf_len == 64)
        {
            rogue_sha256_block(c, c->buf);
            c->buf_len = 0;
        }
    }
}

void rogue_sha256_final(RogueSHA256Ctx* c, unsigned char out[32])
{
    uint64_t bit_len = c->len * 8;
    unsigned char pad = 0x80;
    rogue_sha256_update(c, &pad, 1);
    unsigned char z = 0;
    while (c->buf_len != 56)
    {
        rogue_sha256_update(c, &z, 1);
    }
    unsigned char len_be[8];
    for (int i = 0; i < 8; i++)
    {
        len_be[7 - i] = (unsigned char) (bit_len >> (i * 8));
    }
    rogue_sha256_update(c, len_be, 8);
    for (int i = 0; i < 8; i++)
    {
        out[4 * i] = (unsigned char) (c->h[i] >> 24);
        out[4 * i + 1] = (unsigned char) (c->h[i] >> 16);
        out[4 * i + 2] = (unsigned char) (c->h[i] >> 8);
        out[4 * i + 3] = (unsigned char) (c->h[i]);
    }
}

int rogue_save_format_endianness_is_le(void)
{
    uint32_t x = 0x01020304u;
    unsigned char* p = (unsigned char*) &x;
    return p[0] == 0x04;
}
