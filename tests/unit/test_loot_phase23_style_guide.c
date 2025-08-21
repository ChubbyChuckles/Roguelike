#include "core/path_utils.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Phase 23.2: Ensure style guide doc discoverable and contains key sections */
int main(void)
{
    char path[256];
    int ok = rogue_find_doc_path("LOOT_RARITY_AFFIX_STYLE.md", path, sizeof path);
    assert(ok == 1);
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    assert(f != NULL);
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    fclose(f);
    assert(strstr(buf, "Rarity & Affix Style Guide") != NULL);
    assert(strstr(buf, "Affix ID Naming") != NULL);
    assert(strstr(buf, "Stat Range Guidelines") != NULL);
    assert(strstr(buf, "Adding a New Affix – Checklist") != NULL ||
           strstr(buf, "Adding a New Affix - Checklist") != NULL);
    return 0;
}
