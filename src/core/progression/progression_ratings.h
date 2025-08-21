/* Rating & Diminishing Returns System (Phase 3) */
#ifndef ROGUE_PROGRESSION_RATINGS_H
#define ROGUE_PROGRESSION_RATINGS_H

enum RogueRatingType
{
    ROGUE_RATING_CRIT = 0,
    ROGUE_RATING_HASTE = 1,
    ROGUE_RATING_AVOIDANCE = 2
};
float rogue_rating_effective_percent(enum RogueRatingType type, int rating);
float rogue_rating_apply_chain(enum RogueRatingType type, float base_flat_percent, int rating,
                               float mult_modifier_percent);

#endif
