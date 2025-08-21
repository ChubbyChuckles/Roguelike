#ifndef ROGUE_CORE_SKILL_TREE_H
#define ROGUE_CORE_SKILL_TREE_H

/* Simple skill tree panel to allocate talent points */
void rogue_skill_tree_toggle(void);
void rogue_skill_tree_render(void);
void rogue_skill_tree_handle_key(int sdl_key_sym);
int rogue_skill_tree_is_open(void);

/* Register built-in baseline skills */
void rogue_skill_tree_register_baseline(void);

#endif
