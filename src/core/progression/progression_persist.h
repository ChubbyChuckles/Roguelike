/* Phase 12: Progression Persistence & Migration */
#ifndef ROGUE_PROGRESSION_PERSIST_H
#define ROGUE_PROGRESSION_PERSIST_H

#include <stdint.h>
#include <stdio.h>

#define ROGUE_PROG_MIG_STAT_REG_CHANGED 0x1u
#define ROGUE_PROG_MIG_MAZE_NODECOUNT 0x2u
#define ROGUE_PROG_MIG_TALENT_SCHEMA 0x4u /* passive/talent node set changed */
#define ROGUE_PROG_MIG_ATTR_REPLAY 0x8u   /* attribute journal replay applied */

int rogue_progression_persist_register(void);
int rogue_progression_persist_write(FILE* f);
int rogue_progression_persist_read(FILE* f);
unsigned long long rogue_progression_persist_chain_hash(void);
unsigned int rogue_progression_persist_last_migration_flags(void);
/* For tests: expose version constant */
unsigned int rogue_progression_persist_version(void);
void rogue_progression_persist_reset_state_for_tests(void);

#endif
