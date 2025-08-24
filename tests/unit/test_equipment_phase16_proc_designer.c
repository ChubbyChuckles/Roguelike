/* Phase 16.3: Proc designer JSON tooling test */
#include "../../src/core/equipment/equipment_procs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_temp_procs(const char* path)
{
    const char* json =
        "[\n"
        /* duration_ms extended to ensure overlap across 2 ICD windows for stacking test */
        " {\"name\":\"BurningAegis\",\"trigger\":\"ON_BLOCK\",\"icd_ms\":5000,\"duration_ms\":"
        "12000,\"magnitude\":50,\"max_stacks\":3,\"stack_rule\":\"STACK\",\"param\":0},\n"
        " {\"name\":\"RelentlessStrikes\",\"trigger\":\"ON_HIT\",\"icd_ms\":1000,\"duration_ms\":"
        "1500,\"magnitude\":10,\"max_stacks\":1,\"stack_rule\":\"REFRESH\",\"param\":0}\n"
        "]";
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0)
        f = NULL;
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return 0;
    fwrite(json, 1, strlen(json), f);
    fclose(f);
    return 1;
}

static int simulate_block_sequence(int proc_id)
{
    /* Ensure stacking works: trigger multiple blocks separated by >ICD, avoid expiring after last
     * trigger */
    for (int i = 0; i < 3; i++)
    {
        rogue_procs_event_block();
        if (i < 2)
            rogue_procs_update(5000, 100, 100);
    }
    int stacks = rogue_proc_active_stacks(proc_id);
    if (stacks <= 0)
        return 0; /* magnitude effect implied by stacks */
    return stacks;
}

int main(void)
{
    rogue_procs_reset();
    const char* path = "temp_procs_phase16.json";
    if (!write_temp_procs(path))
    {
        fprintf(stderr, "write procs json failed\n");
        return 1;
    }
    int added = rogue_procs_load_from_json(path);
    if (added != 2)
    {
        fprintf(stderr, "expected 2 procs added got %d\n", added);
        return 2;
    }
    if (rogue_proc_count() < 2)
    {
        fprintf(stderr, "registry size wrong\n");
        return 3;
    }
    /* Find block proc (id order matches registration insertion) */
    int block_id = -1, hit_id = -1;
    for (int i = 0; i < rogue_proc_count(); i++)
    {
        const RogueProcDef* d = rogue_proc_def(i);
        if (!d)
            continue;
        if (d->trigger == ROGUE_PROC_ON_BLOCK)
            block_id = i;
        if (d->trigger == ROGUE_PROC_ON_HIT)
            hit_id = i;
    }
    if (block_id < 0 || hit_id < 0)
    {
        fprintf(stderr, "failed to locate proc ids\n");
        return 4;
    }
    int stacks = simulate_block_sequence(block_id);
    if (stacks < 2)
    {
        fprintf(stderr, "expected stacking >=2 got %d\n", stacks);
        return 5;
    }
    /* Hit proc refresh rule: trigger twice within duration and ensure stack stays 1 */
    rogue_procs_event_hit(0);
    rogue_procs_update(500, 100, 100);
    int s1 = rogue_proc_active_stacks(hit_id);
    if (s1 != 1)
    {
        fprintf(stderr, "expected 1 stack after first hit got %d\n", s1);
        return 6;
    }
    rogue_procs_event_hit(0);
    rogue_procs_update(500, 100, 100);
    int s2 = rogue_proc_active_stacks(hit_id);
    if (s2 != 1)
    {
        fprintf(stderr, "refresh rule broken got %d\n", s2);
        return 7;
    }
    char buf[1024];
    int n = rogue_procs_export_json(buf, (int) sizeof buf);
    if (n < 0)
    {
        fprintf(stderr, "export failed\n");
        return 8;
    }
    if (strstr(buf, "BurningAegis") == NULL || strstr(buf, "RelentlessStrikes") == NULL)
    {
        fprintf(stderr, "export missing names\n");
        return 9;
    }
    remove(path);
    printf("Phase16.3 proc designer load/stack/refresh OK (%d chars)\n", n);
    return 0;
}
