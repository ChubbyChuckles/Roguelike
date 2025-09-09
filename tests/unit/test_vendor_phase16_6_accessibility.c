#include "../../src/core/vendor/vendor_ui.h"
#include <stdio.h>
#include <string.h>

static int expect_contains(const char* hay, const char* needle)
{
    return strstr(hay, needle) != NULL;
}

int main(void)
{
    /* Text-only toggle should impact formatting: include [was XG, +/-Y%] suffix. */
    rogue_vendor_ui_set_text_only(1);
    char buf[256];
    int n = rogue_vendor_ui_format_price_line("Potion", 1, 110, 100, 1, /*high_contrast*/ 0, buf,
                                              (int) sizeof buf);
    if (n <= 0)
    {
        printf("FAIL: formatter returned %d\n", n);
        return 1;
    }
    if (!expect_contains(buf, "[was 100G"))
    {
        printf("FAIL: missing previous price in text-only: %s\n", buf);
        return 1;
    }
    if (!expect_contains(buf, "+10%"))
    {
        printf("FAIL: missing +10%% delta in text-only: %s\n", buf);
        return 1;
    }

    /* High-contrast formatting in non-text-only appends percent delta */
    rogue_vendor_ui_set_text_only(0);
    n = rogue_vendor_ui_format_price_line("Potion", 1, 92, 100, 0, /*high_contrast*/ 1, buf,
                                          (int) sizeof buf);
    if (!expect_contains(buf, "-8%"))
    {
        printf("FAIL: missing -8%% delta in high-contrast: %s\n", buf);
        return 1;
    }

    /* Non-high-contrast should not append delta */
    n = rogue_vendor_ui_format_price_line("Potion", 1, 92, 100, 0, /*high_contrast*/ 0, buf,
                                          (int) sizeof buf);
    if (expect_contains(buf, "%"))
    {
        printf("FAIL: unexpected percent in normal mode: %s\n", buf);
        return 1;
    }

    printf("OK\n");
    return 0;
}
