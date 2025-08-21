/* Phase 17.1: Public schema docs generator for equipment-related JSON assets.
   Provides APIs to emit canonical JSON schema-like documentation for:
     - Item definitions
     - Set definitions
     - Proc definitions
     - Runeword patterns (pattern rule summary)
   Output is a self-contained JSON object with version + sections.
*/
#ifndef ROGUE_EQUIPMENT_SCHEMA_DOCS_H
#define ROGUE_EQUIPMENT_SCHEMA_DOCS_H
#ifdef __cplusplus
extern "C"
{
#endif

    int rogue_equipment_schema_docs_export(char* buf, int cap); /* returns bytes written or -1 */

#ifdef __cplusplus
}
#endif
#endif
