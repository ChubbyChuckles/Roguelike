#define SDL_MAIN_HANDLED 1
#include "../../src/core/integration/tag_registry.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    char err[128];
    /* ctest runs from the build directory; assets live one level up */
    int ok = rogue_tag_registry_validate_file("../assets/tag_registry.json", err, sizeof err);
    assert(ok == 0);
    const char* bad = "{\"skills\":[\"ok\",\"bad space\"]}";
    int bad_rc = rogue_tag_registry_validate_text(bad, err, sizeof err);
    assert(bad_rc < 0);
    printf("OK\n");
    return 0;
}
