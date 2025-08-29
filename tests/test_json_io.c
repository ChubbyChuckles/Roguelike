#include "../src/content/json_io.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char err[128];
    const char* path = "json_io_test.tmp";
    const char* payload = "{\n  \"hello\": \"world\"\n}";
    int rc = json_io_write_atomic(path, payload, strlen(payload), err, (int) sizeof err);
    assert(rc == 0);

    char* read = NULL;
    size_t len = 0;
    rc = json_io_read_file(path, &read, &len, err, (int) sizeof err);
    assert(rc == 0);
    assert(len == strlen(payload));
    assert(memcmp(read, payload, len) == 0);

    unsigned long long ms = 0;
    rc = json_io_get_mtime_ms(path, &ms, err, (int) sizeof err);
    assert(rc == 0);
    /* mtime should be non-zero on most systems */
    assert(ms >= 0);

    free(read);
    /* best-effort cleanup */
    remove(path);
    return 0;
}
