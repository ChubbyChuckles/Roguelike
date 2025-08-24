#include "../../src/world/world_gen.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Ensure optimizations toggles can be switched without affecting benchmark API safety */
    rogue_worldgen_enable_optimizations(0, 0);
    RogueWorldGenBenchmark b1;
    assert(rogue_worldgen_run_noise_benchmark(64, 64, &b1));
    /* Enable SIMD path (may be no-op if platform not built with SSE2; still must not break) */
    rogue_worldgen_enable_optimizations(1, 0);
    RogueWorldGenBenchmark b2 = (RogueWorldGenBenchmark){0};
    assert(rogue_worldgen_run_noise_benchmark(64, 64, &b2));
    /* Basic invariants */
    assert(b1.samples == 64 * 64);
    assert(b2.samples == 64 * 64);
    /* Determinism: scalar_ms should be > 0; simd_ms may be 0 if SIMD disabled at compile time */
    assert(b1.scalar_ms > 0.0);
    if (b2.simd_ms > 0.0)
    { /* if SIMD measured, expect speedup >= 0.5 (arbitrary sanity bound) */
        assert(b2.speedup >= 0.5);
    }
    printf("phase14 optimization tests passed (scalar %.3f ms simd %.3f ms speedup %.2f)\n",
           b2.scalar_ms, b2.simd_ms, b2.speedup);
    return 0;
}
