#ifndef ROGUE_CORE_SKILL_TREE_H
#define ROGUE_CORE_SKILL_TREE_H

/* Simple skill tree panel to allocate talent points */
void rogue_skill_tree_toggle(void);
void rogue_skill_tree_render(void);
void rogue_skill_tree_handle_key(int sdl_key_sym);
int rogue_skill_tree_is_open(void);

/* Register built-in baseline skills */
void rogue_skill_tree_register_baseline(void);

/* Phase 1.3: Enhanced Execution Pathways
        Lightweight runtime-selectable alternate execution variants for sample skills.
        Pathways are intentionally simple (0=DEFAULT,1=EMPOWERED,2=UTILITY) and only
        affect skills whose activation callback was replaced by the internal dispatch
        wrapper (currently Fireball). Non‑wrapped skills ignore pathway changes. */
void rogue_skill_pathway_set(int skill_id, int pathway); /* pathway clamped 0..2 */
int rogue_skill_pathway_get(int skill_id);               /* returns 0..2 (default 0) */
int rogue_skill_pathway_last_exec(int skill_id);         /* last executed pathway or -1 */

#endif
