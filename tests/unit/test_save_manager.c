#include "../../src/core/save_manager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int dummy_write(FILE* f)
{
    const char data[] = "ABC";
    fwrite(data, 1, sizeof data, f);
    return 0;
}
static int dummy_read(FILE* f, size_t size)
{
    char buf[8] = {0};
    fread(buf, 1, size < sizeof buf ? size : sizeof buf, f);
    return (buf[0] == 'A') ? 0 : -1;
}
static RogueSaveComponent DUMMY = {10, dummy_write, dummy_read, "dummy"};

/* Use real buff system (no local stubs). */

int main()
{
    rogue_save_manager_init();
    rogue_save_manager_register(&DUMMY);
    int rc = rogue_save_manager_save_slot(0);
    assert(rc == 0);
    rc = rogue_save_manager_load_slot(0);
    assert(rc == 0);
    printf("save_manager basic test passed\n");
    return 0;
}
