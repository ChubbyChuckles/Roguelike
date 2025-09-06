#include "../../src/util/formula_eval.h"
#include <assert.h>
#include <string.h>

static void run_case(const char* expr, float expect_min, float expect_max, int expect_ok)
{
    RogueFormulaContext ctx;
    ctx.base_scalar = 2.f;
    ctx.per_rank_scalar = 0.5f;
    ctx.rank = 3.f; /* rank 3 */
    ctx.stat_str = 100.f;
    ctx.stat_dex = 40.f;
    ctx.stat_int = 10.f;
    ctx.stat_cap_pct = 150.f;
    float outv = 0.f;
    char err[64];
    int r = rogue_formula_eval(expr, &ctx, &outv, err, (int) sizeof err);
    if (expect_ok)
    {
        assert(r == 0);
        assert(outv >= expect_min && outv <= expect_max);
    }
    else
    {
        assert(r != 0);
    }
}

int main(void)
{
    run_case("base + per*(rank-1)", 2.9f, 3.1f, 1); /* 2 + 0.5*2 = 3 */
    run_case("(base+per)*rank + str*0.01", 2.0f, 999.f, 1);
    run_case("base + / 2", 0.f, 0.f, 0);  /* syntax error */
    run_case("unknown + 1", 0.f, 0.f, 0); /* ident error */
    return 0;
}
