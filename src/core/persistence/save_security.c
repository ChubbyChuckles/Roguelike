#include "save_internal.h"

int rogue_save_set_signature_provider(const RogueSaveSignatureProvider* prov)
{
    g_save_sig_provider = prov;
    return 0;
}

const RogueSaveSignatureProvider* rogue_save_get_signature_provider(void)
{
    return g_save_sig_provider;
}

uint32_t rogue_save_last_tamper_flags(void) { return g_save_last_tamper_flags; }
int rogue_save_last_recovery_used(void) { return g_save_last_recovery_used; }
