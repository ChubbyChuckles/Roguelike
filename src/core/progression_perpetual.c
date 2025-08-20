#include "core/progression_perpetual.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Internal state */
static int g_spent = 0; /* micro-nodes spent */
static double g_global_coeff = 1.0; /* adjustable balancing coefficient */

int rogue_perpetual_init(void){ g_spent=0; g_global_coeff=1.0; return 0; }
void rogue_perpetual_shutdown(void){ g_spent=0; }
void rogue_perpetual_reset(void){ g_spent=0; }

int rogue_perpetual_micro_nodes_allowed(int level){ if(level < 0) return 0; int base = level/2; int milestone = level/25; return base + milestone; }
int rogue_perpetual_micro_nodes_spent(void){ return g_spent; }

int rogue_perpetual_spend_node(int level){ int allowed = rogue_perpetual_micro_nodes_allowed(level); if(g_spent >= allowed) return 0; g_spent++; return 1; }

/* Per-node diminishing contribution constants */
static const double NODE_BASE = 0.015; /* early power per node */
static const double NODE_CURV = 0.07;  /* curvature for diminishing */

double rogue_perpetual_raw_power(void){ double acc=0.0; for(int i=0;i<g_spent;i++){ acc += NODE_BASE / (1.0 + NODE_CURV * (double)i); } return acc; }

double rogue_perpetual_level_scalar(int level){ if(level<=0) return 0.0; const double LAMBDA=140.0; const double ALPHA=0.80; double x = 1.0 - exp(-(double)level / LAMBDA); return pow(x, ALPHA); }

void rogue_perpetual_set_global_coeff(double coeff){ if(coeff < 0.1) coeff=0.1; if(coeff>5.0) coeff=5.0; g_global_coeff=coeff; }
double rogue_perpetual_get_global_coeff(void){ return g_global_coeff; }

double rogue_perpetual_inflation_adjust(double median_ttk_delta){ /* target zero; positive = combat slower -> slight buff; negative = too fast -> nerf */
    double adj = g_global_coeff;
    double k = 0.05; /* gentle proportional */
    adj *= (1.0 + (-k * median_ttk_delta)); /* if delta positive (slower), we reduce nerf (negative effect), so invert sign */
    if(adj < 0.85) adj = 0.85; else if(adj > 1.15) adj = 1.15; g_global_coeff = adj; return g_global_coeff; }

double rogue_perpetual_effective_power(int level){ double base = rogue_perpetual_level_scalar(level); double nodes = rogue_perpetual_raw_power(); return (base + nodes) * g_global_coeff; }
