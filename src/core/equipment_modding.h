/* Phase 17.3/17.4: Sandboxed scripting hooks (minimal) + set diff tool.
 * Script format (deterministic, tiny): up to 16 lines, each either:
 *   add <stat_name> <int_value>
 *   mul <stat_name> <percent>
 * Supported stats: strength,dexterity,vitality,intelligence,armor_flat,
 *  resist_fire,resist_cold,resist_light,resist_poison,resist_status,resist_physical
 * Comments (#...) and blank lines ignored. Any invalid token sequence aborts load.
 */
#ifndef ROGUE_EQUIPMENT_MODDING_H
#define ROGUE_EQUIPMENT_MODDING_H
#ifdef __cplusplus
extern "C" { 
#endif

typedef struct RogueSandboxInstr { unsigned char op; unsigned char stat; int value; } RogueSandboxInstr; /* op:1=ADD,2=MUL */
typedef struct RogueSandboxScript { int instr_count; RogueSandboxInstr instrs[16]; } RogueSandboxScript;

int rogue_script_load(const char* path, RogueSandboxScript* out); /* 0 ok else -1 */
unsigned int rogue_script_hash(const RogueSandboxScript* s); /* FNV-1a fold */
void rogue_script_apply(const RogueSandboxScript* s, int* strength,int* dexterity,int* vitality,int* intelligence,int* armor_flat,int* r_fire,int* r_cold,int* r_light,int* r_poison,int* r_status,int* r_phys);

/* Phase 17.4 set diff tool */
int rogue_sets_diff(const char* base_path, const char* mod_path, char* out, int cap);

#ifdef __cplusplus
}
#endif

#endif
