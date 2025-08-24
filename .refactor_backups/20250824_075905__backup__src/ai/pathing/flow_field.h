#ifndef ROGUE_AI_FLOW_FIELD_H
#define ROGUE_AI_FLOW_FIELD_H

#include "../../core/navigation.h"

typedef struct RogueFlowField
{
    int width;
    int height;
    /* Distances from each cell to the target via walkable cells; INF if unreachable */
    float* dist; /* size = width*height */
    /* Step direction from a cell toward the target; 0,0 if unreachable or blocked */
    signed char* dir_x; /* size = width*height */
    signed char* dir_y; /* size = width*height */
    int target_x;
    int target_y;
} RogueFlowField;

/* Build a flow field pointing all reachable cells toward (tx,ty) on the current world map. */
int rogue_flow_field_build(int tx, int ty, RogueFlowField* out_ff);

/* Free any heap memory owned by the flow field. */
void rogue_flow_field_free(RogueFlowField* ff);

/* Query the recommended cardinal step from (x,y) toward target; returns 0 on success. */
int rogue_flow_field_step(const RogueFlowField* ff, int x, int y, int* out_dx, int* out_dy);

#endif /* ROGUE_AI_FLOW_FIELD_H */
