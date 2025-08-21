/* Phase 3 Rating & Diminishing Returns Implementation */
#include "core/progression/progression_ratings.h"

static const int g_breaks[] = {0, 200, 600, 1400, 2600};
static const int g_break_count = (int)(sizeof g_breaks / sizeof g_breaks[0]);

typedef struct RatingBandCurve { float scale; float k; } RatingBandCurve;
typedef struct RatingCurveSet { RatingBandCurve bands[5]; float hard_cap; } RatingCurveSet;

static const RatingCurveSet g_curves[3] = {
    { { {0.065f,180.f},{0.040f,320.f},{0.022f,640.f},{0.012f,1200.f},{0.008f,2000.f} }, 75.f },
    { { {0.050f,220.f},{0.030f,420.f},{0.018f,780.f},{0.010f,1400.f},{0.007f,2400.f} }, 55.f },
    { { {0.055f,200.f},{0.034f,360.f},{0.020f,700.f},{0.011f,1300.f},{0.0075f,2200.f} },65.f }
};

static float curve_apply(enum RogueRatingType type, int rating){
    if(rating<=0) return 0.f; if(type<0||type>2) type=0; const RatingCurveSet* set=&g_curves[type]; float total=0.f;
    for(int b=0;b<g_break_count;b++){
        int start=g_breaks[b]; if(rating<start) break; int next=(b+1<g_break_count)? g_breaks[b+1]: -1; int band_rating;
        if(next<0) band_rating = rating - start; else { int span = next-start; band_rating = (rating-start>=span)? span: (rating-start); }
        if(band_rating<=0) continue; const RatingBandCurve* bc=&set->bands[b]; float dr=(float)band_rating; float inc=(dr*bc->scale)/(1.f+dr/bc->k); total+=inc; }
    if(total>set->hard_cap) total=set->hard_cap; return total;
}

float rogue_rating_effective_percent(enum RogueRatingType type, int rating){ return curve_apply(type,rating); }
float rogue_rating_apply_chain(enum RogueRatingType type, float base_flat_percent, int rating, float mult_modifier_percent){ float eff=base_flat_percent+curve_apply(type,rating); if(mult_modifier_percent!=0.f) eff*=(1.f+mult_modifier_percent/100.f); return eff; }
