#include "game/combat.h"
#include "game/combat_internal.h"

/* Test / injection hooks (obstruction) */
static int (*g_obstruction_line_test)(float sx,float sy,float ex,float ey) = NULL;
void rogue_combat_set_obstruction_line_test(int (*fn)(float,float,float,float)){ g_obstruction_line_test = fn; }
int _rogue_combat_call_obstruction_test(float sx,float sy,float ex,float ey){ return g_obstruction_line_test? g_obstruction_line_test(sx,sy,ex,ey) : -1; }
