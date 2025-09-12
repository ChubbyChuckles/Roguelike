## Debug overlay tips

### Hitbox Rework Phase 1 (Slices 1.1 Progress)

Pixel-perfect collision foundation plus several advanced features are now implemented:

- `pixel_mask_loader` converts sprite alpha to bit-packed collision masks (alpha threshold configurable) with optional async build via a registered thread pool (`rogue_pixel_mask_set_thread_pool`).
- Multi-format sprite support: PNG (via SDL_image when available) and BMP (native SDL fallback). Extension-based dispatch added; TGA/DDS still require SDL_image (magic number probing deferred).
- Multi-format sprite support: PNG (via SDL_image when available) and BMP (native SDL fallback). Magic-number probing now detects PNG, BMP, and DDS even when extensions are misleading; TGA remains SDL_image-backed. Unknown headers fall back gracefully with a log line.
- Signed Distance Field (SDF) generation (chamfer 3x3 inside/outside passes, int16 grid; positive inside, 0 on boundary) gated by `generate_distance_fields` flag.
- Binary OR 2x2 mipmap chain generation (request via `mipmap_levels`, capped at 6) to enable future multi-resolution broad-phase heuristics.
  - Optional smoothed downsampling: when `edge_smoothing_passes > 0`, a separable 1-2-1 smoothing pass is applied prior to each 2x2 binary downsample to reduce stair-stepping at LOD transitions. Default is 0 (off) to preserve previous behavior and determinism.
- Simple word-run RLE compression (opt-in via `compression_level > 0`) stores a secondary buffer; format tag = 1 (future codecs reserved).
- Metrics collected: total / collision pixels, solid ratio, build time (ns), memory footprint, compressed size, mip levels.
- Async path submits a build job to the shared thread pool when present; otherwise falls back synchronously (unit-tested).
- Unit tests: `test_hit_mask_basic`, `test_hit_mask_integration`, `test_hit_mask_distance_field` (SDF + async), and new `test_hit_mask_multiformat` (BMP fallback) keep regression coverage high.

Recent (Milestone 1.2 slice): Added item collision cache invalidation APIs (`rogue_item_collision_cache_invalidate_handle`, `_invalidate_all`), basic asset timestamp capture, and a lightweight RW lock scaffold (write-serialized) ahead of future concurrent read access & hot-reload driven automatic invalidation.

Upcoming roadmap slices: per-row/stripe multi-threaded pixel processing, edge smoothing, advanced alpha handling (gamma), magic-number format detection, compression ratio analytics + auto selection, extended item collision cache (automatic timestamp polling / background loads), and atlas / animation integration (Milestone 1.3).

#### Milestone 1.3 (Initial Sprite Atlas & Animation Collision Slice)

- Added `sprite_atlas_collision.{h,c}` with:
  - Region extraction (`rogue_sprite_atlas_extract_collision_region`) copying a sub-rect of an atlas mask into a standalone frame (origin preserved).
  - Animation set builder with inline small-object optimization (<=8 frames stored without heap) and cumulative timing array.
  - Sampler (`rogue_animation_collision_sample`) returning frames A/B + local interpolation factor.
  - Blended sampling (`rogue_animation_collision_sample_blended`): simple UNION (bitwise OR) between mid‑span frames with fast path direct frame returns near endpoints (<15% or >85%). Scratch blended frame lazily allocated and resized; reused across calls.
  - Debug assert on dimension mismatch to surface silent fallback (future resample slice will normalize dimensions).
- Deterministic pseudo‑fuzz test `test_sprite_atlas_animation_collision_fuzz` stresses blended capacity growth/reuse across frame counts (1..16), exercising inline vs heap storage; only blends frames when sampled pair dimensions match (per-sample gating avoids debug assert on intentional mismatch variants).
- Existing unit test `test_sprite_atlas_animation_collision` covers baseline interpolation union correctness.
- Roadmap updated marking Milestone 1.3 core tasks complete (geometric morphing, resample path, keyframe optimization deferred).

#### Milestone 2.1 (Multi-Resolution Collision Pipeline Slices)

Foundation for a staged, quality-tiered collision pipeline:

- Added `collision_pipeline.{h,c}` defining:
  - `RogueCollisionQuality` enum (FAST / BALANCED / PRECISE / ULTRA) for future adaptive selection.
  - `RogueCollisionStage` (name, time budget, max candidates, function pointer, per-stage metrics).
  - `RogueCollisionPipeline` container (up to 8 stages, quality level, frame budget, adaptive flag placeholder).
  - Metrics per stage: `last_ms`, EMA `avg_ms`, input / output candidate counts, call counter; pipeline total time.
- Execution stub (`rogue_collision_pipeline_execute`) iterates registered stages, records timings, updates EMA, and honors early exit when a stage returns `false`.
- Per-stage caps are now enforced: if a stage declares `max_candidates > 0`, the pipeline caps `ctx->candidate_count` after the stage and records the capped value in `metrics.output_candidates`. The next stage sees the capped count as its input. Unit: `test_collision_pipeline_phase2_1_stage_cap`.
- Deterministic timing support: optional simulated stage cost injection used exclusively in tests (kept <5ms) to produce stable metric assertions without real workload variance.
- Unit test `test_collision_pipeline_phase2_1` validates:
  - Stage registration / ordering semantics.
  - Candidate mutation across stages (e.g. halving logic in a sample stage).
  - Early-exit behavior preventing later stage execution.
  - Metrics population (last / avg ms, call counts, candidate in/out tracking, pipeline total > 0).
- Roadmap updated: Milestone 2.1 initial slice tasks checked (API, execution loop, metrics, test).

##### Adaptive & Spatial Slice (Phase 2.1 – second increment)

Implemented early performance + quality adaptation features:

- High-resolution timing abstraction (Windows QueryPerformanceCounter / portable fallback) replaces coarse timers; per-stage and pipeline total timings now stable at sub-micro precision for EMA + budget logic.
- Spatial Culling Stage: Initial quadtree (fixed arrays, depth cap=4) partitions candidates; collects those overlapping view rectangle or predicted to enter within a short velocity horizon (~16ms) to avoid pop-in.
- Predictive Inclusion: Velocity (vx,vy) on candidates expands effective query region; fast approaching entities are retained even if just outside the current view rect.
- AABB Prefilter Stage: Performs scalar AABB intersection against view rect and trims candidate list to a conservative cap, computing average squared distance to drive LOD heuristics. (SIMD + hierarchical BV tree deferred.)
  Priority ordering added: on‑screen candidates first, then by squared distance to the view center, with a deterministic id tie‑breaker. Cap enforced to 128 survivors before downstream stages.
- Distance-Based LOD Heuristic: Derives a transient quality_delta from mean distance; large average distance nudges pipeline quality downward; close clustering allows potential upward adjustment (bounded).
- Adaptive Quality Control: If cumulative pipeline time >105% of frame budget, degrade one tier (to a floor). If <60% for several frames (aggregate), upgrade one tier (to a cap). Adjustments logged via metrics; no dynamic stage reordering yet.
- Metrics: Existing per-stage metrics extended implicitly by improved timing precision; new adaptive test asserts downgrade then upgrade path deterministically via simulated candidates & tuned horizon.

Deferred (remaining) items: SIMD AABB + hierarchical bounds, temporal coherence cache (previous-frame reuse), pixel-perfect stage integration & multi-resolution mask selection, dynamic stage reordering / load balancing, advanced frustum (beyond rect) & priority-based enemy ordering.

##### Temporal Coherence Slice (Phase 2.1 – third increment)

- Added temporal cache stage executed before spatial culling: when the view rectangle is unchanged (<=1px jitter) and prior candidate subset was small (<=64), the stage validates candidate IDs & limited drift (<=5px) and sets a skip flag allowing the spatial stage to be bypassed for the frame.
- Cache stores a lightweight snapshot (id + x/y) with last view rect metadata; resets heuristically on movement or mismatch.
- Benefits: avoids rebuilding quadtree for stable scenes, shaving micro to sub-millisecond work in dense test scenarios; deterministic unit test ensures bypass activates only under safe conditions.
- New unit test: `test_collision_pipeline_phase2_1_temporal` asserts first-frame spatial execution, second-frame skip, and re-execution after a view move.
  Update: the temporal cache snapshot is refreshed after the AABB prefilter reorders/caps candidates, ensuring the next frame’s validation compares against the exact downstream sequence and avoids false misses due to order changes.
  Additional test: `test_collision_pipeline_phase2_1_temporal_aabb_priority` validates temporal AABB sweep of fast movers and deterministic priority ordering.
- Roadmap updated marking temporal coherence cache done; remaining deferred: pixel-perfect stage, SIMD/hierarchical broad-phase, dynamic reordering, advanced frustum, priority ordering.

Recent refinements (stability + robustness):

- Temporal cache validation is now order-independent (ID set with <=5px drift tolerance); prevents false misses when upstream reorders without changing membership.
- Hierarchical broad-phase expanded view uses a conservative epsilon (+0.5px) to avoid sub-pixel boundary drops; temporal sweep (~16ms) continues to retain fast movers.
- Headless start-screen and overlay guards avoid SDL renderer calls when NULL and suppress overlay during early start-screen frames. This resolved sporadic crashes in headless CI (`test_start_screen_phase10_1_headless`).

- No gameplay paths yet invoke the pipeline; integration & advanced stages intentionally deferred to keep this slice low-risk.

Update (perf): The hierarchical broad-phase now uses a 4-wide SSE2 SIMD path for batched AABB overlap checks with a scalar fallback for portability. Behavior is deterministic and unchanged; tests remain 100% green.

New (determinism + perf):

- AABB prefilter computes sort keys via SSE2 (4-wide) when enabled and reorders candidates stably based on the existing policy (on‑screen → distance → id). Cap remains 128; temporal cache snapshot refresh unchanged.
- Runtime toggle to force scalar or SIMD paths: call `rogue_collision_simd_set_enabled(0|1)`; it gates both AABB key precompute and hbroad batching. Used by new unit tests to verify parity.
- Lightweight deterministic BV prepass (fixed 4×4 scan over the view rect) trims obvious non-overlaps and preserves input order; respects the same 128-candidate cap.
- Test `test_collision_pipeline_simd_equivalence` asserts identical candidate counts and ordering with SIMD off vs on across the prefilter + hbroad stages.

Pixel-perfect stage LOD (update):

- The pixel-perfect refinement stage is now LOD-aware. For BALANCED it samples the center at a selected mip level derived from distance to the view center; for PRECISE/ULTRA it samples a 3x3 region at that level. FAST retains deterministic thinning only. Helpers `rogue_hit_mask_test_level` and `rogue_hit_mask_level_coords` support mip-aware sampling. The selected mip index is clamped to the mask’s available range `[0..mipmap_count-1]` before coordinate mapping to ensure valid sampling and deterministic behavior across assets with fewer levels.
- Unit test `test_collision_pixel_lod` validates that far candidates can be retained via higher mip occupancy even when the base center is empty; near candidates use base-level sampling and can be pruned. The test also covers the mip-level clamping path. Full Debug/Release runs remain 100% green under `-j12` (currently 720/720 tests).

Bitwise mask rectangle utilities (new):

- Added fast bitwise rectangle queries on pixel masks: `rogue_hit_mask_any_set_in_rect` and pairwise intersection `rogue_hit_mask_intersect_any_same_origin` (word-wise with clipping). SIMD paths now include SSE2 and AVX2 with runtime selection. Behavior is deterministic and scalar/SIMD results are identical.
- Runtime SIMD control & caps:
  - Enum mode: OFF, SSE2, AVX2, AUTO via `RogueHitMaskSimdMode`.
  - APIs:
    - `rogue_hit_mask_simd_set_enabled(0|1)` (compat shim: 0=OFF, 1=AUTO)
    - `rogue_hit_mask_simd_set_mode(ROGUE_HITMASK_SIMD_*)`
    - `rogue_hit_mask_simd_get_mode()` (effective mode)
    - `rogue_hit_mask_simd_get_caps()` (bit0=SSE2, bit1=AVX2)
  - AUTO prefers AVX2 when available, else SSE2, else OFF.

Update (AVX2 widening + parity):

- The intersection path has been widened with an AVX2 implementation that batches 8 words (256 bits) per step with row‑boundary safety and precise misalignment handling. SIMD zero‑detect uses `cmpeq`+`movemask` (byte lane) for SSE2/AVX2 parity; no SSE4.1 dependency. Scalar/SSE2/AVX2 results are identical by construction.
- Edge‑safe algorithm details:
  - Computes the true overlap rect across independently clipped regions.
  - First-fragment aligns both streams before ANDing; never crosses word or row boundaries.
  - Middle processes aligned blocks with AVX2 (256‑bit) or SSE2 (128‑bit), falling back to scalar for short spans.
  - Tail processes a masked fragment conservatively.
- Tests: `test_hit_mask_rect_ops` includes scalar/SSE2/AVX2 parity checks and edge/misalignment cases. Full Debug run with `ctest -j12`: 100% green (currently 721/721).
- Unit test `test_hit_mask_rect_ops` covers correctness (clipping, empty regions, disjoint/overlap) and asserts scalar vs SIMD parity.

Collision pipeline SIMD (AABB + Broad-Phase) — AVX2:

- The collision pipeline now includes AVX2 8‑wide batching for two hotspots:
  - AABB prefilter sort‑key precompute (on‑screen flag, squared distance, id, original index)
  - Hierarchical broad‑phase AABB overlap rejection
- SSE2 4‑wide and scalar fallbacks remain available; results are deterministic and identical across modes.
- Runtime gating: `rogue_collision_simd_set_enabled(0|1)` toggles SIMD for both paths; AUTO chooses AVX2 when available.
- Tests: `test_collision_pipeline_simd_equivalence` validates identical candidate counts and ordering across scalar/SSE2/AVX2. Full Debug/Release (`ctest -j12`) remain 100% green (721/721).

##### Phase 4.1 – Temporal Predictor (Advisory, Metrics-Only)

- Added a lightweight temporal coherence predictor (`src/game/spatial_acceleration.h`) and wired it into the collision pipeline as an advisory stage only. The stage `rogue_collision_stage_temporal_advisory` collects metrics by touching pairs against a primary id and emits conservative “skip” predictions without changing candidate lists or gameplay behavior.
- API helpers: `rogue_collision_advisory_reset(float sep_thresh_px)` to reset the internal cache/threshold and `rogue_collision_advisory_get_metrics(uint32_t* predicts, uint32_t* updates)` to read cumulative advisory counters. Enable per-frame via `RogueCollisionContext.advisory_enabled=1` and set `advisory_primary_*` fields.
- Unit test `tests/unit/test_collision_temporal_advisory.c` asserts invariance of `candidate_count` and increasing advisory metrics across successive runs.
- This is groundwork for a future opt-in guarded speed-up; it is currently metrics-only and deterministic.

Update (Honor Mode + Extended Metrics + Determinism Parity):

- Opt-in Honor Mode: `rogue_collision_advisory_set_honor_mode(1)` allows downstream stages to honor conservative skip suggestions emitted by the temporal predictor. Default remains off (0), preserving identical behavior to metrics-only mode unless explicitly enabled.
- Extended Metrics API: `rogue_collision_advisory_get_extended(&hit_rate, skip_hist[4], &min_cands, &max_cands, &avg_cands)` exposes advisory hit rate, a histogram of skip suggestions, and min/max/avg candidate counts per advisory stage invocation. Baseline reset remains `rogue_collision_advisory_reset(sep_thresh_px)` (default threshold used internally is 12.0 px when not set by caller).
- Determinism Parity Test: `tests/unit/test_collision_temporal_advisory_parity.c` verifies that enabling the advisory with honor mode OFF does not change candidate counts or ordering (no behavioral change). Full suite remains 100% green.
- CTest/Debug build stability: see the section below on MSVC multi-config stabilization; all Debug tests now build and run by default.

##### Milestone 2.2 (Initial Weapon Collision Advanced Slice)

- Added `weapon_collision_advanced.h` (header-only scaffolding) implementing:
  - 3x3 affine transform helpers (translate / rotate / scale / multiply) and composition function `rogue_weapon_collision_compute_transform`.
  - `RogueWeaponCollisionState` with velocity, per-weapon quality override field (scaffold), collision layer mask, and a 16-sample time-windowed motion trail buffer.
  - Trail helper `rogue_weapon_collision_trail_aabb` computing coarse sweep bounds for recent motion (future slice will add motion blur multi-sampling & continuous collision tests).
  - Layer mask overlap helper; `RogueCollisionCandidate` extended with `layer_mask` for upcoming pipeline filtering.
- New unit test `test_weapon_collision_trail` validates transform composition, trail pruning (FIFO + time window), sweep AABB correctness, and layer filtering logic.
- Roadmap updated marking core Milestone 2.2 scaffolding complete; deferred: motion blur sampling, bilinear pixel mask transform, non-uniform scaling & pivot customization, advanced continuous sweep algorithm.

##### Milestone 2.3 (Enemy Collision Optimization Scaffolding – In Progress)

- Added `enemy_collision_opt.{h,c}` with `RogueEnemyCollisionProfile` + `RogueEnemyCollisionBatch` structures.
- Implemented `rogue_enemy_collision_profile_analyze_dims` (initial radius + density heuristic → complexity buckets) and batch aggregation (`add` + `finalize` computing centroid & covering radius).
- LOD bias encoding + clamp helpers (range -8..7) added; manual only (dynamic bias logic deferred).
- Unit test `test_enemy_collision_opt` verifies heuristic mapping, bias clamp, centroid aggregation, and radius growth.
- Deferred (next slices): shape descriptors (convexity/symmetry), SIMD `rogue_enemy_collision_batch_process`, dynamic LOD adaptation, performance prediction & method selection.

###### Update: Enemy LOD Stage + Pixel-Perfect Baseline Integrated

An adaptive enemy profile LOD stage has been added to the collision pipeline. It assigns a signed `lod_bias` (higher importance → more negative) derived from approximate on‑screen footprint & distance, enabling future priority ordering and quality shaping. A baseline pixel‑perfect pruning stage is now active (staged behind the same sandboxed pipeline integration):

- FAST: prune every other candidate (cheap coarse thinning)
- BALANCED: single central sample mask check to discard empty/degenerate masks
- PRECISE / ULTRA: 3×3 sample pattern to aggressively filter trivial masks before full bitmask overlap logic (future slice)

Current deferrals for this milestone now focus on: full bitmask overlap & hierarchical/multi‑resolution selection, SIMD mask intersection paths, rich enemy priority ordering (threat/visibility), temporal reuse synergy between LOD + pixel stages, and dynamic stage reordering. The earlier generic "pixel-perfect stage integration" placeholder is replaced by this implemented baseline.

Test: `test_collision_pipeline_phase2_3_enemy_lod` asserts bias ordering across contrasting candidates (size + distance) and guards the negative‑bias heuristic mapping.

Roadmap file `implementation_hitbox_rework.txt` updated (Stage 3 marked complete with annotation).

Recent semantics refinements (Phase 3):

- Weapon transform composition clarified to T\*R (scale excluded from rotation block) for stricter tests.
- Projectile collision now sweeps along the segment between previous and new positions each tick, except on the activation tick (end-point only) to prevent double hits.
- INSTANT layers sample intensity at the start-of-tick for deterministic timing.
- MULTI_HIT layers enforce max_targets per tick; other types continue to use lifetime caps.
- Animation keyframe interpolation bracketing now uses binary search when keyframe timestamps are sorted (linear scan fallback); behavior unchanged, small perf win in hot paths.

Additional refinement (Phase 3.2): blended-frame pooled buffer

- Animation morphing scratch allocation now uses a tiny pooled buffer (4 entries) for blended frames to reduce per-call heap churn. The pool is zeroed on reuse for determinism and falls back to heap if exhausted. Integrated into allocation paths and release.

### SIMD micro-benchmark (perf label)

A small, headless-safe micro-benchmark is available to quantify SIMD speedups in the collision pipeline while preserving determinism and scalar/SIMD parity.

- Test target: `test_collision_pipeline_simd_microbench`
- Label: `perf`
- Measures: combined AABB prefilter sort-key precompute and hierarchical broad-phase overlap checks, comparing scalar vs SIMD (runtime-toggled) under a deterministic synthetic workload. Performs a one-shot parity check and exits 0 (metrics-only).

Run it (Debug, -j12):

```
ctest --test-dir build -C Debug -j 12 -R test_collision_pipeline_simd_microbench --output-on-failure --verbose
```

Example output:

```
[SIMD microbench] candidates=512 loops=200
  scalar: total=25.000 ms  avg=0.1250 ms/loop
  simd  : total=25.000 ms  avg=0.1250 ms/loop
  speedup (scalar/simd): 1.000x
```

Notes:

- The benchmark is deterministic and does not fail the test if performance varies; it prints metrics and exits successfully.
- SIMD paths are gated by the same runtime toggle used in unit tests; parity with the scalar path is verified before timing.
- You can filter all performance-oriented tests with `-L perf`.

Press F1 in-game to open the debug overlay.

##### Milestone 3.1 (Skill Collision Manager Initial Slice – Complete)

- Added `skill_collision_manager.{h,c}` implementing a sandboxed multi-layer skill effect collision system:
  - Up to 4 layers per effect (start_time_ms + duration_ms each) with bitmask filtering, piercing flag, and per-layer max target cap.
  - 8-point intensity curves (uniform 0..1 domain) with linear interpolation; default flat (1.0) when unspecified (all zeros).
  - Deterministic tick function `rogue_skill_collision_effect_tick` producing hit records (target id, layer index, global time, evaluated intensity).
  - Lifecycle helpers: init, add_layer, intensity evaluation, finished predicate; effect completion when all layers elapsed.
  - Unit test `test_skill_collision_manager_phase3_1` validates activation scheduling, intensity ramp, piercing vs non-piercing, max target limiting, and completion flags.
- This slice is headless-safe and not yet integrated with gameplay execution paths (no regression risk). Future milestones will add spatial shapes, projectile trajectories, mask/SDF overlap integration, cooldown gating, and advanced effect stacking.

###### Deepening Slice: Projectile + Frame Interpolation Scaffold (Milestone 3.1 Extension)

- Added projectile support to `RogueSkillCollisionLayer` (position, velocity, radius) with per-tick advancement inside `rogue_skill_collision_effect_tick` when type == PROJECTILE.
- Introduced radius-based projectile hit gating (targets now carry x,y coordinates for distance checks) and a simple projectile test helper.
- Added fractional frame index helper `rogue_skill_collision_layer_frame_index` returning 0..N-1 (float) for prospective animated mask sampling (used by upcoming animation sync milestone).
- New unit test `test_skill_collision_manager_projectile` validates projectile motion (hits expected targets as it traverses) and frame index progression semantics.
- Roadmap annotated with deepening slice; advanced projectile physics (terrain collision, ricochet, SDF-guided pathing, trail sampling, mask interpolation) deferred.

##### Milestone 3.2 (Animation Collision Synchronization – Scaffold)

- New module `animation_collision_sync.{h,c}` introduces:
  - `RogueAnimationCollisionSync`: keyframe timestamps array, optional keyframe mask pointers (NULL in current scaffold), linear interpolation toggle (future spline / quality modes deferred), and frame skip threshold placeholder.
  - Update: Wrap-aware ordering across loop boundaries is now enforced. When a looping timeline wraps, EXIT events from the previous cycle are emitted before ENTER events at the start of the next cycle, preserving determinism. Verified by `test_animation_collision_sync_events_loop_scaled`, which also checks scaled-time parity.
  - `RogueCollisionTimeline`: up to 16 collision windows (timestamp, duration, mask index, intensity multiplier) with optional looping fields (loop evaluation modulo placeholder for later slice).
  - `rogue_animation_collision_evaluate_timeline`: determines active window indices at arbitrary time (ms). Deterministic, stable ordering; overlap simply produces multiple indices (no event batching yet).
  - `rogue_animation_collision_interpolate_masks`: returns bracketing keyframes + linear interpolation factor t (0..1). If interpolation disabled or at/after last keyframe, clamps (b=NULL, t=0).
- Unit test `test_animation_collision_sync_basic` covers timeline activation at sample times, overlap simplification, midpoint interpolation (t≈0.5), end clamp, and disabled interpolation path.
- Roadmap updated marking partial completion (structs + basic eval + linear interpolation + unit test) while deferring spline/intermediate mask morphing, adaptive quality, frame skipping heuristics, speed scaling, and event batching APIs.
- Next deepening steps will integrate skill layer fractional frame index with keyframe-driven mask sampling & morph, then add speed scaling (animation playback rate) and event emission (enter/exit window callbacks) for richer combat timing.

###### Deepening Slice (Phase 3.2 Advanced): Speed Scaling + Frame Skip Cache + Smooth/Quintic Interpolation

- Added `playback_speed` application via `rogue_animation_collision_evaluate_timeline_scaled` (wrapper keeps original API stable). Time domain scaled so higher speed advances collision windows earlier relative to real time.
- Introduced cached evaluation state `RogueAnimationCollisionEvalState` and `rogue_animation_collision_evaluate_timeline_cached` honoring `frame_skip_threshold` (ms). Reuses previous active set when no window boundary (start/end) lies in the interval, avoiding redundant per-frame scans under high frame rates.
- Boundary detection logic safe for looping timelines (wrap normalization) guarantees no missed ENTER/EXIT events before future full event queue integration.
- Implemented lightweight higher-quality interpolation modes:
  - if `interpolation_quality >= 0.5`, linear t is transformed with smoothstep (cubic Hermite)
  - if `interpolation_quality >= 0.9`, use quintic smootherstep (`6t^5 - 15t^4 + 10t^3`) for even smoother easing
- Added advanced unit test `test_animation_collision_sync_advanced` covering: speed scaling activation, cache reuse vs boundary invalidation, smoothstep quality toggle, and linear fallback correctness. New test `test_animation_collision_sync_quality` validates both cubic and quintic easing factors at representative sample points.
- Roadmap updated (Milestone 3.2) marking speed scaling wrapper, frame skip cached evaluation, and basic smooth interpolation mode complete; mask morphing, advanced spline curves, adaptive quality feedback loop, and pooled blended mask buffers remain deferred.

###### Deepening Slice (Phase 3.2 Morph Baseline): Conservative Mask Union

- Added baseline real-time mask morphing via `rogue_animation_collision_morph_mask` (union OR of bracketing keyframes for mid interpolation range). Endpoints (t<=0.15 / t>=0.85) return original frames directly to avoid unnecessary work.
- `RogueAnimationCollisionSync` now owns a reusable scratch blended frame (`blended_scratch`) allocated lazily and resized only when dimensions change (improves cache locality and avoids per-frame malloc churn).
- Conservative union guarantees no false negatives versus either keyframe (slightly over-approximates in-between true geometric interpolation). Future slices will introduce distance-field guided blending or progressive erosion to tighten mid-phase masks plus dimension resampling.
- New unit test `test_animation_collision_sync_morph` verifies union contains both source bits at midpoint, fast-path endpoint returns, and interpolation disable fallback.
- Roadmap updated: marks mask blending baseline complete; advanced spline curves, adaptive quality loop, pooled buffer strategies, and geometric resample blend remain.

###### Deepening Slice (Phase 3.2 Events): Ordered batching + scaled wrapper

- `rogue_animation_collision_timeline_events` now guarantees chronological ordering with ENTER before EXIT when timestamps tie (e.g., one window ending exactly when another begins). A lightweight stable insertion sort is applied to the small event buffer to enforce ordering.
- Added `rogue_animation_collision_timeline_events_scaled` which applies `playback_speed` to the time domain before emitting events, matching the scaled evaluation semantics.
- New unit test `test_animation_collision_sync_events` validates the tie-breaking order at a shared boundary (30ms: ENTER then EXIT) and confirms parity between unscaled and scaled intervals mapping to the same window times.

###### Deepening Slice (Phase 3.2 Overlap Resolution): Deterministic selection strategies

- Added `rogue_animation_collision_resolve_overlap` and `_scaled` to select a single active window when multiple overlap.
- Both strategies are intensity-first with deterministic tie-breakers:
  - Highest-Intensity Latest-Start (then lowest index)
  - Highest-Intensity Earliest-Start (then lowest index)
- The scaled variant applies `playback_speed` for time-domain parity with scaled evaluation.
- Unit `test_animation_collision_sync_overlap` covers strategy choices and scaled-time parity.
- Full suite verification: Debug (SDL2, -j12) 100% green (712/712).

##### Milestone 3.3 (Spell Interaction System – Baseline)

- New `spell_interaction_engine.{h,c}` provides a simple rule-based interaction engine:
  - Spatial proximity via radius (squared distance check)
  - Probabilistic gating with strict rng threshold (rng<probability)
  - Order-insensitive rule matching and outputs: type, result effect id, intensity multiplier, and consume flag
- Unit test `test_spell_interaction_engine` covers proximity true/false, order-insensitive matching, and probability gate behavior (0.0 and 0.5 thresholds).
- Roadmap updated: baseline complete; advanced transformation/composition mechanics, scheduling/conflict resolution, and multi-resolution distance checks deferred.

### Asset Browser (Overlay Phase 1 Enhancement)

- New tabbed interface (All / Textures / Audio / JSON / Shaders) with live wildcard filter ("\*" / "?"; case-insensitive).
- Displays per-session texture/audio peaks, reload count, and approximate texture memory (w*h*4 for loaded textures).
- Manual "Poll Reload Now" plus optional Auto Poll toggle for hot-reload monitoring.
- Recursive enumeration & cached listing of JSON and shader assets (refreshable) to aid rapid inspection.
- Row selection for textures/audio reveals a Details section (id, dimensions, ref count, load state, failure flag) and any registered dependency ids.
- Dependency listing uses existing asset_dep registry; graphical graph + thumbnails deferred to a later phase (see Asset Overlay roadmap).
- Regex filtering and advanced thumbnail caching remain future tasks (current build shows basic inline scaled preview). Implemented: audio playback controls, JSON syntax highlighting + shallow validation (first 12 lines, brace/string balance), sprite sheet grid overlay (adjustable cell size), JSON metadata editor (editable buffer + save/reload & structural validity), and initial sprite coordinate editor (grid-aligned rectangle add/select/delete with overlay outlines and export stub). Upcoming: animation frame editor, loop point editing, regex upgrade, persistent sprite rect export.
- NEW (Asset Overlay Phase 2 slice):

  - Drag-and-drop import: dropping a file onto the game window injects a ::drop::<path> token into the asset browser import field (headless-safe; only active when overlay input enabled).
  - Win32 file dialog: "Open File Dialog" button (Windows only) opens a native picker (filters optional). Selected path is staged for import. Cross-platform (Linux/macOS) support remains on the roadmap (Phase 7).
  - Audio loop points (Phase 3 slice): Selected audio assets now expose a Loop Point editor (Load Loop Pts / Start± / End± / Apply). Loop ranges are stored per asset in-memory (start/end ms) via new asset manager APIs; playback looping with custom ranges will integrate with mixer callbacks in a future phase.
  - Asset tagging (Phase 3 slice): Per-texture & per-audio up to 8 lightweight tags (lowercase) with add/remove UI and a Tag Filter field that ANDs with the wildcard filter.
  - Optimization Recommendations (Phase 4 slice): Toggle "Show Optimization Recs" to list:
    - Large Textures (>=1024px in width or height) – consider downscaling or using compressed variants.
    - Lazy Referenced Textures – assets with non-zero ref count metadata but not yet loaded (candidate preload set).
    - Lazy Referenced Audio – similar heuristic for audio clips.
      Future: integrate file size, compression ratio probes, and atlas/streaming suggestions.
  - Dependency Cycle Visualization (Phase 4 slice): Added placeholder toggle. Runtime registry prevents cycles, so none appear; future enhancement may surface rejected edges log and mini graph export.
  - Phase 5 Initial Slice (Advanced Management):
    - Atlas Tool (baseline): Slider selects atlas index and triggers horizontal build via `rogue_asset_manager_build_atlas_horizontal` (non-destructive; UV/visual preview pending).
    - Memory Profiler (baseline): Shows approximate texture memory (w*h*4) and peak; future will add audio buffers, real GPU queries, per-atlas breakdowns.
    - Performance Metrics (baseline): Cumulative load microseconds + counts (textures/audio) and streaming queue depth via existing metrics; rolling averages/histograms planned.
  - Streaming Queue (enhanced baseline): Toggle streaming, shows pending job list (index, texture index, state loaded/failed/pending, original path) plus manual Step (1/4/All) controls; future upgrades will add age/ETA/progress bars and worker thread metrics.
    - Cache Strategy (placeholder): Scaffold for future eviction policy & compressed variant preference; currently informational.
    - VCS Overlay (placeholder): Reserved area for upcoming git status integration (modified/untracked asset highlighting, diff helpers).
    - File Dialog Root: Native file open dialog now defaults to assets/ directory when present (authorship quality-of-life).
  - Compression Comparison (baseline): "Compression Compare" toggle probes sibling .ktx2/.ktx/.dds variants for the selected texture, listing on-disk sizes & % savings; deeper GPU memory/timing analysis deferred.
  - Asset Comparison (Phase 6 initial slice): For textures, you can now set Compare A and Compare B from the Details section. The panel shows each id, dimensions, delta (A-B width/height), and area ratio (A area / B area). A Clear Comparison button resets both. Visual side-by-side/heatmap previews are deferred until a public scaled draw helper is introduced.

  #### Phase 6 (Workflow Slice 1): Hotkeys & Bookmarks

  - New overlay hotkey chords (require overlay active):
    - Ctrl+Alt+T Atlas Tool
    - Ctrl+Alt+Q Streaming Queue
    - Ctrl+Alt+M Memory Profiler
    - Ctrl+Alt+P Performance Metrics
    - Ctrl+Alt+C Compression Compare
    - Ctrl+Alt+B Add Bookmark (stores current selection)
  - Bookmarks panel section lists up to 16 session bookmarks with click-to-jump. Clear Bookmarks button resets. Persistence + named sets planned (roadmap Phase 6 follow-up).
  - Hotkey Help toggle surfaces an inline legend inside the Asset Browser.

  #### Phase 6 (Workflow Slice 2): Workflow Templates & JSON Undo/Redo

  - Workflow Templates: Toggle a new panel to generate stub JSON template files for rapid prototyping. Buttons currently emit two minimal categories (workflow + skillproc) into assets/ with auto-incremented filenames (`_generated_workflow_#.json`, `_generated_skillproc_#.json`). The panel reports the last generated path and success/failure state. Future slices will allow custom template roots, richer preset content, multi-generate with pattern fields, and schema binding for immediate validation feedback.
  - JSON Editor Undo/Redo: The asset JSON editor now maintains an 8‑entry snapshot ring. A baseline snapshot is captured on open, then subsequent snapshots are pushed on manual Reload and the first edit after a debounce window. Undo/Redo buttons traverse history; creating a new snapshot after undoing truncates forward history. Consecutive duplicate snapshots are skipped. Planned enhancements: timestamp + size metadata, diff preview (lhs/rhs line delta), adjustable depth, idle coalescing, and structured (per‑field) undo.

  - File Dialog Scrollbar (UI polish): The native file dialog's cached listing now renders inside a 12‑row viewport with a vertical scrollbar (mouse wheel + draggable thumb) to prevent large directory enumerations from pushing later UI off‑panel. Pure UI enhancement; no behavioral change to import staging.
  - File Dialog Scrollbar Visibility (refinement): Scrollbar track widened with dark translucent backdrop + border and increased minimum thumb size for contrast/accessibility.

### New Skill Visual Authoring Tools (Phase 2.4 updates)

- Visual Sprite Sheet Editor: In Skills → Visuals, an interactive grid overlay appears when a cast sprite sheet is assigned. It auto-infers a tentative grid (heuristic 64×64 cells) and lets you:
  - Left‑click a cell to select a frame (highlights selection).
  - Right‑drag to adjust grid dimensions (prototype heuristic: horizontal delta → grid_width, vertical delta → grid_height; clamps ≥1). The overlay updates live without mutating the underlying definition until actions are applied.
  - Grow Frame Count: Sets frame_count to (selected_index + 1) if that exceeds the current frame_count.
  - Set Frame Count = Grid Cells: Applies grid_width × grid_height to frame_count explicitly.
  - Auto‑grid inference only runs when no explicit grid metadata is present; manual adjustments persist for the session via the definition fields.
  - All interactions are headless‑safe; rendering paths are SDL‑guarded.
- Asset Dependency Viewer: In Skills → Visuals (below the animation preview), a collapsible section lists tracked asset dependencies filtered to those matching the current cast sheet basename. Useful for spotting related assets (e.g., packed JSON metadata, secondary sprites) and future hot‑reload/validation flows. The list truncates after an internal cap to keep the panel responsive.
- Live Timing Mode (Animation Preview): A new checkbox in the Skills → Visuals preview enables real-time timing adjustment. When enabled, moving the "Preview Speed (%)" slider immediately recalculates and persists `frame_duration_ms` (clamped 1..1000ms) so authors can feel final timing without an extra Apply step. When disabled, the legacy explicit "Apply Speed → Frame Duration" workflow remains for cautious edits.
- Asset Validation Feedback Panel: Provides on-demand thumbnails and status lines for Cast / Projectile / Impact / AoE sprite assets. Flags MISSING (file absent), CORRUPT/UNSUPPORTED (load failure), and DIM ERR (0 or >4096 size). Includes Auto Refresh (per-frame reload while iterating externally) and manual Refresh. Thumbnails render only when SDL is available; logic remains headless-safe.

### Recent Skills/Effects Additions (Phase 1.2)

Advanced Effect Composition (Phase 2.2 update): Experimental Effect Tree editor upgraded with an Advanced Node Graph: zoom (mouse wheel), pan (RMB drag), multi-select (Ctrl+Click), group drag with snapshot, add child, delete selected leaves, link reversal, and automatic delay derivation from hierarchical spans (Complex Effect Dependencies). These features are SDL‑guarded (headless safe) and persist existing layout data; new zoom/pan state is transient for now.

- SPAWN_ENTITY EffectSpec kind: spawns 1..8 transient entities (default 1) with lifetime (default 5000ms) using a fixed 64-slot pool; auto-expire; authoring fields `spawn_entity_count`, `spawn_entity_life_ms`.
- Experimental EffectNode Tree Structure: optional hierarchical composition (`effect_tree_nodes[8]`) on `RogueSkillDef` with parent indices (-1 root). When `effect_tree_node_count>0`, runtime scheduling derives node start times relative to parents; legacy flat `effect_nodes[3]` retained for back-compat and still used in existing overrides/content. Debug API + overlay UI now support live editing (toggle between flat and tree modes) and JSON overrides export/import includes an `effect_tree` block. Validation enforces bounds, cycle detection, SPAWN_ENTITY authoring constraints, non-negative delays, repeat interval requirements, window coverage (duration >= repeat_count\*interval), and a conservative total scheduled span cap (<=60s) to catch runaway chains.

**Advanced State Machine & Queued Activation (Phase 1.3 slice)**: Added `rogue_skill_request(id, ctx)` which attempts immediate activation and, on failure due to an in-progress cast or cooldown (with input buffering), marks the skill `queued_active` and schedules a deferred activation inside the skill's `input_buffer_ms` window. New execution states: `ROGUE_SKEXEC_QUEUED` (pending) and scaffold for `ROGUE_SKEXEC_GLOBAL_LOCKOUT`. Queued activations auto-fire right after a casting skill completes or when cooldown elapses, then clear the queue flag—no behavior change for instant skills lacking buffering. Unit: `test_skills_phase1_3_advanced_state_machine`.
**PNG Sequence Loader (Phase 1.4)**: Added `rogue_skill_load_png_sequence(directory, prefix, out_frames, max_out)` scanning for `prefix_001.png` style (fallback to unpadded) to build per-frame sprites. Currently loads each PNG as its own texture (atlas & ownership consolidation TODO). Smoke test `test_skills_phase1_4_png_sequence_loader` covers negative path.
**Sprite Atlas Generation (Phase 1.4)**: Added `rogue_skill_pack_frames_horizontal(frames, count, out_atlas)` which consolidates a sequence of independently loaded frame textures into a single horizontal atlas texture, remapping frame regions in-place and freeing old textures. Helper `rogue_skill_free_sequence_frames` releases un-packed sequence frame textures. Negative-path test `test_skills_phase1_4_sprite_atlas_pack` validates deterministic no-op when no assets present (headless safe). Future: packing heuristics & vertical/area bin-pack.
**Asset Dependency Tracking & Hot-Reload Foundation (Phase 1.4)**: Added fixed-cap (256) dependency registry with ref counting + mtime snapshotting. APIs: `rogue_skill_asset_dep_track`, `..._untrack`, `..._reset`, `..._count`, `..._data`, `..._poll_changes(cb,user)`. Polling detects modified source assets (PNG/JSON) and notifies caller for manual texture/preview reload (no automatic texture recreation yet). Unit: `test_skills_phase1_4_asset_dep_tracking`.

**Performance Optimization Slice (Phase 6 – partial)**: Introduced early performance features for the asset manager:

- Preloading APIs: `rogue_asset_manager_preload_texture` + batch `rogue_asset_manager_preload_textures` to warm caches during startup sequences.
- Lazy Loading Toggle: `rogue_asset_manager_set_lazy_loading(bool)` defers SDL texture/audio creation until `rogue_asset_manager_ensure_texture_loaded(index)` is invoked (or implicit on first non-lazy acquire when toggle is off).
- Metrics Collection: `rogue_asset_manager_get_metrics` / `rogue_asset_manager_reset_metrics` expose cumulative microseconds + load counts (textures & audio) measured with SDL performance counters when available (headless-safe: counts still increment; timings zero when SDL absent).
- Streaming Loader (incremental): `rogue_asset_manager_set_streaming_enabled`, `rogue_asset_manager_enqueue_texture_stream`, and per-frame `rogue_asset_manager_stream_step(max)` spread texture decode across frames (no worker thread yet; future upgrade path). Metrics expose queue depth and streamed load count.
- Atlas Tooling: `rogue_asset_manager_build_atlas_horizontal(indices,count,uvs,cap)` builds a horizontal atlas texture and returns per-sprite normalized UVs (headless-safe no-op when SDL unavailable). Source textures remain valid (non-destructive).
- Platform Optimization Hooks: `rogue_asset_manager_set_prefer_compressed_textures(1)` probes for sibling `.ktx2`, `.ktx`, or `.dds` variants (in that order) and substitutes when present, falling back gracefully when absent.
- Debug Overlay Panel: New "Asset Metrics" panel shows texture/audio load counts, cumulative timings, streaming queue depth, streamed load tally, and toggles (Streaming Enabled, Prefer Compressed Variants, Reset Metrics).
- Roadmap alignment: Phase 6 performance tasks (preloading, lazy loading, profiling, streaming, atlas tooling, platform optimization) now implemented; future slices may add threaded streaming & eviction.
  **Developer Tools & Utilities (Phase 7 – initial)**:
- Extended usage analytics (peaks, reload counters, last reload tick) via `RogueAssetUsageStats`.
- Hot-reload integration now records reload events inside `rogue_asset_manager_poll_reload` (incremental file mtime detection) enabling future dashboards.
- Asset Browser (stub) panel: textual listing of first N texture/audio entries with filter substring (integration wiring pending full overlay registration UI pass).
- Atlas generation script scaffold: `tools/atlas_generate.py` emits placeholder horizontal atlas manifest (even-slice UVs) for future real packing; supports early pipeline experimentation.
- New unit test `test_asset_phase7_usage_stats` validates extended stats and reload note hook.
- Upcoming (not yet implemented): dedicated `asset_tool` multi-command CLI (list/stats/diff/reload), inspection panel with dependency graph & checksum delta, real atlas packing (dimension probe + bin-pack), and integrated asset issue surfacing in the Asset Browser.
- Phase 7 Completion Update: Added `asset_tool` (stats, list, checksum-snapshot, checksum-verify, diff, inspect) and integrated Asset Browser overlay panel (live filter, usage peaks, reload counters). Inspection via `asset_tool inspect --id <substr>` prints per-texture CRC + dependencies. Future enhancement will add graphical dependency graph + checksum delta visualization.
  **Enhanced Execution Pathways (Phase 1.3)**: Introduced optional runtime-selectable pathways for showcase skill (Fireball) via a dispatch wrapper (default / empowered / utility mini-dash). APIs: `rogue_skill_pathway_set/get/last_exec`. Pathway 0 preserves legacy behavior; other pathways add deterministic side-effects without altering timing or state machine semantics. Unit: `test_skills_phase1_3_execution_pathways`.
  **Enhanced Data Model Tests (Phase 1.5)**: Added `test_skills_phase1_5_enhanced_schema` validating extended schema fields (visual/audio/AoE/projectile & effect node timing) plus negative cases (max_rank, frame_count, pierce_count, aoe_shape, repeat_count without interval). Roadmap item 1.5 now marked complete.

- Panels selector: A small "Panels" window appears in the top-right. Use its checkboxes to toggle which panels are visible. The selector itself can’t be hidden to avoid lock-out.
- Shortcuts panel & quick switching: Press '?' to open a concise "Shortcuts" panel listing core key bindings at a glance. Hold Alt and press 1..9 to open common panels quickly: 1 System, 2 Items, 3 Skills, 4 Map, 5 Audio/VFX, 6 Entities, 7 Content Graph, 8 Validation, 9 Dialogue. Esc clears focus. Ctrl+S triggers contextual saves (Skills overrides, Map JSON). Ctrl+Z / Ctrl+Y map to Undo/Redo in the Map editor.
- Command Palette: Press Ctrl+Shift+P to open a palette of overlay commands. Type to filter; Enter to run. Defaults include Validation (Run Now/Show Panel), Skills (Save/Load Overrides), Content Graph (Export DOT), and quick “Open Items/Skills/Map”. You can register your own commands at startup.
- Global Search: Press Ctrl+K to search across Items and Skills by id or name (case-insensitive substring). Up/Down navigate results; Enter jumps to the right panel, opens it, selects the entry, and scrolls it into view. Initial scope covers Items/Skills; more registries and fuzzy ranking are planned.
- Navigation history & breadcrumbs: Use Alt+Left/Right to move Back/Forward through your overlay navigation history (panel + selection). Supporting panels render a breadcrumb header (e.g., Items > Weapons > iron_sabre) that updates as you change selection. Global Search jumps are tracked in history.
- Toast notifications: Non‑modal toasts appear in the overlay for common actions (info/warn/error) and auto‑dismiss. You’ll see them when saving/loading Skills overrides, when Auto‑Reload applies changes, when Validation starts/completes, and when running actions from the Command Palette. They’re headless‑safe and won’t interfere with input.
- Theme & Accessibility: In the System panel, adjust Theme Preset (Dark, Light, High Contrast), DPI scale, and Font Size. Your choices persist to `build/overlay_theme.json`. Core widgets have been migrated to use theme colors for consistent visuals and better contrast. Map and Audio/VFX panels now consume theme accents for their world-space gizmos (brush ghost, colliders, path heatmap, autotile hints, falloff ring). Entities panel adds a themed world‑space selection gizmo (rect + crosshair) around the selected entity using theme accents; toggle it in the Entities panel options. New: a "Colorblind-safe palette" checkbox remaps accent colors and increases toast contrast; it persists to `overlay_theme.json`.
  Micro‑interactions (M6.1): Buttons, checkboxes, sliders, input fields, combos, and tree nodes now show hover/pressed feedback and a clear keyboard focus ring using theme accents. These improve discoverability and accessibility and respect the Colorblind-safe palette.
  Tooltips (M6.1): Attach a tooltip to the next widget call with `overlay_set_next_tooltip("Text...")`. On ~400ms hover the tooltip appears near the cursor with themed styling. Headless-safe; all widgets consume this automatically.
  Icons (M6.2): A small icon set is now available for common actions. Use `overlay_icon_button("Save", OVERLAY_ICON_SAVE)` or `overlay_icon_button("Undo", OVERLAY_ICON_UNDO)` to render a labeled button with a left-aligned icon. Icons are monochrome bitmaps tinted by the theme accent and are headless-safe. Adoption now spans Map (Undo/Redo, Save/Load), Skills (Save/Load Overrides), Audio/VFX (Play/Spawn @ Cursor), Dialogue (Start/Advance, Reset), Validation (Validate All), and Content Graph (Compute All Hashes, Export DOT/JSON, Fit to Selection, Clear Pins, List Orphans/Hubs, Export Subgraph DOT/JSON, Back).
- Items panel: Type in the search box to filter by id or name (case-insensitive). Click table headers to sort (toggles asc/desc). The list is virtualized for large registries; scroll with the mouse wheel while hovering the table or use Up/Down keys. Home/End jump to top/bottom. Hold Shift to accelerate step.
  Now includes a right-side vertical scrollbar: click the track to page and drag the thumb to scroll; it stays in sync with the virtualized row window.
  Default row density is tuned for performance: row height 16px with 1px padding (adjustable via the Row Height slider). Perf smoke tests on 50k rows showed a small but consistent frame-time improvement over the previous 18px/2px default.
  Templates & Presets: In the Create New Item wizard, use curated Presets to prefill common item shapes, or save your own Templates and reapply later. "Save as Template" writes a JSON fragment to `build/templates/items/<name>.json` (atomic). "Load Template" applies fields back to the wizard. Toasts confirm success or report errors; all paths are headless-safe.
  Batch Create (Items): Pattern expansion with preview and validation. Provide Id Format and Name Format using printf-style specifiers (e.g., "iron*sword*%03d", "Iron Sword %d"), plus Start and Count. The preview flags invalid formats and duplicate ids. Apply creates items via the current Create wizard fields; a toast summarizes successes/failures.
  Items Preview (M4.1): The Create wizard shows a live textual preview under "Preview" summarizing Category, Rarity, Level, Base Stats (Damage dmin–dmax or Armor), Sockets (min..max), and an informational Est. Value heuristic. Updates immediately as you edit; headless-safe.
  Tooltips (M6.1): Attach a tooltip to the next widget call with `overlay_set_next_tooltip("Text...")`. On ~400ms hover the tooltip appears near the cursor with themed styling. Headless-safe; all widgets consume this automatically.
- Skills Effects Node Graph: Canvas, node fills/borders, edge lines, and labels are now driven by the overlay theme for consistent contrast across presets (Dark/Light/High-Contrast). Linking and node selection visuals adapt to accent colors.
- Content Graph: SDL preview and diagnostics visuals consume overlay theme colors, including accents and text tones, improving readability and consistency.
  Controls (M5.1): In the mini‑view, Right‑mouse drag pans, the mouse wheel zooms (cursor‑centered; 0.5×–2.0× clamp), and F fits the selection. A toggle "Isolate subgraph (limit list)" filters the list to nodes reachable from the current selection up to the current Preview depth. Click a node in the mini‑view to select/drill and update the crumb trail; a small hint inside the view reminds: "RMB drag=pan, Wheel=zoom, F=fit". Headless‑safe: drawing is guarded and won’t run in tests.
  Tree Node control: Collapsible sections are available as a first‑class widget and used in Map, Validation, and Content Graph panels. Toggle with mouse click or Enter/Space; focus ring appears when navigated via keyboard.
  Finders (M5.3): In addition to Orphans and Hubs (top 10), a new “List Missing Providers” finder scans dependencies and lists unresolved references as “missing <dep> (referenced by <id>)”. Buttons are icon+label for consistency and remain headless-safe.
  Layout & highlighting (M5.2):
  - Group halos: Nodes that share the selected root’s top-level group (prefix before the first '/') are softly tinted in the SDL mini‑view for quick visual context.
  - Explain Path: Enter a target id to compute and highlight a path from the current root to that target within the current Preview Depth. On success, a breadcrumb shows the chain and nodes/edges along the path are highlighted using accent colors. The last requested target is remembered so you can continue exploring with the highlight active.
  - Depth cues: The outermost preview layer has a subtle border emphasis to convey depth at a glance.
    Snapshots & Diff (M5.4):
  - Capture: Use “Capture A” and “Capture B” to snapshot the focused subgraph (root + BFS up to the Preview Depth).
  - Diff: Click “Compute Diff A→B” to compute Added/Removed edge sets and per‑node degree deltas; enable “Show Diff Overlay” to render diffs in the mini‑view (Added = accent_1, Removed = error red). Explain Path highlights remain in text_accent.
  - Export: “Export Diff JSON” writes `build/content_subgraph_diff.json` with nodes, added/removed edges, degree deltas, and a cycles list for the focused subgraph.
  - Notes: All visuals are SDL‑guarded for headless runs; buffers use compile‑time caps for MSVC C mode compliance.
- Map panel (M4.4): Live previews for authoring. A brush ghost shows your current footprint (square/rect). Optional overlays visualize colliders and a pathfinding heatmap. Autotiling hints highlight neighbors the placement would affect. A local 64‑step Undo/Redo ring protects edits (Ctrl+Z / Ctrl+Y) with a Clear History button. All visuals are SDL‑guarded with headless‑safe textual fallbacks.
- Dialogue panel (M4.5): Textual live preview of the current line and a mini branch graph (ASCII list with a current marker). Includes a simple conversation runner (Start/Advance/Reset) and an optional style inspector that applies via debug APIs. Auto-registers a sample script when none are present so you can try it immediately. Headless‑safe; SDL‑guarded where applicable.
- Entity inspect: In the Entities panel, hold Shift and LeftClick on a unit in the world to select and inspect it. This uses the camera and tile mapping to pick the nearest enemy under the cursor.
  Duplicate & batch tools: With an enemy selected, use "Duplicate Enemy" to spawn a copy with configurable dx/dy offset (headless‑safe). The "Batch Creator (spawn copies)" can place a ring of N duplicates around the player at a chosen radius (8‑directional pattern, repeats as needed). Useful for quick combat/AI triage.
- Skills: The Skills panel now uses top-level tabs: Overview, Effects, Visuals, Audio, and Testing. The Overview tab includes the Create New Skill wizard with a template workflow. Select a template skill id and click "Apply Template" to prefill name/max-rank/passive/timing; optionally enable "Copy Coeffs" to duplicate coefficient params to the preview and to the created skill. New: a searchable template picker (filter + table) lets you quickly find a template by substring and select it by clicking the row.
  Duplicate from selected: Click "Duplicate From Selected" to open the wizard prefilled from the current skill. The new name gets a "\_Copy" suffix; max rank, passive flag, and timing fields are carried over. Enable "Copy Coeffs" to also clone coefficient parameters.
  Batch Create (Skills): Pattern expansion with preview and duplicate detection. Provide a Name Format (printf-style), Start, and Count, and optionally set Passive/Max Rank/timing fields. The preview lists generated names and flags duplicates. Apply creates the batch and reports a concise summary.
  Testing positions: In the Testing tab, adjust Cast Pos and Target Pos using a new Vector2 control (X/Y). When you run a simulation, the profile JSON includes optional keys cast_pos: [x, y] and target_pos: [x, y]. Both are optional; when omitted, defaults apply and existing profiles remain valid.
  Visuals helpers: In the Visuals tab, use the Sprite Sheet Browser to quickly assign sprite paths for Cast/Projectile/Impact/AoE. Pick the Assign-To target, type to filter, then click a file under assets/ to assign and persist via overrides. A Grid Configurator helper shows grid_width × grid_height and offers a one-click "Set Frame Count = Grid Cells" action; it only changes frame_count when you click it. Note: current browser enumerator targets Windows; cross-platform support is planned.
  Asset File Picker: A unified picker (Images/Audio/All) with substring filter recursively scans assets/ (Windows enumerator) and assigns selected image files directly to the current Assign-To target (Cast/Projectile/Impact/AoE). Audio rows (when category=Audio) are listed for future assignment flows but only image selections currently auto-assign. Headless-safe.
  Range Sliders: New overlay_range_slider_int/float widgets provide ordered min/max authoring (Phase 2.4). Initial adoption in Skills → Overview adds non-persistent design helpers: "Cooldown Target Band" (float ms range) and "Design Rank Window" (rank span). Future persistence wiring (e.g., damage min/max bands, projectile range gates) will extend usage; current implementation is headless-safe and reuses existing slider styling.
  Curve Editor: Introduced overlay_curve_editor (Phase 2.3 item) as a lightweight polyline widget (click to add, drag to move, Add Mid/Remove buttons) used in Skills → Overview to sketch a Damage Scalar Curve (rank→scalar). Prototype only—values not yet persisted; establishes groundwork for future saved progression curves (cooldown reduction, coefficient ramps).
  Gradient Editor: Added overlay_gradient_editor prototype (Phase 2.3) enabling authoring of color gradients for future particle/theme ramps. Supports adding stops by clicking the bar, selecting/dragging (endpoints locked at 0 and 1), removing interior stops, and editing RGBA per stop (reuses overlay_color_edit_rgba). Example "Trail Color Gradient" shown in Skills → Overview. Non-persistent; slated for particle system + persisted JSON integration.
  Timeline Editor: New overlay_timeline_editor prototype (Phase 2.3) for sketching multi‑phase skill execution windows as blocks (add via empty click, drag to move, drag edges to resize, Delete to remove). Non‑persistent design helper currently in Skills → Overview; future iterations will add persistence, overlap constraints, type editing, and export tooling.
  Asset Import Wizard: Enumerates common sprite sheet image formats (PNG/TGA/BMP) recursively under assets/ (Windows enumerator). Provides Filter box, Assign Target combo (Cast/Projectile/Impact/AoE), staging list (up to 64 entries), and Import → Assign which applies the last staged file per target to the current skill’s visual params. Current scope updates only sprite path fields (no copy/move). Future: drag & drop (SDL_DROPFILE), structured copy into assets/skills/<id>/ subdirs, thumbnails, validation (dimension/grid hints), audio import, cross‑platform FS abstraction.
  Multi-select & Dynamic Lists (Phase 2.4 prototypes): Added `overlay_multiselect_bits` (bitmask multi-select) and `overlay_list_editor` (add/reorder/delete string list). Sample integrations in Skills → Visuals: "Status Tags" (non-persistent effect flag mask) and "Combo Chain" editor (authoring scaffold). Persistence & schema binding planned; current implementations are headless-safe UI scaffolds.
  Formula Preview (Phase 2.4 prototype): A lightweight expression evaluator (`formula_eval.c`) powers a new "Formula Preview" section in Skills → Overview. Enter expressions using +,-,\*,/, parentheses, and identifiers (base, per, rank, str, dex, int, cap) to experiment with coefficient scaling. Live rank/stat sliders update the computed value in real time. Prototype only – no persistence or syntax highlighting yet.
  Audio helpers: In the Audio tab, use the Sound ID Browser to pick from `assets/sounds.cfg` in a filterable table. Choose Assign-To (Cast/Impact/Loop), type to filter by id/path, and click a row to assign the id to the selected target. Changes mark the skill dirty and persist via overrides. Works headlessly; no SDL required.

> CI & Releases: Artifacts uploaded from CI include the computed semantic version and short SHA in their artifact name. Packaged archives created by CPack also embed the semver and platform in the filename for easy identification.
> Verified locally: CPack generated archives like "roguelike-0.0.0+dev+gf45026d-windows.zip" with matching .sha256 checksum files.

Vendor-only SDL2 policy (Windows):

- Windows builds and tests link and run exclusively against the vendored SDL2 in `third_party/SDL2` (including SDL2_image/mixer DLLs staged for runtime). CMake prefers this root automatically and will fail the configure if SDL2 cannot be found under the provided/manual root. No vcpkg/system SDL fallbacks remain in build or CI.
- Linux/macOS continue to use system packages (pkg-config/Homebrew) with identical semantics. All CI jobs build and test with parallelism (-j12) and set dummy SDL drivers for headless safety.

CTest/Debug build stabilization (Windows/MSVC multi-config):

- Pre-created per-config test runtime directories under `build/tests/<Config>/` to normalize PATH composition for SDL runtimes and test binaries.
- Added an aggregation target that depends on all unit test executables so they are built by default as part of the ALL target. This eliminates “Not Run” cases where tests hadn’t yet been built in Debug.
- Result: Full Debug suite executes reliably with parallelism (`-j12`) and is all green (719/719) locally; Release remains green as well.

Headless start screen corruption-scan guard (Release parity):

- In headless mode, `rogue_start_screen_maybe_scan_corruption` now marks the scan as complete and sets the conservative corruption-at-start flag, then returns early. This avoids parallel file I/O and descriptor parsing during test runs and eliminates a Release-only flake observed under `ctest -j12`.
- Validation: Full Release suite on Windows (vendored SDL2) passes 100% (719/719) with parallelism; previously flaky tests `test_start_screen_phase10_4_reduced_motion` and `test_dungeon_phase8_3_upgrade_guarantee` are stable when run as part of the full set.

### Worldgen stability (Phase 8.3 upgrade guarantee) – Release validation

- Hardened the dungeon chest placement path to guarantee the upgrade marker (overlay code 14) is placed adjacent to the deepest-room tier chest (10..13) when the upgrade override is forced.
- Guards added: strict tile buffer and room-bounds validation (clamping), non-empty scan-region checks, safe overlay reads/writes, and a fallback whole-map scan when the deepest-room region is invalid or empty. Smart-drop planning is now gated by the override to avoid unintended behavior when upgrades are forced.
- Test `test_dungeon_phase8_3_upgrade_guarantee` passes deterministically; repeated 10× runs are stable. Full Release suite verified green under -j12 (716/716).
- Changes are headless-safe and do not alter gameplay behavior outside the guarded/tested upgrade path.

- Skills validation: Saving Overrides JSON now runs validation first and blocks the save on errors (a message explains what to fix). Creating a new skill will run validation and show a warning if the definition is invalid (creation still proceeds so you can iterate). A headless-safe API `rogue_skill_debug_validate(err, cap)` is available for tools/tests. New: a sticky "Validation Status" banner at the top of the Skills panel shows OK or "ERROR: <reason>" and refreshes live on edits (timing/coeffs/visuals) and on Create/Save/Load. Also new: inline validation messages inside the Effects tab for primary/node EffectSpec IDs and timing/HP gate fields; errors display next to the inputs as you type.
- Skills Visuals / Advanced: Edit optional fields live (sprites: cast/projectile/impact/aoe; animation: frame_count/frame_duration_ms/loops/grid; audio: cast/impact/loop + volume/pitch variance; AoE: shape/radius/angle; projectile: velocity/trajectory/pierce/homing). Changes apply via headless-safe setters and persist to overrides JSON. Validation checks asset existence and parameter bounds and surfaces issues on save/create.
  Visuals UX: Discrete numeric controls are being converted to labeled enum dropdowns to reduce errors. Current conversions:
  - AoE Shape → combo with: None, Circle, Cone, Line
  - Trajectory Type → combo with: Linear, Arc, Homing, Scatter
- Optional: `skill_type` enum can be set on skills (MELEE, RANGED, AOE_SPELL, BUFF, DEBUFF, HEAL, SUMMON, PASSIVE, ULTIMATE). If omitted, it defaults to UNKNOWN for back-compat.
- Skills Meta UI: The Skill Type control is a labeled enum combo (not a numeric slider), listing: UNKNOWN, MELEE, RANGED, AOE_SPELL, BUFF, DEBUFF, HEAL, SUMMON, PASSIVE, ULTIMATE. Changes persist via overrides and refresh validation immediately.
- New: Skills Meta section includes a Skill Type selector with human-readable labels and "Type Presets" buttons that apply safe baseline timing/coeff defaults per type. The property panels are context-sensitive (e.g., AoE-only fields for AOE_SPELL, projectile-only fields for RANGED). Simulation controls moved to the Testing tab.
- Real-time preview (Testing tab): Toggle "Enable Real-time Preview" to see the selected skill’s visuals rendered with sprite-sheet animation; use "Auto-animate" and "Zoom" to control playback and scale. The preview selects cast/projectile/impact/AoE sprites based on `skill_type`, caches textures, and is headless-safe (no SDL calls when no renderer is present).
  Playback controls:
  - Play/Pause: toggles animation (mapped to Auto-animate).
  - Loop: preview-only override of the definition’s loop flag.
  - Step: advances exactly one frame when paused.
  - Reset: returns animation time/frame to 0.
  - Frame slider: visible when paused for direct frame scrubbing.
  - Preview target: a dropdown lets you preview Cast, Projectile, Impact, or AoE sprites on demand (independent of `skill_type`). Cast preview uses grid-sliced frames; other targets display the full texture unless grid metadata is provided.
    Timing respects `frame_duration_ms` and total frames from `frame_count` or grid_width × grid_height; preview controls do not modify runtime data.
    Skills Live Preview (M4.2, textual): Above the simulation controls, a headless-safe table summarizes the selected skill: name/type, Cast and Cooldown timing, Coeffs (base/per-rank), Primary Effect kind/magnitude and Effect Nodes (with delays/repeats), plus a rough estimated total tick count computed from repeats/periods. Complements the sprite preview; updates as you edit.
    Audio/VFX (M4.3): In the Audio/VFX panel, audition sounds and inspect attenuation deterministically.
  - Packed atlas sprite support (new): In addition to grid slicing, the preview (and any future tooling) can build frames from a small packed-atlas JSON using a purpose-built lightweight scanner (no external full JSON dependency). Two shapes are accepted and silently ignore malformed entries:
    1. Array form: `{ "frames": [ { "x":0, "y":0, "w":32, "h":32 }, { "x":32, "y":0, "w":32, "h":32 } ] }`
    2. Object form (TexturePacker-style subset): `{ "frames": { "idle_0": { "frame": { "x":0,"y":0,"w":32,"h":32 } }, "idle_1": { "frame": { "x":32,"y":0,"w":32,"h":32 } } } }`
       Only `x,y,w,h` are read; rotation/trim/pivot metadata (if present) is ignored. The loader assigns the shared texture pointer to each `RogueSprite` and caps at the caller-provided buffer size. Invalid or out-of-bounds rectangles are skipped. Unit test: `test_skills_phase1_7_packed_sprite_loader` covers both JSON shapes. This keeps sprite authoring flexible (tight packing) without forcing a full JSON DOM layer.
  - Toggles: Enable Positional Audio, Listener follows player; adjust Falloff Radius (tiles).
  - Play Sound @ Cursor: Emits through the FX bus at the cursor’s world position using camera/tile mapping.
  - Attenuation Preview: Headless‑safe ASCII bar samples effective gain at key radius fractions (0/25/50/75/100%).
  - Optional Gizmo: Falloff ring visualization near listener/player (rendered only when an SDL renderer is available).
- Effects overrides import: The Skills overrides loader now applies the primary `effect_spec_id` and any `effect_nodes[]` atomically after parsing, regardless of JSON key order. This prevents partial application and matches the Effects tab editor’s export format.
- Effects tab – EffectSpec palette: Toggle a mini palette to browse all registered EffectSpecs with filters (substring, Kind, Debuff-only, and Category toggles: Offensive/Defensive/Movement/Utility). Select an entry and choose Assign-To (Primary or Node 0..2), then click Assign Selected to update the composition. Use "Clear Nodes" to reset node assignments quickly. Changes mark the composition dirty and persist via overrides. Powered by a new safe enumeration helper (`rogue_effect_count`).
- Effects tab – Node Graph: Toggle the Node Graph to arrange up to a few nodes visually; drag to move, click to select, and edit parameters (EffectSpec ID, delay/duration/repeats/interval, HP gate) in the side panel. New: explicit connections.
  - Linking workflow: select a source node, click "Start Link", then click a destination node to connect; use "Cancel Link" to abort. Cycles are prevented.
  - Unlink/Clear: "Unlink Selected" removes the parent link; "Clear Selected Node" resets its EffectSpec and timings.
  - Apply timing: "Apply Connections to Delays" derives each node's delay_ms by summing parent spans (duration or repeat_count\*repeat_interval_ms). "Chain Nodes" remains as a quick X‑order auto‑sequence.
  - Visuals: edges render between connected nodes; nodes tint by validation status (red = invalid id, orange = timing/param issues, green = OK). Rendering is headless‑safe.
- Effects tab – Authoring help: A small "Repeat Mode" helper (0 none / 1 count / 2 window) maps inputs to repeat_count, repeat_interval_ms, and duration_ms safely. Contextual suggestions appear when the selected EffectSpec defines timing: "Apply spec duration (ms)" and "Use spec pulse period (ms)" (optionally deriving repeat_count). Extra one‑click fixes address common interval/duration inconsistencies.
- Effects tab – Presets & Batch helpers: Quick Node Presets (Instant, Periodic Window, Counted Pulses), HP gate quick-set buttons (0/25/50%), and Batch Helpers (Fill empty nodes with Primary, Normalize window durations to whole pulses, Clear all HP gates) to speed up common authoring flows; UI-only and headless-safe.

Notes

- Panels are movable and their layout (x, y, width, visibility) persists across runs. The layout file is `build/overlay_layout.json`. To reset, delete that file. Tip: drag any panel by its title bar; height clamps to the viewport. Long tables auto-cap visible rows to avoid overflow.
- If a click doesn’t select, make sure the cursor is over the world (not another UI), and keep Shift held while clicking.

## Vendor UI updates (Phase 16)

- Tabs: Buy / Sell / Buyback / Special with Left/Right navigation.
- Buy tab: pricing tooltip shows value→margin→rep→negotiation→demand chain.
- Negotiation preview: shows min..max discount and success probability; respects lockout timers (security guard) and displays remaining lockout time.
- Reputation strip: a small progress bar with current tier label and a perks hint (based on tier unlock tags).
- Accessibility (16.6):
  - High-contrast deltas: When High Contrast is enabled (System panel), BUY list lines append explicit price deltas (e.g., +10% or -8%) for quicker scanning of value changes.
  - Text-only mode: A new Vendor UI text-only toggle forces plain text rendering for item lines, including previous price and percent delta suffixes for readability in low-graphics contexts. Backed by a reusable formatter helper.
  - Tests: `tests/unit/test_vendor_phase16_6_accessibility.c` verifies formatter outputs across normal, high-contrast, and text-only modes.

<div align="center">

# Roguelike (Top‑Down Zelda‑like) – C / SDL2

[![Build Status](https://github.com/ChubbyChuckles/Roguelike/actions/workflows/ci.yml/badge.svg)](../../actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Standard: C11](https://img.shields.io/badge/standard-C11-blue.svg)]()

![alt text](https://github.com/ChubbyChuckles/Roguelike/blob/main/assets/vfx/start_bg.jpg?raw=true)

Clean, **modular**, and **test‑driven** 2D action roguelike foundation written in portable C11.
Focused on deterministic simulation, incremental feature layering, and maintainable pipelines.

### Tests

CI runs Debug and Release with SDL2 enabled and parallel ctest. Recent additions include Phase 5.2 JSON integration tests for vendor inventory and loot generation, crafting, equipment persistence, and inventory operations. New Phase 2 JSON validation slice adds affix gating (weapon-only affixes excluded from armor), inventory filtering/sorting semantics, and two-handed equip enforcement. Total tests now: 710 (Debug/Release).

Latest fix: cached animation timeline boundary detection now checks scaled time (respects playback_speed) when deciding cache reuse. New unit test `test_animation_collision_sync_cache_scale` verifies activation across boundaries at speed > 1.

<em>“A teaching & experimentation sandbox for loot, combat, procedural generation, progression, and systems design.”</em>

</div>

---

## 0. New Structured Overview

This README has been refactored for fast navigation while preserving every byte of the prior detailed phase logs. A concise, task‑oriented section layout now fronts the document; the full historical/phase narrative appears verbatim in Appendix A ("Full Phase Logs – Original Content"). Nothing was deleted; only reorganized.

### Quick Jump Index

| Section                        | Purpose                                           |
| ------------------------------ | ------------------------------------------------- |
| 1. High‑Level Overview         | Elevator pitch & design pillars                   |
| 2. Feature Matrix              | Snapshot of implemented vs planned features       |
| 3. Systems Overview            | One‑paragraph summaries of each major subsystem   |
| 4. Build & Run                 | Configure, build, test commands                   |
| 5. Configuration & Assets      | Data formats & asset locations                    |
| 6. Testing & Quality Gates     | Determinism, CI gates, fuzz/stat suites           |
| 7. Development Workflow        | Everyday contributor loop                         |
| 12. Contributing               | Contribution standards                            |
| 13. License                    | MIT license reference                             |
| 14. Media / Screenshots        | Placeholder images / planned diagrams             |
| 15. Quick Reference Cheatsheet | Frequently used commands                          |
| Appendix A                     | Full original README phase logs (complete detail) |

---

## 1. Description

Layered, deterministic top‑down action roguelike engine emphasizing: modular boundaries, reproducible simulation, incremental phase roadmaps, strong test coverage (unit, integration, fuzz, statistical), and data‑driven extensibility (hot reload, schema docs, JSON/CSV/CFG ingest). Systems include loot & rarity, equipment layering (implicits → uniques → sets → runewords → gems → affixes → buffs), dungeon & overworld procedural generation, AI behavior trees + perception + LOD scheduling, skill graph & action economy, persistence with integrity hashing, UI virtualization & accessibility, and emerging economy/crafting pipelines.

### Core Design Pillars

- Determinism first (explicit seeds, reproducible golden snapshots)
- Progressive complexity (phased roadmaps with tests per slice)
- Data before code (config/schema/hot reload/tooling surfaces early)
- Integrity & telemetry (hashes, anomaly detectors, analytics export)
- Maintainability (module boundaries audit, minimal cross‑module coupling)

Note for Windows contributors: prefer ASCII punctuation in docs (e.g., '-' instead of '–') to avoid codepage‑dependent test failures when tests read fixed‑size buffers.

### Testing & Quality Gates (quick)

    - Build: use CMake multi‑config generators with parallelism (e.g., -j12)
    - Run tests: ctest -C Debug -j12 --timeout 10 --output-on-failure (use -R <regex> for targeted runs)

Notes:
Latest CI verification: Debug and Release (SDL2) with -j12 both passed 100% (684/684). New unit tests cover core Items (JSON) loading behavior: directory enumeration/indexing, per-file schema rollback on malformed JSON, JSON vs cfg equivalence, JSON→cfg fallback behavior, and Phase 2 JSON validation (affix gating, inventory filter/sort ordering, two-handed equip rule). A Release-only heap corruption in worldgen chest placement (BFS neighbor/queue bounds) was fixed by adding strict index checks and a queue capacity guard in `rogue_dungeon_place_chests`. Worldgen optimization benchmark stabilized via adaptive repetition to avoid timer granularity flakiness in CI. Persistence tests use centralized save path builders; a recent fix updated `test_save_incremental_basic` to honor per-test directories via `rogue_build_slot_path(0)` for stability under parallel runs. On Windows/MSVC, the Content Graph SDL preview avoids VLA-like locals by using compile-time caps (OVERLAY_CG_MAX_NODES/EDGES); the System panel shows FPS via overlay_last_dt() when metrics aren’t initialized. New validation path: `tests/unit/test_skills_validation_pipeline.c` confirms that an offensive-looking skill without coefficients fails validation until a coeff entry is added, after which validation passes. The marked upgrade chest now carries planned contents metadata on its placement record: planned_def_index and planned_rarity, chosen via a strictly-better planner tied to the inventory snapshot with a robust category fallback. New: `rogue_dungeon_debug_sample_reward_tier()` exposes a deterministic sampler for tests, `rogue_dungeon_upgrade_possible()` gates the upgrade guarantee with a conservative equipment snapshot, and `rogue_dungeon_set_upgrade_possible_override()` provides a test hook. Units `test_dungeon_phase8_loot_and_materials`, `test_dungeon_phase8_5_ev_sanity`, `test_dungeon_phase8_3_upgrade_guarantee`, and `test_dungeon_phase8_3_upgrade_coupling_integration` are passing. Phase 9 adds: mutator registry + JSON loader, deterministic K‑choose‑N selection with compatibility enforcement (incompatibility pairs and per‑group caps), run‑summary callback registration/emission, and a depth profile JSON export helper. Units `test_dungeon_phase9_1_mutator_loader`, `test_dungeon_phase9_2_selection`, and `test_dungeon_phase9_3_stacking_and_export` are green. Phase 4 encounter planning expanded with ΔL/critical-path weighting and modifier smoothing; Phase 5 advanced with objective scripting, gate/keystone flags, and dynamic substitution plus two new unit tests; gate remains green.

Vendor Analytics (Phase 14): Added `vendor_analytics.[ch]` module with telemetry APIs and unit tests. Tracks purchases/buybacks by category/rarity, gold spent vs vendor payouts, gold sink coverage ratio, price elasticity slope per category (least squares from observations), negotiation success rate with average skill split, and a price drift EWMA monitor with configurable threshold and latched alerts. Integrated hooks in pricing (sales) and buyback paths. Unit `tests/unit/test_vendor_phase14_analytics.c` exercises core metrics and drift signaling. New: `tests/unit/test_vendor_phase14_6_analytics_stability.c` validates elasticity stability (duplication invariance, degenerate slope ~0) and drift false‑positive guard under ±5% noise with a 25% threshold.

Vendor Security (Phase 15): Anti‑exploit safeguards and integrity checks.

- Purchase spree guard: Tracks rapid purchase bursts and applies a temporary exploit scalar into vendor→player prices up to +5% within a 10s window (caps at 20 purchases).
- Negotiation spam guard: Per‑vendor attempt tracker with a 10s window; the 6th attempt triggers a 20s lockout. Query lockout remaining via the public API.
- Price manipulation guard: Sudden large‑volume purchases apply a small temporary margin buff; integrated into pricing and analytics adjustment percentage.
- Journal hash chain verification on load: The vendor transaction journal persists count/hash/entries (VEX1 block) and verifies a rolling FNV‑1a hash on load. Tampering triggers a load‑time integrity failure.
- Tests: `test_vendor_phase15_security` and `test_vendor_phase15_4_journal_verify` cover spree bounds, lockout behavior, export/import integrity, and tamper detection.

Economy migration tool (Phase 13.3): A new CLI `econ_migrate` updates the versioned economy header and clears dynamic vendor state when value model parameters change.

- Usage: econ_migrate --slot N --curve V --margin V
- Behavior: Loads the given slot, sets RogueEconomyHeader {curve_version, margin_policy_version}, resets vendor pricing EWMA arrays and clears buyback buffers, then saves.
- Test: `test_save_phase13_3_econ_migrate_cli` invokes the tool logic directly and verifies header updates and state clearing.

Vendor journal compaction (Phase 13.4–13.5):

- API: `rogue_vendor_tx_journal_compact_summary(RogueVendorTxCompactionSummary* out)` aggregates the in‑memory transaction journal into a compact summary (totals for sales/buybacks/assimilations, gold sold/bought, cumulative reputation delta, first/last timestamps). Returns 0 on success, -1 when the journal is empty.
- Test: `test_vendor_phase13_compaction` builds a small journal and asserts exact summary totals and timestamps, ensuring equivalence to a full replay for these statistics.

New: Dungeon analytics export and vendor consumer (Phase 9.4)
Vendor UI (Phase 16 slices):

- Pricing breakdown API and tooltip formatter enable dynamic price decomposition in UI (base, condition, policy, rep, negotiation, demand, scarcity, security/exploit, global/biome) with a compact multi-line string. Unit `test_vendor_phase16_tooltip` covers formatting snapshot basics.
- Vendor panel filter persistence module stores category mask, rarity band, and stat range for the session with safe clamping. Unit `test_vendor_phase16_filter_persist` validates default/reset and roundtrip.

- Tabbed Vendor Panel (Phase 16.1): The in-game vendor panel now has tabs (Buy, Sell, Buyback, Special). Use Left/Right to switch tabs; the active tab is tracked in app state and persisted for the session. The Buy tab integrates the filter controls and shows dynamic pricing breakdown tooltips for the hovered/selected item. Placeholders for Sell/Buyback/Special are wired for upcoming slices. Unit `test_vendor_phase16_tabs` verifies tab getter/setter clamping and wrap behavior without SDL.

- Gating manifest helper `rogue_dungeon_export_gating_manifest(path, graph)` writes a compact JSON manifest of inferred traversal/puzzle capability ids and a rooms count. Creates parent directories automatically. Unit: `tests/unit/test_dungeon_phase9_4_gating_manifest.c`.
- Vendor run_summary consumer registers a listener that reacts to reward multipliers; minimal and safe by default. Unit: `tests/unit/test_vendor_phase9_run_summary_consumer.c`.

New: Dungeon Phase 7 Hazards & Traps

- Trap subsystem with JSON definitions (trigger, telegraph_ms, damage profile, cooldown) and damage scaling by depth ΔL and avoidance stats.
- Overlap resolver caps simultaneous high-DPS hazards; disarm interaction uses a deterministic skill-based success chance.
- Validation hardening: the Entities content validator forces skip-texture mode in headless runs to avoid renderer I/O and sheet slicing on 0×0 stubs; prior flag is restored after validation.
- Unit tests: `tests/unit/test_dungeon_phase7_traps.c` covers telegraph timing, scaling monotonicity, overlap cap, and disarm odds.

Phase 6 (initial slice): Added a minimal puzzle/traversal module (`world_gen_dungeon_puzzle.[ch]`) with a tiny JSON loader for puzzle templates and a traversal marker placement scaffold (jump glyph markers in puzzle rooms; timed door markers in treasure rooms). A soft-lock watchdog validates graph connectivity. Unit: `test_dungeon_phase6_puzzle_traversal.c`.

Dungeon generator (Phase 1 update):

- A compact grammar DSL is available for shaping abstract layouts: L(n)=linear, H(k)=hub, B(n,b)=branching. Use via `rogue_dungeon_generate_from_grammar(ctx, "B(18,3)", &graph)` or the extended params API.
- Constraints (loop_percent, max_deadends, min/max degree) and a critical-path length target are enforced in `rogue_dungeon_generate_graph_ex(ctx, &RogueDungeonGenParams{...}, &graph)` with deterministic results and unit coverage.

Developer note (Content Graph overlay): Internals were modularized into `overlay_cg_helpers.{h,c}` (predicates/BFS) and `overlay_cg_snapshot_diff.{h,c}` (snapshots/diff/export/toggle). `panels_content_graph.c` received Doxygen comments (comments-only). Verified Debug (SDL2) build and full CTest run from `build` with `-C Debug -j12`: 100% green (594/594).

MSVC compatibility note:

- Some overlay panels compile as C under MSVC; ensure declarations precede statements (C89/C90 rule). The Skills Effects Node Graph source was adjusted by moving a typedef and static state declarations to the top of the block to satisfy this requirement.

Phase 1.3 runtime slice:

- Execution state now includes INTERRUPTED. `rogue_skill_get_exec_state` reports INTERRUPTED when an activation was canceled via interrupt until the next activation clears it.
- Skill context (`RogueSkillCtx`) carries cast_pos/target_pos and an affected entity array; when unset, cast_pos defaults to the player’s position and target_pos defaults to cast_pos. Affected count is clamped to 8 for safety.
- Interrupt API `rogue_skill_interrupt(id, ctx)` cancels active cast/channel, sets `interrupted_active` and timestamp, triggers end FX (`skill/<id>/end`), and applies partial resource refunds honoring `refund_on_cancel_pct`. New activations clear the interrupted flag. - Re-verified after AI Utility Selector destructor wiring (advanced_nodes): Debug SDL2 full suite remains 100% green (582/582) under -j12. - Optional: enable AI blackboard write/get tracing during fuzz triage by defining ROGUE_TRACE_BB=1 at build time (writes bb_trace.txt in the test working dir). Default is off for quiet CI. - Test helper for speed: in unit tests that load skills content, call `rogue_skills_set_skip_icon_loads(1)` to bypass icon texture I/O. Used by `test_skills_roundtrip_schema` and `test_skills_base_autoreload` to cut runtime to milliseconds while preserving semantics.
  Formatting: - Run per-file clang-format locally: use the CMake targets `format` (auto-fix) and `format-check` (verify). These work on Windows without hitting command-line length limits. - CI installs clang-format and runs `cmake --build build --target format-check`; failures will show which files need formatting.
  Windows CI notes: - The workflow uses the Ninja generator under an MSVC dev environment to avoid intermittent MSBuild stalls when compiling many small unit test targets in parallel.

### Build flags and modules

APIs: `src/debug_overlay/overlay_core.h` plus widgets in `overlay_widgets.h` (Label, Button, Checkbox, SliderInt/Float, InputText, Combo, TreeNode/Pop, ColorEdit RGBA, Table). Input capture in `overlay_input.h`.

### Configuration & Assets (Phase 1 Asset Structure Completion)

- Directory Taxonomy (stable v1):
  - graphics/: sprites/ (characters/enemies/npcs/items/environment/ui), textures/ (materials/effects/backgrounds), fonts/ (ui/dialogue)
  - audio/: music/{ambient,combat,menu}, sfx/{combat,environment,ui,character}, voice/dialogue/
  - data/: levels/{dungeons,overworld}, configs/{gameplay,graphics,audio}, localization/{en,es,fr}, schemas/ (sprite.schema.json, audio.schema.json ... upcoming)
  - shaders/: vertex/, fragment/, compute/
  - meta/: manifests/ (generated), checksums/, documentation/
- Asset Classification Module: `src/util/asset_classification.{h,c}` exposes `rogue_asset_classify(path)` → enum and `rogue_asset_type_str`. Pure string heuristics; no I/O; unit test `test_asset_classification` verifies all categories.
- Placeholder Asset Enforcement (Phase 2): Added `rogue_asset_placeholder_exists()` helper and committed a tiny `assets/placeholder.png` sentinel used by validation/UI injection flows; unit test `test_asset_placeholder_enforcement` covers presence + negative misspelled path.
- Level / Map Schema (Phase 2): Added `schema_levels.{h,c}` defining a JSON shape for authored maps (id, w, h, tiles RLE string, optional tile_size, spawns[], environment_tags[]). Unit test `test_level_schema` covers valid + invalid samples. Cross-field bounds (e.g., spawn inside map) are deferred to higher-level validators.
- Naming Conventions: snake_case, singular category nouns, semantic depth (e.g., graphics/sprites/characters/player/player_idle.png). Revisions favor explicit role (impact_sprite.png vs generic sprite1.png). Audio loops suffixed `_loop` when seamless.
- Version Control: Runtime binaries (png/ogg/wav/json/cfg) tracked. Future large source assets (psd/blend) excluded until /source_art policy phase. Generated manifests/checksums (meta/) to be gitignored when introduced (Phase 4).
- Contributor Guidelines (seed): Provide power‑of‑two friendly sprite sheets when practical; keep individual frame dims ≤512; prefer OGG for music/SFX (MP3 permitted for licensed tracks); normalize peak loudness to -1 dBTP, integrated LUFS target -14 (music) / -18 (ambience); localization JSON one top-level key per feature domain.

#### UI Theme JSON Schema (Phase 2)

Phase 2 introduces a structured JSON schema for overlay/theme configuration, migrating from the legacy key=value `assets/ui_theme_default.cfg`. The schema (internal id `ui_theme`) is defined in `schema_theme.{h,c}` and validated with the RogueSchema system; an accompanying unit test `test_ui_theme_schema` covers positive and negative cases.

Required:

- name: string (1–63 chars)

Optional 32-bit RGBA color integers (stored as unsigned 0xRRGGBBAA):

- panel_bg, panel_border, text_normal, text_accent
- button_bg, button_bg_hot, button_text
- slider_track, slider_fill, tooltip_bg, alert_text

Optional metrics (integer ranges enforced by schema):

- font_size_base (4..256)
- padding_small (0..128)
- padding_large (0..512)
- dpi_scale_x100 (50..400) — logical DPI scale \* 100 (e.g. 125 = 1.25×)

Forward Compatibility: `allow_additional_fields = true` so future layout / spacing keys can be added without breaking older builds. A forthcoming schema versioning pass will add an optional `schema_version` top-level field and centralized migration hooks.

Example (minimal valid theme):

```json
{
  "name": "default_dark",
  "panel_bg": 538976511,
  "panel_border": 1077952575,
  "text_normal": 4294967295,
  "font_size_base": 14,
  "dpi_scale_x100": 100
}
```

(Decimal integers shown for portability across JSON parsers; hex may be supplied in authoring tools before a future formatting helper is added.)

Migration Path: Existing `overlay_theme.json` runtime persistence continues unchanged. Once JSON-driven themes are loaded, a small adapter will export current runtime colors → schema-compliant JSON for editing and reloading. That adapter plus versioned migrations are tracked under the Phase 2 "schema versioning" roadmap item.

Items JSON schema_version and migration (Phase 1 addendum):

- Item definitions in JSON may include an optional `schema_version` (1..1024). The loader now captures this and, after loading a batch, invokes a centralized migration hook to adapt older entries to the current item schema version (`ROGUE_ITEMS_SCHEMA_VERSION_CURRENT`). The default hook is a no-op; tooling or future slices can register a migration via `rogue_items_set_migration_hook` to rewrite fields in-place as schemas evolve. All paths remain headless-safe and validated by the existing item schema; malformed entries are rejected with detailed logs and do not affect previously loaded items.

##### Schema Versioning (Phase 2 completion)

All JSON content schemas (items, skills, entities, tilesets, sprites, audio, levels, ui_theme) now expose an optional integer `schema_version` field (1..1024). A lightweight linear migration registry (`rogue_schema_register_migration`) enables registering consecutive steps (v→v+1). Unit test `test_schema_versioning_basic` demonstrates migrating a `ui_theme` document from version 1 to 2 by injecting a missing `panel_border` field. Future enhancement: automatic pre-validation migration pass and documentation export of available migrations.

#### Phase 3 (Asset System – Completion)

Phase 3 delivers full SDL integration for the initial asset manager slice:

- `asset_manager.{h,c}`: Fixed-cap caches for textures (256) and audio clips (128) with basename‑derived ids, acquire/release ref counting, and compaction on final release.
- Real texture loading (SDL_image): `IMG_LoadTexture` on first acquire (lazy) capturing `width/height` via `SDL_QueryTexture`. Headless (no renderer) builds skip creation gracefully.
- Audio registry (SDL_mixer): `Mix_LoadWAV` lazy loads clips; negative caching prevents repeated failed attempts for missing/corrupt files until a file change occurs.
- Negative caching: `load_failed` flag on both texture and audio records short‑circuits subsequent loads; cleared automatically if mtime changes (reload) or on re-add.
- Hot-reload polling: `rogue_asset_manager_poll_reload()` stat()s tracked asset paths; on mtime delta destroys and reloads textures/audio (width/height re‑queried). Returns count of reloaded assets (used by future editor loop).
- Path resolution: `rogue_find_asset_path` now prepends `SDL_GetBasePath` (plus ascents) to relative probe list improving robustness when running tests from nested build directories or packaged binaries.
- Supported formats: all SDL_image enabled formats (PNG/BMP/JPG/etc.) automatically usable without code changes; audio loading currently targets WAV (extendable to OGG/MP3 once decode policy chosen).
- Tests: `test_asset_manager_basic` exercises duplicate suppression, refcounting, path join normalization, and audio slot allocation (including negative cache path). Full suite passes with Phase 3 features enabled.

Next (Phase 4 preview): integrity & validation layer (schema cross-check, existence, dimension constraints), dependency tracking unification (skills + global), placeholder fallback injection, manifest + checksum generation feeding CI asset verification.

### Items loading (JSON‑first)

- Startup prefers JSON item definitions under `assets/items/` when present, with layered fallbacks to the legacy cfg directory and finally a single cfg (`assets/test_items.cfg`).
- The JSON directory loader rebuilds the fast lookup index after load to keep behavior identical for downstream systems.
- Path resolution uses `SDL_GetBasePath` with relative ascents to be robust across working directories and CI packaging.
- CI: Verified on Windows with SDL2 enabled. Debug and Release builds are green; full suites pass under parallel ctest (`-j12`).

- ROGUE_ENABLE_JSON_CONTENT (default ON): Compiles the content JSON foundation (I/O and schema envelope). Built as an object library (`rogue_content_json`) and linked into `rogue_core` when enabled. A vendored cJSON stub lives under `third_party/cjson` and is linked as `rogue_thirdparty_cjson` for now; replace with the full cJSON later.
- Outputs: HTML, LaTeX/PDF, and XML generated via a dedicated CMake target.
- Prereqs (Windows):
  - Doxygen 1.9.8+ and Graphviz (dot) for diagrams
  - LaTeX distribution (MiKTeX/TeX Live) for PDF (optional)
- Build docs:

  - Generate your build first (CMake multi-config), then run the docs target.
  - In VS Code: run the “docs” task or invoke the CMake target.
  - Outputs land in `build/docs/html`, `build/docs/latex`, and `build/docs/xml`; the PDF (refman.pdf) is under `build/docs/latex` if LaTeX is installed.
  - Custom HTML theme is applied from `docs/templates/theme.css`.
  - CI: Every push/PR builds docs on Windows with SDL enabled and uploads the HTML as an artifact named `docs-html` and as a Pages artifact; GitHub Pages deploys automatically from CI. - Pages URL: enable GitHub Pages for this repo to serve the latest docs. See Actions → Deploy Pages job for the live link after a successful run.
    Build hygiene:

- Warnings: The build uses strong warnings by default. You can opt-in to treat warnings as errors by configuring CMake with -DROGUE_WARNINGS_AS_ERRORS=ON. On MSVC this maps to /WX; on GCC/Clang it maps to -Werror. CI keeps this OFF to avoid spurious red builds across compilers.

  Contributor notes:

  - See docs/DOXYGEN_GUIDE.md for short templates and style rules when adding comments.
  - Prefer documenting headers (APIs) and hard-to-understand internal modules. Keep examples minimal and link related symbols via @ref/@see.

### Debug Overlay (early)

- Unified in-game debug overlay behind a compile-time flag.
- Feature flag: ROGUE_ENABLE_DEBUG_OVERLAY (default ON). When OFF, headers provide no-op stubs for zero overhead.
- Toggle with F1; the overlay renders after the HUD. Input is captured while active so gameplay doesn’t receive keys/mouse.
- APIs: `src/debug_overlay/overlay_core.h` plus widgets in `overlay_widgets.h` (Label, Button, Checkbox, SliderInt/Float, InputText). Input capture in `overlay_input.h`.
  - Layout: simple columns via `overlay_columns_begin/overlay_next_column/overlay_columns_end` (equal or custom widths). Widgets honor column width.
  - Layout now auto-wraps rows across columns; `overlay_next_column` advances within the row, wrapping to the next row after the last column. Row spacing uses the tallest widget in the row for clean grids.
  - Focus: Tab/Shift+Tab traversal across all interactive widgets; Enter/Space activate buttons/checkboxes; sliders respond to Left/Right. InputText supports caret navigation (Home/End/Left/Right), insertion/backspace at caret; clicking the field gives focus and captures input.
  - Split views and preferences: A resizable splitter (`overlay_splitter_begin/end`) enables two-pane layouts with persisted widths per panel via a small preferences store. Current adopters: Items (list + details), Skills (tabs/selection + content), and Map (controls + preview placeholder). Preferences persist to `build/overlay_prefs.json`; delete that file to reset.
- Headless-safe: widget drawing guards avoid SDL calls when no renderer is present (useful in unit tests).
- Tests: `test_overlay_core` and `test_overlay_widgets` (smoke), with the latter validating headless usage and basic interactions via simulated input.
  - New: `test_overlay_layout_focus` covers 2-column auto-wrap and focus traversal.
  - New: `test_overlay_table_widget` validates Table header sorting toggles and row selection using simulated input.
  - New: `test_overlay_inputtext_caret` exercises caret navigation (Home/End/Left/Right), insertion, and backspace sequencing under focus changes.
  - Player debug APIs covered in `test_player_debug_api`: clamps, derived stat recompute on stat changes, god-mode damage bypass, noclip flag roundtrip, and teleport.
  - Verification: Overlay tests pass headlessly in Debug (SDL2) with parallel ctest. Full suite currently all‑green in Debug with SDL2 and -j12.
  - Dungeon Phase 2: Room templates scaffolding, JSON loader (key-order independent), and an environmental deco overlay layer with transform-aware stamping and overlay-aware navigation blocking. Safety is enforced via a `RogueTileMap.overlay_magic` invariant to guard overlay access. See `tests/unit/test_dungeon_phase2_room_templates.c` and `tests/unit/test_dungeon_phase2_room_templates_json.c` (includes deco layering and key-order fuzz).
  - Dungeon Phase 3: Biome & Theme Layering module implements deterministic biome selection per depth, theme profile (fog density, ambient color, ambient SFX), hazard palette with depth scaling, and rare biome substitution. Covered by `tests/unit/test_dungeon_phase3_theme.c`. Phase 3.5 adds statistical tests for biome distribution envelopes and rare-biome probability bounds (`tests/unit/test_dungeon_phase3_5_biome_distribution.c`).
  - Dungeon Phase 4 (slice): Encounter planner with BFS-derived room depths, depth-scaled budgets, elite/miniboss spacing constraints, and rare nemesis injection via a deterministic micro-RNG stream. Covered by `tests/unit/test_dungeon_phase4_encounters.c`.

Overlay panels:

- System panel shows FPS, frame time, draw calls, and tile quads, and includes a toggle for the metrics/overlay and overlay enable.
- Player panel exposes HP/MP/AP and core stats (STR/DEX/VIT/INT) with sliders, God Mode/No-clip toggles, and simple teleports (spawn/center). Player debug APIs are headless-safe and used by the panel.
- Skills panel (new) lists skills and lets you edit timing (cooldown/cast/channel) and coefficients (RogueSkillCoeffParams). Includes a quick 2s single-skill simulate action that dumps a JSON summary for inspection.

  - JSON overrides integration: Save and Load buttons persist skill overrides to/from `build/skills_overrides.json`.
  - Auto-load on startup: set `ROGUE_SKILL_OVERRIDES` to point at an overrides JSON file; when unset, the app attempts `build/skills_overrides.json`.
  - Implementation uses atomic write helpers from json_io; manual edits to the file can be loaded live via the panel's Load button.
  - New: Auto‑Reload Overrides toggle polls the overrides file mtime each frame and applies changes automatically without clicking Load.
  - New: Auto‑Reload Base Skills JSON toggle watches the base skills JSON and performs a full registry reload when the file changes; exposed as a separate checkbox in the Skills panel.
  - Validator coverage extended: added cross‑reference tests for proc.effect_spec_id and coeff requirements across offensive vs passive skills.
  - Internals: the Skills panel is now modularized into per‑tab units (overview/effects/visuals/audio/testing) with headers/guards and a small orchestrator; shared helpers centralize validation banners and overrides save/refresh. Headless‑safe preview paths are preserved for CI runs.

- Entities panel (new): quick inspector for runtime enemies.

  - Lists current enemies with id, alive flag, and position; select a row to target.
  - Actions: Kill, Teleport to Player, and Spawn at Player. Useful for triaging combat/AI.
  - Backed by headless-safe APIs in `src/core/entities/entity_debug.{h,c}` and covered by unit test `tests/unit/test_entity_debug_api.c`.
  - Content validation: a headless schema validator lives in `src/content/schema_entities.{h,c}` with a unit `tests/unit/test_entity_schema.c` that loads enemy type defs from assets and validates fields like group bounds, radii, speed, XP, and loot chance.

- Tilesets schema (new foundation for Map Editor):
  - Headless schema module `src/content/schema_tilesets.{h,c}` defines tilesets.json with fields: `id` (string), `tile_size` (int), `atlas` (string), and `tiles[]` array of objects `{ name, col, row }`.
  - Legacy adapter synthesizes JSON from `assets/tiles.cfg` so existing content validates without format migration.
  - Unit `tests/unit/test_tilesets_schema.c` validates the default assets/tiles.cfg via the schema; runs headlessly in the suite.

### Validation System

The runtime includes a state/content validation system to catch schema issues and cross-reference errors:

- Manager: `src/core/integration/state_validation_manager.{h,c}` registers per-system validators and cross-rule checks, schedules runs on an interval, logs events to a ring buffer, and exposes telemetry (runs, warnings, corruptions, time).
- Wiring: `src/core/integration/validation_wiring.{h,c}` registers default checks at startup: items, entities, tilesets, tag registry, and a cross-rule for skills.
- Overlay: A “Validation” panel lets you Run Now, Force All (bypass snapshot hash incremental skip), and adjust the interval. It shows stats and recent events.
- Headless/CI: A CLI tool `validate_content` runs these checks headlessly and returns a CI-friendly exit code.

Skills panel validation integration:

- Save Overrides JSON: runs `rogue_skill_debug_validate` first; blocks the save when invalid and surfaces the error text in-panel.
- Create flow: runs validation and shows a warning when invalid. This is warn-only to keep prototyping fast; you can tighten to hard-block later if desired.

CLI usage (optional):

```
validate_content --force-all --fail-on-warn
```

Exit codes: 0 OK, 1 warnings (only with --fail-on-warn), 2 corruption. Use `--quiet` to suppress per-event prints.

### Content Graph (Phase 13.3 – expanded)

Status: Complete. - `rogue_asset_dep_count()`, `rogue_asset_dep_get(index, &id, &path)`, `rogue_asset_dep_get_deps(id, out, max)`, and `rogue_asset_dep_hash(id, &out_hash)` (cached; invalidated on edits). - Filters: substring and advanced prefixes `id:`, `path:`, `group:`, `hash:`, `dep:X`, `rev:X`. - For the selected node: direct dependencies, reverse dependencies (who depends on me), and the node’s file hash (hex). - Group size summaries and path collision markers. - Actions: “Compute All Hashes”, “Export DOT” (writes `build/content_graph.dot`), and “Export JSON” (writes `build/content_graph.json`).
• DOT → `build/content_subgraph.dot`
• JSON → `build/content_subgraph.json` with shape `{ "root": string, "depth": int, "nodes": [{ "id", "hash" }], "edges": [{ "from", "to" }] }`. - Layered SDL preview for the selected node and its multi‑hop dependencies (BFS), with a max‑depth slider; toggleable on‑panel. - Visuals: nodes tinted by group; diagnostics line shows last dependency registration rejection (cycle or path conflict) when present; if the last rejection was a cycle and both endpoints are visible in the preview, the connecting edge is highlighted in red. - Interactions: Shift+LMB drag to pin/move node; use ‘Clear Pins’ to reset. - skills/base → assets/skills_uhf87f.json; skills → depends on skills/base - tiles (assets/tiles.cfg), sounds (assets/sounds.cfg), enemies/types (assets/enemies.json) - dialogue/style (assets/dialogue/style_default.json), dialogue/scripts (assets/dialogue/dialogues.json), dialogue (aggregates both) - ui/hud_layout (assets/hud_layout.cfg), ui/theme (assets/ui_theme_default.cfg) - player/anim (assets/player_anim.cfg), player/sheets (assets/player_sheets.cfg) - projectiles (assets/projectiles.cfg), tag_registry (assets/tag_registry.json) - world/biomes, world/trees, world/plants, world/resource_nodes, world/mining_nodes (assets/\*.cfg)

• Content Graph panel: filter by id/path/group/hash, export DOT/JSON, SDL preview with BFS, and quick finders for Orphans and Hubs (top 10 by out‑degree). - Core APIs in `src/core/world/map_debug.{h,c}` provide simple editing and JSON persistence: - `rogue_map_debug_set_tile(x, y, id)`, `rogue_map_debug_brush_square(x, y, radius, id)`, `rogue_map_debug_brush_rect(x, y, w, h, id)` - `rogue_map_debug_save_json(path, err, cap)` and `rogue_map_debug_load_json(path, err, cap)` with compact RLE tiles inside `{ "w": W, "h": H, "tiles": "..." }`. - Loader fixes prevent malformed literal detection and off‑by‑one pointer advance; validated by unit `tests/unit/test_map_debug.c`. - Map Editor panel (now usable): - Labeled tile selection using RogueTileType names; Erase toggle. - Brush modes: Square (radius) and Rect (x0,y0,x1,y1 inputs). - Utilities: Pick tile under player, Fill entire map with current tile, Clear map (EMPTY). - Persistence: editable JSON path with Save/Load buttons using compact RLE format via map_debug APIs. Edits invalidate the tile sprite LUT so visuals refresh immediately.

    - Items panel (new): manage and live-edit item definitions.
    	- Lists item defs in a sortable table with row selection.
    	- Edit core fields for the selected item (e.g., name, sockets, rarity) using safe setters.
    	- Persistence: specify a JSON path and use Save/Load to export/import item defs via `rogue_item_debug_save_json`/`_load_json` (atomic save, dynamic buffer sizing under the hood).
    	- Live vendor repricing: editing item defs updates vendor slot prices in-place via `rogue_vendor_on_item_def_changed`; bulk JSON loads trigger `rogue_vendor_reprice_all`.
    	- Creation Wizard: a guided form to add new base items (ID/Name/Category and core stats). Backed by `rogue_item_debug_create` which validates input, ensures unique IDs, sanitizes fields, and appends via `rogue_item_defs_add`.
    	- Duplicate-as-template: "Duplicate From Selected" opens the Create wizard prefilled from the selected row and suggests a unique id automatically.
    	- Virtualized list (prototype): large registries use a simple row-offset slider to render only visible rows (20 by default) for headless stability; mouse wheel/scrollbar integration planned.
    	- Headless-safe: backed by `src/core/loot/item_debug.{h,c}`; covered by unit `tests/unit/test_item_debug_api.c`.

Content schemas (foundation):

- Items schema (new): `src/content/schema_items.{h,c}` defines and validates items.json (id/name/category required; ranges for stack_max, rarity, sockets, etc.). Units `tests/unit/test_items_schema.c` and `tests/unit/test_items_roundtrip_schema.c` cover validation and a schema-backed export→envelope→reload roundtrip.
- Skills schema (new): `src/content/schema_skills.{h,c}` defines and validates skills.json with required fields and ranges, allowing extra fields for forward-compat. A new unit `tests/unit/test_skills_roundtrip_schema.c` exports a subset to JSON, wraps it in a versioned "skills" envelope, validates, atomically writes, and reloads via the loader to exercise the envelope path.

Items registry & migration:

- Stable handles: `RogueItemDefHandle` provides a generation-checked handle for item base defs. Helpers: `rogue_item_def_handle_from_index`, `rogue_item_def_index_from_handle`, `rogue_item_def_get_by_handle`. Generations bump on add/reset to invalidate stale references.
- Migration/export tool: `items_migrate` (built by CMake) exports the current registry to a versioned JSON envelope using `json_envelope` + `json_io`. Usage: `items_migrate <out.json>`. It attempts to load `assets/items` or falls back to `assets/test_items.cfg` if the registry is empty.
- Loader compatibility: `rogue_item_defs_load_from_json(path)` accepts both a raw JSON array of item defs and a versioned envelope with `$schema: "items"` (reads from `entries`). The schema-backed roundtrip test covers both paths.
- JSON-first toggle and directory scan: Build with `-DROGUE_ITEMS_PREFER_JSON=ON` (default) to prefer `assets/items/*.json` at startup. The loader scans the directory (Win32/posix), loads each file, validates against `schema_items`, skips malformed files safely, and rebuilds the fast id→index map. Fallbacks remain: cfg directory, then single `assets/test_items.cfg`.

Integration harness stabilization:

- `tests/integration/test_boot.c` now aligns its working directory with the repo root (like other integration tests) and uses the full `RogueAppConfig` initializer (logical dimensions and background color). This resolved a previous headless/SDL segfault in CI.

  Test save‑path isolation (stability under parallel ctest):

  - Centralized builders in `src/core/persistence/save_paths.{h,c}` construct slot/autosave/backup/json/quicksave paths and create directories as needed.
  - Tests run in isolated prefixes via `rogue_save_paths_set_prefix_tests()` which uses `ROGUE_TEST_SAVE_DIR` when set, else a per‑PID temp prefix. Persistence tests were updated to use `rogue_build_*` helpers instead of hardcoded filenames.
  - Result: previously flaky save/analytics tests are stable under `ctest -j12`.

Data I/O utilities (for upcoming content schemas):

- json_io: read whole file, atomic write (temp + replace), and file mtime in ms; all return detailed errors via char\* buffers.
- json_envelope: versioned envelope helpers for JSON content files: { "$schema": string, "version": u32, "entries": object|array }. Provides create/parse and frees.
- Tests: `test_json_io` and `test_json_envelope` validate round-trip, mtime, and parse error cases in Debug (SDL2) with parallel ctest.

### Data‑Driven Skill Coefficients (Phase 10.1)

- Centralized coefficients can be loaded from JSON/CSV via `skills_coeffs_load` into the runtime registry.
- The effective scalar per skill is: mastery × specialization × central_coeff(skill_id).
- Minimal APIs:
  - `int rogue_skill_coeffs_parse_json_text(const char* json)`
  - `int rogue_skill_coeffs_load_from_cfg(const char* path)`
  - `int rogue_skill_coeff_exists(int skill_id)`
- Test: run `ctest -C Debug -j8 -R test_skills_phase10_coeffs_loader` to validate parsing and scaling against the stat cache.

### External EffectSpec Config (Phase 10.2)

- Load EffectSpec definitions from a JSON array via `effect_spec_load` and register them in the runtime registry.
- API:
  - `int rogue_effects_load_from_json_text(const char* json, int* out_ids, int max_ids)`
  - `int rogue_effects_load_from_file(const char* path, int* out_ids, int max_ids, char* err, int errcap)`
- Supported fields mirror the legacy key=value parser:
  - kind, debuff, buff_type, magnitude, duration_ms, stack_rule, snapshot
  - scaling: scale_by_buff_type, scale_pct_per_point, snapshot_scale
  - preconditions: require_buff_type, require_buff_min
  - DOT/AURA: pulse_period_ms, damage_type, crit_mode, crit_chance_pct, aura_radius, aura_group_mask
  - SPAWN_PROJECTILE: proj_speed (float, units/sec), proj_life_ms (float, ms), proj_count (int, 1..n)
  - SPAWN_ENTITY: spawn_entity_count (int, 1..8, default 1 if omitted/0 → clamped), spawn_entity_life_ms (float ms, >0, default 5000). Spawns that many lightweight transient entities at the caster position immediately; they auto-expire after the lifetime. (Foundation for future full Summoning system — no AI/behaviors yet.)
- Defaults & semantics:
  - Unset kind → STAT_BUFF; unset stack_rule → ADD; disabled preconditions/scaling use sentinel values.
  - Multiplicative stacking is a no‑op without a baseline; magnitude is percent (100 = no change).
  - For SPAWN_PROJECTILE: damage inherits from EffectSpec.magnitude; projectiles spawn from player and use facing to set initial direction. When proj_count > 1, multiple identical projectiles are emitted in the same frame.
  - For SPAWN_ENTITY: entities are positional placeholders only (Phase 1.2). Lifetime <0 or 0 is sanitized to 5000ms; count outside 1..8 is clamped. Future phases will attach behaviors/ally flags.
  - New kinds: DAMAGE (direct damage), AOE_BLAST (instant area damage), TELEPORT (moves the player along facing by magnitude units; clamped to map bounds). All are available via the loader and runtime.
  - Target inference defaults: if target is omitted, HEAL and TELEPORT default to SELF; AURA and AOE_BLAST default to AREA.
- Test: `test_effectspec_json_loader` covers additive and multiplicative behavior; some tests disable buff dampening (`rogue_buffs_set_dampening(0.0)`) for deterministic rapid re‑applies.
  - Additional test: `tests/unit/test_effectspec_spawn_projectile.c` validates projectile count and damage mapping; full suite is green in Debug (SDL2) under `-j12` (588/588).
  - Additional test: `tests/unit/test_effectspec_spawn_entity.c` validates spawn count clamping (default=1, upper bound 8) and expiry after lifetime.

### Skills Validator (Phase 10.3)

- Central validation entry point: `int rogue_skills_validate_all(char* err, int err_cap)`.
- Checks performed:
  - Skill `effect_spec_id` references must exist when set.
  - Proc definitions must reference valid EffectSpecs; duplicate (event_type, effect_spec_id) pairs are flagged.
  - "Offensive" skills require a coefficient entry in the central table (`rogue_skill_coeff_exists`).
- Proc introspection helpers: `rogue_skills_proc_count()` and `rogue_skills_proc_get_def(int idx, RogueProcDef* out)` for tools/tests.
- Test: `test_skills_phase10_3_validator` initializes the event bus, registers procs/skills, asserts failures for bad refs/dupes/missing coeffs, then passes after fixes.
- Run focused tests: from `build` dir, `ctest -C Debug -j12 -R "test_skills_phase10_3_validator|test_effectspec_json_loader"`.

### Skills Auto‑doc (Phase 10.4)

- Curated documentation generator for skills inputs and related configs.
- API: `int rogue_skills_generate_api_doc(char* buf, int cap)` → writes a stable multi‑section text; returns bytes written, or -1 if `cap` is too small.
- Sections (in order): SKILL_SHEET_COLUMNS, SKILL_FLAGS_AND_TAGS, COST_MAPPING_EXTENSIONS, COEFFS_JSON_FIELDS, EFFECTSPEC_JSON_REFERENCE, VALIDATION_TOOLING.
- Test: `ctest -C Debug -j12 -R test_skills_phase10_4_api_doc` validates ordering and small‑buffer failure.

## Start Screen

The start screen includes:

- Smooth transition to gameplay: Start Screen fades out and, unless reduced-motion is enabled, a brief world fade-in overlay plays to ease the cut. Overlays are cancelled safely on exit.
- Reduced-motion compliance (skips animated fades) and day/night tinting.

- ROGUE_START_DEV_ESCAPE=1: in developer builds, pressing Esc while in gameplay returns to the Start Screen (for rapid iteration). Disabled by default.
  Load overlay basics:
- Opens from Load Game and lists existing slots (up to configured slot count).
- Each entry shows a placeholder thumbnail tinted by a seeded color, plus basic header info.
- Up/Down navigate; Enter loads the selected slot and transitions to gameplay.
- Delete a slot with a confirmation modal from within the list; deletion uses the save manager and is non-destructive to other slots.
- Virtualized list: only visible rows render, and the selection is kept in view as you scroll. In headless/test mode the list defaults to slot 0 only for determinism.

Robustness:

- Continue and Load are gated by a descriptor sanity check (version match, non-zero sections, required components present) and re-validate the selected save header immediately before attempting to load. Corrupt or incompatible headers are ignored and will not trigger a transition.
- For deterministic tests, the Start Screen considers slot 0 when deriving Continue visibility and when populating the Load overlay in headless mode.

Environment overrides:

- ROGUE_START_BG: absolute or relative path to a background image.
- ROGUE_REDUCED_MOTION=1: skip fades and animations for accessibility.
- ROGUE_START_LIST_ALL=1: list all save slots in the Load overlay (default behavior lists slot 0 only in headless/tests to keep snapshots deterministic).
- ROGUE_START_CONFIRM_NEW=1: require a confirmation modal for New Game (default off; headless auto-accepts).
- ROGUE_START_BUDGET_MS: override the Start Screen frame-time budget in milliseconds for the early-frame baseline guard (default 1.0). If the guard detects a regression (absolute or +25% relative), optional visuals (spinner/parallax) are suppressed.
  - Note: The relative regression check only applies after baseline sampling completes; setting the threshold negative disables the relative check (useful for perf smoke tests). For the Start Screen perf smoke unit, the absolute check is disabled by setting `start_perf_budget_ms = 0.0` to avoid CI variance while still exercising the path.
- ROGUE_LOG_LEVEL=debug|info|warn|error: control console verbosity.
- ROGUE_SKILL_OVERRIDES: optional path to a JSON file with per-skill override values; used by the debug Skills panel and auto-loaded during app init.

Credits & Legal overlay:

- Access from the Start screen menu (Credits).
- Tabs: Credits, Licenses, Build. LEFT/RIGHT switches tabs; ESC closes.
- Scroll with UP/DOWN; inertial scrolling with reduced-motion compliance.
- Build tab shows git hash, branch, and build time compiled in via CMake.

Logging (quieter console by default):

- Default log level is WARN. DEBUG/INFO messages are suppressed in normal runs.
- Override with ROGUE_LOG_LEVEL. Accepted values: debug, info, warn, error (or 0..3).

  - PowerShell (Windows): `$env:ROGUE_LOG_LEVEL = 'debug'` then run the app.
  - bash/cmd: `ROGUE_LOG_LEVEL=debug ./roguelike` (bash) or `set ROGUE_LOG_LEVEL=debug && roguelike.exe` (cmd).
  - Reset in PowerShell: `Remove-Item Env:ROGUE_LOG_LEVEL`.
    The app also auto-reads the env on first log, and main() initializes it early.

  Additional noise guards:

  - PNG loader (Windows/WIC) warns once per unique missing/broken asset path to avoid flooding logs during headless tests.

Skills overrides quickstart:

- To capture your current tuning edits, open the Skills panel and click "Save Overrides JSON" (writes to `build/skills_overrides.json`).
- To apply external edits, modify the JSON on disk, then click "Load Overrides JSON" in the panel.
- To use a custom location, set the environment variable before launch:

  - PowerShell: `$env:ROGUE_SKILL_OVERRIDES = 'C:/path/to/overrides.json'`
  - cmd: `set ROGUE_SKILL_OVERRIDES=C:\path\to\overrides.json`
  - Clear in PowerShell: `Remove-Item Env:ROGUE_SKILL_OVERRIDES`

  Persistence robustness:

  - The save system tolerates empty/initial saves (no registered components) by computing CRC/SHA over an empty payload and still writing integrity footers. This enables the initial New Game save path to succeed in minimal test harnesses.
  - Save/Load debug spam has been routed through the central logger at DEBUG level; default WARN keeps the console quiet unless explicitly enabled.

## Audio & VFX (Phases 1–7 Snapshot)

Current capabilities:

- Unified FX bus with deterministic ordering & frame compaction (merged repeat counts) feeding audio (SDL_mixer-backed) and VFX spawning.
- Audio registry with lazy load, per-category + master mixer gains, positional attenuation stub, deterministic variant selection (id suffixed \_N), and music category isolation.
- Music system (Phase 6.1–6.6): logical state machine (explore/combat/boss) with per-state track registration, linear cross-fade (configurable ms), bar-aligned deferred transitions (`rogue_audio_music_set_tempo`, `rogue_audio_music_set_state_on_next_bar`), side-chain ducking envelope, procedural layering (deterministic sweetener selection), environmental reverb preset stubs (`rogue_audio_env_set_reverb_preset` smoothing wet mix), and distance-based low-pass attenuation (`rogue_audio_enable_distance_lowpass`, `rogue_audio_set_lowpass_params`).
- VFX registry & instance pool (lifetime scaling, layers BG→UI, world vs screen space, time scaling/freeze) plus particle emitter pool (variation distributions UNIFORM/NORMAL for scale & lifetime) with composite effects (CHAIN/PARALLEL) and per-instance overrides (scale, lifetime, color).
- Gameplay mapping layer (keys → audio/vfx with priority) wired to damage, skills, buffs, loot triggers.
- Config loader (CSV) with hot-reload watcher & validation error surfacing.
- Deterministic RNG seed override for reproducible particle/audio variant selection.

Recent additions (Phase 6–7):

- APIs: rogue_audio_music_register, rogue_audio_music_set_state, rogue_audio_music_set_state_on_next_bar, rogue_audio_music_set_tempo, rogue_audio_music_update, rogue_audio_duck_music, rogue_audio_music_current, rogue_audio_music_track_weight.
- Cross-fade weights and duck envelope integrated into rogue_audio_debug_effective_gain for testability; future SDL channel volume automation will hook into these scalars.
- New tests:
  - test_audio_vfx_phase6_1_4_music_system (cross-fade midpoint & completion + duck envelope)
  - test_audio_vfx_phase6_2_beat_aligned (verifies bar-aligned deferred transition & fade timing)

Phase 7 (now expanded):

- Blend modes (registry only): RogueVfxBlend enum (ALPHA default; ADD, MULTIPLY) with set/get APIs; renderer will map to GPU states later.
- Screen shake manager: pooled shakes (amplitude, frequency, duration) aggregated into a composite camera offset each frame with decay, deterministic for tests.
- Performance scaling: global 0..1 emission multiplier (rogue_vfx_set_perf_scale / get) applied to particle emission accumulator for adaptive density.
- GPU batch flag stub: enable/disable + query hook to gate future batched sprite/particle path (no batching logic yet).
- Trails: per‑VFX trail emitters (trail_hz, trail_life_ms, trail_max) with per‑instance accumulators; particles flagged as trails for metrics; respects perf scaling.
- Post‑processing stubs: global bloom enable + threshold/intensity params; color grade LUT id + strength; public getters/setters with clamping; renderer hookup pending.
- Decals: registry + instance pool with lifetime aging; spawn with pos/angle/scale; per‑layer counts and screen‑space collection helper.
  Test coverage: Phase 6 and 7 tests validate cross‑fade/ducking/layering, emission math, trails/post/decals behaviors.

Phase 8 (performance & budgeting):

- Per-frame VFX stats snapshot and profiler API: `rogue_vfx_profiler_get_last(RogueVfxFrameStats*)` with counters for spawned*core/trail, culled*{pacing,soft,hard}, and active pools.
- Spawn control: `rogue_vfx_set_pacing_guard(enable, threshold_per_frame)` runs before `rogue_vfx_set_spawn_budgets(soft, hard)` each frame; culled counts are attributed to the stage that clamps.
- Pool audits: `rogue_vfx_particle_pool_audit` and `rogue_vfx_instance_pool_audit` expose active/free and simple run metrics for fragmentation checks.
- Stress: 100 simultaneous impacts test ensures pacing/soft/hard caps work under load without pool corruption.
  All Audio/VFX tests are green locally in Debug with SDL2 and `-j8`.

### Debug Overlay: Audio / VFX Panel (Phase 12)

- New overlay panel "Audio / VFX" enables rapid iteration on audio and visuals without leaving the game:
  - Play sounds by id/key and spawn VFX at a world position or at the cursor (screen→world conversion uses camera and tile scale).
  - Mixer controls: master and per‑category gains, mute toggle, and positional attenuation toggle.
  - Performance controls: global perf scale, pacing guard enable/threshold, and soft/hard spawn budgets.
  - Live stats readout via `RogueVfxFrameStats` (spawned core/trail, culled pacing/soft/hard, active pools).
- A thin headless‑safe API (`src/core/audio_vfx/audiovfx_debug.{h,c}`) decouples the overlay from core FX modules and is exercised by a unit test `tests/unit/test_audio_vfx_overlay_debug_panel.c`.

Phase 9 (determinism & replay):

- Ordering determinism: dispatcher sorts by (emit_frame, priority, id, seq). Sequence normalized post-sort, eliminating producer-order ties.
- Order-insensitive digest: frame digest is XOR of per-event hashes over (type, priority, id, repeats), excluding seq; exposed via `rogue_fx_hash_accumulate_frame` and `rogue_fx_hash_get` for replay.
- Replay & hashing: `rogue_fx_replay_begin_record/end_record/is_recording`, `rogue_fx_replay_load/enqueue_frame/clear`, `rogue_fx_events_hash` (FNV-1a 64) for test validation and divergence checks.
- Tests: ordering invariance and replay/hash stability are covered by `test_audio_vfx_phase9_1_ordering_tuple` and `test_audio_vfx_phase9_2_replay_and_hash` (Debug SDL2, -j8).

## Skills – Effect Processing Pipeline (Phase 3 slice)

Initial slice of the EffectSpec pipeline is in place:

- EffectSpec extended with stacking rule, snapshot flag, periodic pulse scheduler, and simple child chaining (up to 4 children with per-child delay).
- Minimal pending-event queue processes pulses and child chains deterministically via `rogue_effects_update(now_ms)`; called from the rotation simulator loop and can be invoked from the main loop.
- Buff system integration uses existing stacking behaviors (UNIQUE/REFRESH/EXTEND/ADD). Snapshot flag is forwarded to buff records.
- Tests: `test_effectspec_tick_and_chain` validates periodic pulses at exact 100ms quanta over the duration window and a parent→child delayed apply at 50ms.
  Additional stacking rules:
- MULTIPLY and REPLACE_IF_STRONGER were added alongside UNIQUE/REFRESH/EXTEND/ADD. Multiplicative interprets incoming magnitude as percent (150 = +50%); replace-if-stronger updates magnitude only when higher and preserves the longer remaining duration. See `tests/unit/test_effectspec_stack_variants.c`.

EffectSpec parser (Phase 3.1):

- New parser reads simple key=value text (and files) to register EffectSpecs. Fields:
  - effect.<index>.kind = STAT_BUFF
  - effect.<index>.buff_type = STAT_STRENGTH | POWER_STRIKE
  - effect.<index>.magnitude = <int>
  - effect.<index>.duration_ms = <ms>
  - effect.<index>.stack_rule = ADD|REFRESH|EXTEND|UNIQUE|MULTIPLY|REPLACE_IF_STRONGER
  - effect.<index>.snapshot = 0|1
  - effect.<index>.pulse_period_ms = <ms>
  - effect.<index>.childN.id / effect.<index>.childN.delay_ms
- Registration preserves ascending index and defaults stack_rule to ADD if unspecified; explicit UNIQUE is respected.
- Multiplicative effects are a no‑op when no baseline buff of the same type exists (avoids creating a stack from zero). Covered in `tests/unit/test_effectspec_parser.c`.

Preconditions and deterministic ordering (Phase 3.2):

- Each EffectSpec may declare preconditions `require_buff_type` and `require_buff_min` to gate application. If set, the effect only applies when the total of the specified buff type meets the minimum (default min=1). When not set, a sentinel value disables the gate.
- The pending-effect scheduler is deterministic: events are processed ordered by (when_ms, seq), where seq is a per-event sequence assigned at schedule time to break ties for identical timestamps. Reset clears the sequence counter. See `tests/unit/test_effectspec_preconditions_and_order.c`.

Scaling (Phase 3.4):

- Per-attribute scaling lets an effect’s magnitude scale with another buff’s total: fields `scale_by_buff_type` and `scale_pct_per_point` compute `effective = magnitude * (100 + pct*total)/100`.
- `snapshot_scale=1` captures the scaled magnitude at apply time and uses it for all scheduled pulses; `snapshot_scale=0` recomputes at each pulse based on live totals.
- Parser keys supported: `scale_by_buff_type`, `scale_pct_per_point`, `snapshot_scale`.
- Covered by `tests/unit/test_effectspec_snapshot_scale.c`.

Same-timestamp ordering (determinism): when a periodic pulse and a child effect are scheduled for the exact same `when_ms`, the system processes the pulse before the child. Locked by unit `tests/unit/test_effectspec_pulse_child_order.c`.

DOTs (Phase 5 summary):

- Debuff flag: DOT specs default to harmful (debuff=1) unless explicitly overridden.
- Stacking semantics: UNIQUE blocks reapply while active; REFRESH cancels and realigns pulses from the latest apply; EXTEND keeps existing pulse schedule and extends end time. Internally tracks `last_apply_ms` and skips stale pulses for REFRESH.
- Crit modes: per-application crit snapshot vs per-tick crit (deterministic hash); test hooks support a one-shot/global override for determinism.
- Mitigation & logging: Each tick routes through combat mitigation and records damage events including overkill.
- Tests: `test_effectspec_dot_basic`, `test_effectspec_dot_stack`, and `test_effectspec_dot_crit` validate duration, stacking, and crit behavior alongside existing ordering/scaling tests. All pass in Debug with SDL2 (-j8).

HEAL EffectSpec (Phase 1.2 slice):

- New effect kind ROGUE_EFFECT_HEAL restores player health up to max, clamped to prevent overheal.
- Classification: defaults to non-debuff (beneficial); supports negative magnitudes for damage if needed.
- Integration: applies via combat event stream for logging; supports periodic pulses and child chaining like other effects.
- Parser/Loader: recognizes "HEAL" in KV and JSON formats; runtime apply path handles health restoration deterministically.

Skill Execution State and Profiling (Phase 1.3 slice):

- Minimal execution state enum: RogueSkillExecState { IDLE, CASTING, CHANNELING, COOLDOWN } with inline getter `rogue_skill_get_exec_state`.
- Lightweight profiling: per-skill timestamps for last_act_start_ms, last_act_end_ms, last_cast_begin_ms, last_cast_end_ms; populated at runtime transitions for performance monitoring.
- Headless-safe: getters work without SDL; profiling aids in debugging execution timing without full state machine overhead.

### Skill Events on the Event Bus (Phase 7.1)

Two new skill events are emitted by the skills runtime and delivered via the central event bus:

- ROGUE_EVENT_SKILL_CHANNEL_TICK (0x0701): fired at deterministic channel tick boundaries. Payload: skill_id (u16), tick_index (1-based, u16), when_ms (double scheduled time).
- ROGUE_EVENT_SKILL_COMBO_SPEND (0x0702): fired when a combo spender consumes points (instant, cast-complete, or channel). Payload: skill_id (u16), amount (u8), when_ms (double).

Usage pattern (tests and gameplay):

- Subscribe with `rogue_event_subscribe(event_id, callback, user)`.
- Publish occurs inside skills runtime; to deliver callbacks in headless/unit tests, call `rogue_event_process_priority(now_ms)` periodically. The queue is deterministic and callbacks run in publish order by (when_ms, seq).
- See `tests/unit/test_skills_phase7_event_bus.c` for a minimal example that subscribes, advances skills over simulated time, pumps the bus, and asserts receipt.

### Probability & Smoothing (Phase 7.3)

- Proc defs include `chance_pct` (0..100). If omitted, defaults to 100 for back-compat.
- Deterministic RNG stream ensures reproducible results across runs; optional `use_smoothing` accumulates misses to bound variance so triggers converge under sustained attempts.
- Covered by `test_skills_phase7_3_probability`.

#### Loop Guard and Dynamic Scaling (Phases 7.4–7.5)

- Loop guard prevents proc storms and cycles using:
  - Depth cap (max = 8) on nested proc-triggered applications.
  - Cycle signature hash combining event type, effect/proc ids, and time bucket; repeated signatures within the active call tree are suppressed.
- Dynamic proc scaling reduces effective chance after recent repeated triggers:
  - Per-proc recent activity window; each extra trigger reduces chance by ~12% up to ~60%.
  - Deterministic: scaling is derived from the recorded recent slots and the dedicated RNG stream.
- Test notes: Some tests disable buff dampening (`rogue_buffs_set_dampening(0.0)`) to allow rapid re-application when validating scaling/loop guard behavior. See `tests/unit/test_skills_phase7_4_5_loop_and_scaling.c`.

### Auras & Area Effects (Phase 6 – slice)

- New EffectSpec kind AURA with fields: `aura_radius` and `pulse_period_ms`.
- Runtime: each pulse applies damage to enemies within radius of the player, flowing through the standard mitigation and damage-event pipeline. Debuff defaults to 1 unless explicitly overridden.
- Determinism: per-tick crits use a deterministic hash; pulse schedule uses the same pending-event queue as DOTs (tick at t=0 then every `pulse_period_ms` until duration end).
- Defaults: unspecified `aura_radius` defaults to 1.5f for safety; harmful magnitude implies `debuff=1` if unset.
- Exclusivity: optional `aura_group_mask` enables replace-if-stronger behavior across mutually exclusive aura groups; weaker re-applies are ignored. Covered by `test_effectspec_aura_exclusive`.
- Test: `test_effectspec_aura_basic` validates entry/exit and pulse timing determinism. All EffectSpec tests pass in Debug with SDL2 (-j8).
