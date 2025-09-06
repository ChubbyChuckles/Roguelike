/* Lightweight formula expression evaluator (prototype for Formula Editors Phase 2.4)
   Supports +,-,*,/ and parentheses with float literals and a small fixed set of
   variable identifiers. Intent: rapid live preview in debug overlay; NOT a
   sandbox for untrusted input beyond internal tooling.

   Grammar (recursive descent):
     expr  := term ( ('+'|'-') term )*
     term  := factor ( ('*'|'/') factor )*
     factor:= NUMBER | IDENT | '(' expr ')'

   Variables available (case-insensitive, normalized to lowercase):
     base  -> skill coefficient base_scalar
     per   -> skill coefficient per_rank_scalar
     rank  -> current preview rank (1..max_rank)
     str   -> player STR stat sample (preview)
     dex   -> player DEX stat sample (preview)
     int   -> player INT stat sample (preview)
     cap   -> coefficient stat_cap_pct (as fraction 0..1 internally) / 100

   Error handling: returns 0 on success, <0 on parse/eval error. When err buffer
   supplied writes short diagnostic.
*/
#ifndef ROGUE_UTIL_FORMULA_EVAL_H
#define ROGUE_UTIL_FORMULA_EVAL_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueFormulaContext
    {
        float base_scalar;     /* base */
        float per_rank_scalar; /* per */
        float rank;            /* rank */
        float stat_str;        /* str */
        float stat_dex;        /* dex */
        float stat_int;        /* int */
        float stat_cap_pct;    /* cap (already percent e.g. 150 -> 150) */
    } RogueFormulaContext;

    int rogue_formula_eval(const char* expr, const RogueFormulaContext* ctx, float* out_value,
                           char* err, int err_cap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_UTIL_FORMULA_EVAL_H */
