#ifndef ROGUE_AI_LOCAL_AVOIDANCE_H
#define ROGUE_AI_LOCAL_AVOIDANCE_H

/* Given a desired cardinal step (dx,dy) from (x,y), try to avoid immediate collisions by
 * probing side alternatives. Returns 1 if adjusted, 0 if original is fine, -1 if no move. */
int rogue_local_avoid_adjust(int x, int y, int* dx, int* dy);

#endif /* ROGUE_AI_LOCAL_AVOIDANCE_H */
