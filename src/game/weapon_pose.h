#ifndef ROGUE_WEAPON_POSE_H
#define ROGUE_WEAPON_POSE_H

#ifdef __cplusplus
extern "C" { 
#endif

typedef struct RogueWeaponPoseFrame {
    float dx, dy;      /* pixel offsets relative to player sprite center */
    float angle;       /* degrees */
    float scale;       /* uniform scale */
    float pivot_x;     /* 0..1 normalized within weapon frame */
    float pivot_y;     /* 0..1 normalized within weapon frame */
} RogueWeaponPoseFrame;

/* Ensure pose + textures for weapon id loaded (lazy). Returns 1 on success, 0 on failure. */
int rogue_weapon_pose_ensure(int weapon_id);
/* Retrieve frame pose (NULL if not loaded or out of range). */
const RogueWeaponPoseFrame* rogue_weapon_pose_get(int weapon_id, int frame_index);
/* Get SDL texture pointer (void* to avoid SDL include in header). */
void* rogue_weapon_pose_get_texture(int weapon_id, int frame_index, int* w, int* h);

#ifdef __cplusplus
}
#endif

#endif
