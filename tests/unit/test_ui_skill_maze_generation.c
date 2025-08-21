#include "core/skill_maze.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    printf("START\n");
    RogueSkillMaze m;
    int ok = rogue_skill_maze_generate("assets/skill_maze_config.json", &m);
    printf("AFTER_GEN ok=%d nodes=%d edges=%d rings=%d\n", ok, m.node_count, m.edge_count, m.rings);
    assert(ok);
    assert(m.rings >= 2);
    assert(m.node_count > 0);
    assert(m.edge_count > 0); /* basic structural sanity */
    int ring_max = 0;
    for (int i = 0; i < m.node_count; i++)
    {
        if (m.nodes[i].ring > ring_max)
            ring_max = m.nodes[i].ring;
    }
    for (int e = 0; e < m.edge_count; e++)
    {
        if (m.edges[e].from < 0 || m.edges[e].from >= m.node_count || m.edges[e].to < 0 ||
            m.edges[e].to >= m.node_count)
        {
            printf("EDGE_OOB e=%d from=%d to=%d node_count=%d\n", e, m.edges[e].from, m.edges[e].to,
                   m.node_count);
            return 1;
        }
    }
    printf("RING_MAX=%d\n", ring_max);
    assert(ring_max == m.rings);
    printf("PTR nodes=%p edges=%p\n", (void*) m.nodes, (void*) m.edges);
    rogue_skill_maze_free(&m);
    printf("AFTER_FREE nodes=%p edges=%p\n", (void*) m.nodes, (void*) m.edges);
    printf("maze_generation_ok\n");
    return 0;
}
