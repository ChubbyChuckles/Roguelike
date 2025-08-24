#ifndef ROGUE_INPUT_EVENTS_H
#define ROGUE_INPUT_EVENTS_H
#include "app_state.h"
void rogue_process_events(void);
/* Process any skill activations queued during event polling (run after movement). */
void rogue_process_pending_skill_activations(void);
#endif /* ROGUE_INPUT_EVENTS_H */
