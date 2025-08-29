#ifndef ROGUE_SAVE_INTERN_H
#define ROGUE_SAVE_INTERN_H

#include <stddef.h>

/* Public string interning API used across systems */
int rogue_save_intern_string(const char* s);  /* returns index (>=0) or -1 on failure */
const char* rogue_save_intern_get(int index); /* returns interned string or NULL */
int rogue_save_intern_count(void);

/* Internal helpers used by strings component loader */
void rogue_save_intern_reset_and_reserve(int count);
void rogue_save_intern_set_loaded(int index, char* owned_string);

#endif /* ROGUE_SAVE_INTERN_H */
