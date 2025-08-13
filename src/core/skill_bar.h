#ifndef ROGUE_CORE_SKILL_BAR_H
#define ROGUE_CORE_SKILL_BAR_H

void rogue_skill_bar_render(void);
void rogue_skill_bar_set_slot(int slot, int skill_id);
int rogue_skill_bar_get_slot(int slot);
void rogue_skill_bar_update(float dt_ms);
void rogue_skill_bar_flash(int slot);

#endif
