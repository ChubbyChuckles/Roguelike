#ifndef ROGUE_THIRDPARTY_CJSON_H
#define ROGUE_THIRDPARTY_CJSON_H

/* Minimal stub header for cJSON to satisfy build integration until the real
 * library is dropped in. This is NOT a full implementation. Define
 * ROGUE_CJSON_STUB=1 when using this stub. */

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct cJSON
    {
        /* opaque */
        int type;
        char* valuestring;
        int valueint;
        double valuedouble;
        struct cJSON* next;
        struct cJSON* prev;
        struct cJSON* child;
        char* string;
    } cJSON;

    /* Parser API (stubbed) */
    cJSON* cJSON_Parse(const char* value);
    void cJSON_Delete(cJSON* item);
    char* cJSON_Print(const cJSON* item);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_THIRDPARTY_CJSON_H */
