# Rarity & Affix Style Guide (Phase 23.2)

Authoritative conventions for adding or modifying item rarity tiers and affix (prefix/suffix) definitions. Follow this to keep content coherent, balanced, and readable.

Contents
- Adding a New Affix - Checklist

## 1. Goals
* Communicate item power progression via immediately recognizable rarity colors & names.
* Keep affix ids short, descriptive, and machine‑friendly (no spaces, lowercase snake).
* Preserve deterministic behavior: ordering & weights remain stable unless intentionally rebalanced (then document change & add/update tests).

## 2. Rarity Tiers
Existing core tiers (indexes 0..4) map to colors through the centralized rarity color utility (Phase 24.3). Names SHOULD follow escalating clarity:

| Index | Canonical Name | Visual Intent | Typical Use |
|-------|----------------|---------------|-------------|
| 0 | common | Neutral baseline | Core progression fodder |
| 1 | uncommon | Slight tint | Small single stat bumps |
| 2 | rare | Stronger color | Introduces 1–2 affixes |
| 3 | epic | Vibrant accent | High stat ceilings, unique combos |
| 4 | legendary | Distinct highlight / beam | Signature or build‑defining effects |

Guidelines:
* Avoid adding new tiers until systems (UI color mapping, pity, floor, VFX) are extended—never hot‑add above 4 without roadmap phase + tests.
* Power Delta: Each ascending tier should feel ~35–55% more impactful (aggregate) than the previous over a representative build segment. Exceeding this risks compression (skipping tiers). Undershooting dulls excitement.
* Rarity Floors (5.7) & Pity (5.8) protect players from drought; when rebalancing, validate regression tests (5.3 distribution + 20.2/20.3 stats) still pass.

## 3. Rarity Weights
Per‑rarity affix weight arrays (`weight_per_rarity[5]`) control availability. Recommended patterns:
* Monotonic Non‑Increase for niche high tier affixes: e.g. `[40,25,10,4,1]`.
* Plateau then taper for broadly usable affixes: `[60,60,45,25,10]`.
* Zero weight cleanly disables at a tier; prefer zero to tiny non‑zero (avoids misleading probability expectations).
* If total selectable weight for a prefix or suffix at a given tier falls below 3 affixes, consider adding filler to preserve diversity.

Rebalance Process:
1. Copy pre‑change JSON snapshot from `rogue_affixes_export_json` (Phase 21.3 tooling).
2. Apply adjustments; keep diff minimal and grouped (see Section 7 sorting).
3. Update balancing notes (roadmap or a future CHANGELOG section) if probability envelopes shift >10% for any tier.
4. Run distribution & anomaly tests (5.3, 22.5, 20.2/20.3) to ensure no inadvertent spikes.

## 4. Affix ID Naming
Conventions:
* Lowercase snake case: `dmg_flat_small`, `agility_boost`, `crit_chance`, `life_leech`.
* Prefix semantic group with short stem when families exist (e.g. `dmg_flat_`, `dmg_pct_`, `res_fire_`).
* Avoid ambiguous abbreviations (`spd` → prefer `speed`). Acceptable short stems: `dmg`, `atk`, `mov`, `res`, `crit`.
* Never encode rarity or numeric ranges in the id (`_rare`, `_10_20` etc.). Those are data fields.
* Length limit: must fit into `char id[48]` (enforced at parse). Keep practical target <= 32 for tooling.

Collision Handling:
* Duplicate ids should be rejected in a future validation phase; presently avoid manual duplicates (would overwrite semantic meaning during balancing reviews).

## 5. Stat Range Guidelines
* `min_value` ≤ `max_value`; parser auto‑corrects if reversed but DO NOT rely on this for authored content.
* Narrow early tiers; widen range on higher tiers to add excitement (wider high rolls) while raising floor modestly.
  Example progression (flat damage):
  * Tier 0: 1–2
  * Tier 1: 2–4
  * Tier 2: 4–7
  * Tier 3: 6–11
  * Tier 4: 9–15
* Avoid negative ranges unless mechanic explicitly supports drawback affixes (future risk/reward phase). Current pipeline assumes non‑negative for additive stats.

### 5.1 Quality Scalar Interaction
When using `rogue_affix_roll_value_scaled`, verify that scaling does not allow surpassing `max_value`. The function biases toward the ceiling without exceeding it. Ensure `max_value` still represents a meaningful perfect roll; do not inflate ceilings solely for quality—this erodes low/medium roll differentiation.

## 6. Prefix vs Suffix Allocation
High impact multiplicative or build‑defining stats (e.g., crit chance, elemental conversion) should be mutually exclusive across prefix/suffix to curb stacking abuse unless backed by a separate balancing phase. If both sides include same stat family, enforce diminished ranges on one side.

## 7. File Ordering & Sorting
* Maintain alphabetical order by affix id within config to ensure stable diffs (Phase 21.5 style mirrored for items).
* Group families contiguously (all `dmg_flat_` before `dmg_pct_`). If natural alphabetical breaks a logical group, optionally use a shared stem to keep grouped.
* A future automated sorter may extend `rogue_item_defs_sort_cfg`; author now as if sorter exists.

## 8. Adding a New Affix - Checklist
1. Choose id following Section 4.
2. Select type (PREFIX/SUFFIX) balancing existing counts (target near parity ±15%).
3. Define min/max respecting Section 5 progression.
4. Author per‑rarity weights pattern (Section 3); ensure at least one non‑zero weight.
5. Append line alphabetically in `affixes.cfg` (retain header/comment block if any).
6. Regenerate JSON snapshot (tooling) if tracking changes.
7. Run unit tests: affix parsing, export JSON, rarity distribution (5.3), anomaly detection (22.5) for sanity.
8. Update documentation if semantic category is new (add brief bullet here under Section 9).

## 9. Semantic Categories (Current)
* Flat Damage (`dmg_flat_*`): Additive weapon damage increments.
* Agility (`agility_*`): Movement / attack speed scaling hooks.
* Future reserved: life_leech, bleed_chance, res_* (elemental resist), armor_flat, armor_pct.

## 10. Prohibited Patterns (Until Dedicated Phase)
* Affixes modifying drop rates / rarity floor (reserved for analytics integrity).
* Affixes that alter save serialization format (complex systemic coupling).
* Player input / control modifying affixes (belongs to skill or reaction system phases).

## 11. Review & Testing
Any PR adding or rebalancing ≥5 affixes should attach a short diff summary (counts per rarity, min/max delta ranges). Optionally produce before/after weight distribution charts (external script) – not mandatory yet.

## 12. FAQ
Q: Why not encode scaling formulas instead of fixed ranges?
A: Determinism & simplicity. Ranges keep balancing transparent and tooling straightforward; dynamic formulas can be introduced later behind feature flag with tests.

Q: Can we introduce negative affixes for corruption mechanics?
A: Yes, but only after a future corruption phase adds UI surfacing + roll explanation; document trade‑off semantics here when implemented.

---
Maintained under roadmap Phase 23.2. Update this file atomically with any systemic rarity or affix semantic change.
