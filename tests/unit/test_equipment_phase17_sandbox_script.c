/* Phase 17.3: sandbox scripting loader/apply/hash test */
#include "../../src/core/equipment/equipment_modding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_file(const char* path, const char* text)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "wb");
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

static void ck(int c, const char* m)
{
    if (!c)
    {
        printf("FAIL:%s\n", m);
        exit(1);
    }
}

int main()
{
    const char* path = "tmp_sandbox_script.txt";
    const char* script =
        "# test script\nadd strength 10\nadd armor_flat 25\nmul strength 20\n"; /* strength: (0+10)
                                                                                   +20% => 12 */
    ck(write_file(path, script) == 0, "write script");
    RogueSandboxScript s;
    ck(rogue_script_load(path, &s) == 0, "load ok");
    ck(s.instr_count == 3, "instr count");
    unsigned int h1 = rogue_script_hash(&s);
    ck(h1 != 0, "hash nonzero");
    int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
    int rf = 0, rc = 0, rl = 0, rp = 0, rs = 0, rph = 0;
    rogue_script_apply(&s, &str, &dex, &vit, &intel, &armor, &rf, &rc, &rl, &rp, &rs, &rph);
    ck(str == 12 && armor == 25, "apply math");
    /* invalid line test */
    const char* bad = "foo strength 5\n";
    ck(write_file(path, bad) == 0, "write bad");
    RogueSandboxScript s2;
    ck(rogue_script_load(path, &s2) != 0, "bad rejected");
    remove(path);
    printf("Phase17.3 sandbox script OK (hash=%u)\n", h1);
    return 0;
}
