#ifndef ROGUE_CONTENT_JSON_ENVELOPE_H
#define ROGUE_CONTENT_JSON_ENVELOPE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueJsonEnvelope
    {
        char* schema; /* UTF-8 string, owned by caller after parse */
        uint32_t version;
        char* entries; /* JSON text of the entries value (array or object), owned by caller */
    } RogueJsonEnvelope;

    /* Compose a versioned JSON envelope string:
     *  { "$schema": "<schema>", "version": <version>, "entries": <entries_json> }
     * Returns 0 on success and allocates *out_json (caller frees with free()).
     */
    int json_envelope_create(const char* schema, uint32_t version, const char* entries_json,
                             char** out_json, char* err, int err_cap);

    /* Parse a versioned JSON envelope. On success, fills out_env fields with allocated strings.
     * Caller must free with json_envelope_free(). Returns 0 on success.
     */
    int json_envelope_parse(const char* json_text, RogueJsonEnvelope* out_env, char* err,
                            int err_cap);

    /* Free fields allocated by json_envelope_parse. Safe on NULL. */
    void json_envelope_free(RogueJsonEnvelope* env);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CONTENT_JSON_ENVELOPE_H */
