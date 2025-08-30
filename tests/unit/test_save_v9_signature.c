/* Test v9 optional signature provider (dummy) */
#include "../../src/core/persistence/save_manager.h"
#include "../../src/core/persistence/save_paths.h"
#include <stdio.h>
#include <string.h>

/* Buff stub (deduplicated) */
/* Use real buff system */

static int dummy_sign(const unsigned char* data, size_t len, unsigned char* out, uint32_t* out_len)
{ /* simple XOR checksum signature */
    unsigned char x = 0;
    for (size_t i = 0; i < len; i++)
        x ^= data[i];
    if (*out_len < 1)
        return -1;
    out[0] = x;
    *out_len = 1;
    return 0;
}
static int dummy_verify(const unsigned char* data, size_t len, const unsigned char* sig,
                        uint32_t sig_len)
{
    if (sig_len != 1)
        return -1;
    unsigned char x = 0;
    for (size_t i = 0; i < len; i++)
        x ^= data[i];
    return (x == sig[0]) ? 0 : -1;
}

int main(void)
{
    if (ROGUE_SAVE_FORMAT_VERSION < 9u)
    {
        printf("SIG_SKIP v=%u\n", (unsigned) ROGUE_SAVE_FORMAT_VERSION);
        return 0;
    }
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    RogueSaveSignatureProvider prov = {1, dummy_sign, dummy_verify, "dummy_xor"};
    rogue_save_set_signature_provider(&prov);
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("SIG_FAIL save\n");
        return 1;
    }
    /* Load (should verify) */
    int rc = rogue_save_manager_load_slot(0);
    if (rc != 0)
    {
        printf("SIG_FAIL load rc=%d flags=0x%X\n", rc, rogue_save_last_tamper_flags());
        return 1;
    }
    printf("SIG_OK provider=%s flags=0x%X\n", prov.name, rogue_save_last_tamper_flags());
    /* Now corrupt signature byte and expect tamper flag signature + error */
#if defined(_MSC_VER)
    FILE* f = NULL;
    fopen_s(&f, rogue_build_slot_path(0), "r+b");
#else
    FILE* f = fopen(rogue_build_slot_path(0), "r+b");
#endif
    if (f)
    {
        fseek(f, -1, SEEK_END);
        int c = fgetc(f);
        if (c != EOF)
        {
            unsigned char b = (unsigned char) c;
            b ^= 0xFF;
            fseek(f, -1, SEEK_CUR);
            fwrite(&b, 1, 1, f);
        }
        fclose(f);
    }
    else
    {
        printf("SIG_FAIL reopen\n");
        return 1;
    }
    rc = rogue_save_manager_load_slot(0);
    if (rc == 0)
    {
        printf("SIG_FAIL expected tamper rc flags=0x%X\n", rogue_save_last_tamper_flags());
        return 1;
    }
    if ((rogue_save_last_tamper_flags() & ROGUE_TAMPER_FLAG_SIGNATURE) == 0)
    {
        printf("SIG_FAIL no signature flag tf=0x%X rc=%d\n", rogue_save_last_tamper_flags(), rc);
        return 1;
    }
    printf("SIG_TAMPER_OK rc=%d tf=0x%X\n", rc, rogue_save_last_tamper_flags());
    return 0;
}
