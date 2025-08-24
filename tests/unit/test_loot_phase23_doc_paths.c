#include "../../src/core/path_utils.h"
#include <assert.h>
#include <string.h>

/* Test newly added rogue_find_doc_path helper (Cleanup 24.1 extension) */
int main(void)
{
    char p[256];
    int ok = rogue_find_doc_path("OWNERSHIP.md", p, sizeof p);
    assert(ok == 1);
    assert(strstr(p, "OWNERSHIP.md") != NULL);
    /* Negative case */
    ok = rogue_find_doc_path("nonexistent_doc_12345.md", p, sizeof p);
    assert(ok == 0);
    return 0;
}
