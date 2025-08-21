#include "core/save_manager.h"
#include <stdio.h>
#include <string.h>

/* Use real buff system (no local stubs). */

int main(void)
{
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("JSON_EXPORT_FAIL save\n");
        return 1;
    }
    char buf[512];
    if (rogue_save_export_json(0, buf, sizeof buf) != 0)
    {
        printf("JSON_EXPORT_FAIL export\n");
        return 1;
    }
    if (strstr(buf, "\"sections\"[") || strstr(buf, "id\":0"))
    {
        printf("JSON_EXPORT_FAIL malformed\n");
        return 1;
    }
    if (!strstr(buf, "\"version\"") || !strstr(buf, "\"sections\""))
    {
        printf("JSON_EXPORT_FAIL missing_fields\n");
        return 1;
    }
    printf("JSON_EXPORT_OK len=%zu\n", strlen(buf));
    return 0;
}
