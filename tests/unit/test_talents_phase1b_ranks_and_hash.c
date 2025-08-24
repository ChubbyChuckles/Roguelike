/* Unit test: Talents ranks serialization v2 + hash stability
   - Build a tiny 2-node line maze
   - Unlock both nodes; verify ranks reported as 1
   - Serialize (v2) and ensure header==2 and varint ranks decode to 1,1
   - Deserialize a crafted v1 buffer (no ranks), ensure ranks restored to 1 via legacy path
   - Verify hash is stable across v2->shutdown->v2 and v1->shutdown->v2 load paths
   - Respec all and verify ranks reset to 0 */
#include "../../src/core/app/app_state.h"
#include "../../src/core/progression/progression_maze.h"
#include "../../src/core/skills/skill_talents.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void build_maze2(RogueProgressionMaze* mz)
{
    memset(mz, 0, sizeof *mz);
    mz->base.node_count = 2;
    mz->base.rings = 1;
    mz->base.nodes = (RogueSkillMazeNode*) calloc(2, sizeof(RogueSkillMazeNode));
    for (int i = 0; i < 2; ++i)
        mz->base.nodes[i].ring = 0;
    mz->meta = (RogueProgressionMazeNodeMeta*) calloc(2, sizeof(RogueProgressionMazeNodeMeta));
    for (int i = 0; i < 2; ++i)
    {
        mz->meta[i].node_id = i;
        mz->meta[i].ring = 0;
        mz->meta[i].level_req = 1;
        mz->meta[i].cost_points = 1;
        mz->meta[i].adj_start = i;
        mz->meta[i].adj_count = 1;
    }
    mz->adjacency = (int*) calloc(2, sizeof(int));
    mz->adjacency[0] = 1; /* 0 adjacent to 1 */
    mz->adjacency[1] = 0; /* 1 adjacent to 0 */
    mz->total_adjacency = 2;
}

/* local copy of varint decode for test */
static int varint_decode_u32(const unsigned char* in, int in_len, unsigned int* out_val)
{
    unsigned int result = 0;
    int shift = 0;
    int n = 0;
    while (n < in_len)
    {
        unsigned char byte = in[n++];
        result |= ((unsigned int) (byte & 0x7Fu)) << shift;
        if (!(byte & 0x80u))
            break;
        shift += 7;
        if (shift > 28)
            break;
    }
    *out_val = result;
    return n;
}

int main(void)
{
    RogueProgressionMaze mz;
    build_maze2(&mz);
    assert(rogue_talents_init(&mz) == 0);
    g_app.talent_points = 5;

    /* Unlock both nodes */
    assert(rogue_talents_unlock(0, 0, 1, 0, 0, 0, 0) == 1);
    assert(rogue_talents_unlock(1, 0, 1, 0, 0, 0, 0) == 1);
    assert(rogue_talents_get_rank(0) == 1);
    assert(rogue_talents_get_rank(1) == 1);

    unsigned char buf[128];
    int wrote = rogue_talents_serialize(buf, sizeof buf);
    assert(wrote > 0);
    assert(buf[0] == 2); /* v2 header */
    int node_count = (int) buf[1] | ((int) buf[2] << 8) | ((int) buf[3] << 16);
    assert(node_count == 2);
    /* ranks start at offset bytes+4 */
    int offset = 4 + node_count; /* we store 1 byte per node for unlocked */
    unsigned int r0 = 0, r1 = 0;
    int used0 = varint_decode_u32(buf + offset, wrote - offset, &r0);
    assert(used0 > 0);
    int used1 = varint_decode_u32(buf + offset + used0, wrote - offset - used0, &r1);
    assert(used1 > 0);
    assert(r0 == 1 && r1 == 1);
    unsigned long long h_v2 = rogue_talents_hash();

    /* Reload v2 and confirm hash equal */
    rogue_talents_shutdown();
    assert(rogue_talents_init(&mz) == 0);
    int read2 = rogue_talents_deserialize(buf, (size_t) wrote);
    assert(read2 == wrote);
    unsigned long long h_v2_rt = rogue_talents_hash();
    assert(h_v2 == h_v2_rt);
    assert(rogue_talents_get_rank(0) == 1);
    assert(rogue_talents_get_rank(1) == 1);

    /* Craft a v1 buffer (no ranks), ensure legacy path sets rank=1 for unlocked */
    unsigned char v1[8];
    v1[0] = 1; /* version 1 */
    v1[1] = (unsigned char) (node_count & 0xFF);
    v1[2] = (unsigned char) ((node_count >> 8) & 0xFF);
    v1[3] = (unsigned char) ((node_count >> 16) & 0xFF);
    /* unlocked bytes (2 nodes, both 1) */
    v1[4] = 1;
    v1[5] = 1;
    int v1_size = 6; /* 4 header + 2 unlocked bytes */

    rogue_talents_shutdown();
    assert(rogue_talents_init(&mz) == 0);
    int read1 = rogue_talents_deserialize(v1, (size_t) v1_size);
    assert(read1 == v1_size);
    assert(rogue_talents_get_rank(0) == 1);
    assert(rogue_talents_get_rank(1) == 1);
    unsigned long long h_v1 = rogue_talents_hash();
    assert(h_v1 == h_v2);

    /* Full respec clears ranks */
    int refunded = rogue_talents_full_respec();
    assert(refunded == 2);
    assert(rogue_talents_get_rank(0) == 0);
    assert(rogue_talents_get_rank(1) == 0);

    rogue_talents_shutdown();
    rogue_progression_maze_free(&mz);
    printf("test_talents_phase1b_ranks_and_hash: OK\n");
    return 0;
}
