#define ROGUE_CJSON_STUB 1
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

/* Extremely small stub so code can compile/link; not a real JSON parser. */
cJSON* cJSON_Parse(const char* value)
{
    (void) value;
    cJSON* n = (cJSON*) calloc(1, sizeof(cJSON));
    return n;
}

void cJSON_Delete(cJSON* item)
{
    if (!item)
        return;
    free(item->valuestring);
    free(item->string);
    free(item);
}

char* cJSON_Print(const cJSON* item)
{
    (void) item;
    char* s = (char*) malloc(3);
    if (s)
    {
        s[0] = '{';
        s[1] = '}';
        s[2] = '\0';
    }
    return s;
}
