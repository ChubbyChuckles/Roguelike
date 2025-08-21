/* test_kv_parser.c - Unit tests for unified key/value parser (Phase M3.1) */
#include "util/kv_parser.h"
#include <stdio.h>
#include <string.h>

static const char* sample = "# comment\n"
                            " key1 = value one  \n"
                            "key2=42\n"
                            "invalid_line_without_equal\n"
                            "key3 = spaced = value ; trailing comment\n"
                            "; full line comment\n"
                            "empty_key=\n";

int main(void)
{
    RogueKVFile kv = {sample, (int) strlen(sample)};
    int cursor = 0;
    RogueKVEntry e;
    RogueKVError err;
    int ok_count = 0;
    int saw_error = 0;
    while (1)
    {
        int r = rogue_kv_next(&kv, &cursor, &e, &err);
        if (r == 1)
        {
            ok_count++;
            if (e.line == 2 && strcmp(e.key, "key1") != 0)
            {
                printf("FAIL key1 name\n");
                return 1;
            }
        }
        else
        {
            if (cursor < kv.length)
                saw_error = 1;
            else
                break;
        }
        if (cursor >= kv.length)
            break;
    }
    if (ok_count < 3)
    {
        printf("FAIL expected >=3 entries got %d\n", ok_count);
        return 1;
    }
    if (!saw_error)
    {
        printf("FAIL expected an error for invalid line\n");
        return 1;
    }
    printf("OK test_kv_parser (%d entries)\n", ok_count);
    return 0;
}
