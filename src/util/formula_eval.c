#include "formula_eval.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    const char* s;
    const RogueFormulaContext* ctx;
    char err[64];
    int has_err;
} FEState;

static void set_err(FEState* st, const char* msg)
{
    if (!st->has_err)
    {
        snprintf(st->err, sizeof st->err, "%s", msg);
        st->has_err = 1;
    }
}
static void skip_ws(FEState* st)
{
    while (*st->s && (unsigned char) *st->s <= 32)
        st->s++;
}
static int match(FEState* st, char c)
{
    skip_ws(st);
    if (*st->s == c)
    {
        st->s++;
        return 1;
    }
    return 0;
}

static float parse_expr(FEState* st); /* fwd */

static float parse_number(FEState* st, int* ok)
{
    skip_ws(st);
    int neg = 0;
    if (*st->s == '+')
        st->s++;
    else if (*st->s == '-')
    {
        neg = 1;
        st->s++;
    }
    float v = 0.f;
    int digits = 0;
    while (*st->s && isdigit((unsigned char) *st->s))
    {
        v = v * 10.f + (float) (*st->s - '0');
        st->s++;
        digits++;
    }
    if (*st->s == '.')
    {
        st->s++;
        float place = 0.1f;
        while (*st->s && isdigit((unsigned char) *st->s))
        {
            v += (*st->s - '0') * place;
            place *= 0.1f;
            st->s++;
            digits++;
        }
    }
    if (!digits)
    {
        *ok = 0;
        set_err(st, "expected number");
        return 0.f;
    }
    if (neg)
        v = -v;
    *ok = 1;
    return v;
}

static float parse_ident(FEState* st, int* ok)
{
    skip_ws(st);
    char buf[16];
    int i = 0;
    if (!(isalpha((unsigned char) *st->s) || *st->s == '_'))
    {
        *ok = 0;
        set_err(st, "expected ident");
        return 0.f;
    }
    while (*st->s && (isalnum((unsigned char) *st->s) || *st->s == '_') && i < (int) sizeof buf - 1)
    {
        buf[i++] = (char) tolower((unsigned char) *st->s++);
    }
    buf[i] = '\0';
    *ok = 1;
    if (strcmp(buf, "base") == 0)
        return st->ctx->base_scalar;
    if (strcmp(buf, "per") == 0)
        return st->ctx->per_rank_scalar;
    if (strcmp(buf, "rank") == 0)
        return st->ctx->rank;
    if (strcmp(buf, "str") == 0)
        return st->ctx->stat_str;
    if (strcmp(buf, "dex") == 0)
        return st->ctx->stat_dex;
    if (strcmp(buf, "int") == 0)
        return st->ctx->stat_int;
    if (strcmp(buf, "cap") == 0)
        return st->ctx->stat_cap_pct / 100.f;
    set_err(st, "unknown ident");
    return 0.f;
}

static float parse_factor(FEState* st, int* ok)
{
    skip_ws(st);
    if (match(st, '('))
    {
        float v = parse_expr(st);
        if (!match(st, ')'))
        {
            set_err(st, "missing )");
            *ok = 0;
            return 0.f;
        }
        *ok = 1;
        return v;
    }
    if (isdigit((unsigned char) *st->s) || *st->s == '+' || *st->s == '-')
    {
        return parse_number(st, ok);
    }
    if (isalpha((unsigned char) *st->s) || *st->s == '_')
    {
        return parse_ident(st, ok);
    }
    set_err(st, "unexpected token");
    *ok = 0;
    return 0.f;
}

static float parse_term(FEState* st)
{
    int ok = 1;
    float v = parse_factor(st, &ok);
    if (!ok)
        return 0.f;
    for (;;)
    {
        skip_ws(st);
        if (*st->s == '*' || *st->s == '/')
        {
            char op = *st->s++;
            int ok2 = 1;
            float rhs = parse_factor(st, &ok2);
            if (!ok2)
                return 0.f;
            if (op == '*')
                v *= rhs;
            else
            {
                if (rhs == 0.f)
                {
                    set_err(st, "div0");
                    return 0.f;
                }
                v /= rhs;
            }
        }
        else
            break;
    }
    return v;
}

static float parse_expr(FEState* st)
{
    float v = parse_term(st);
    for (;;)
    {
        skip_ws(st);
        if (*st->s == '+' || *st->s == '-')
        {
            char op = *st->s++;
            float rhs = parse_term(st);
            if (op == '+')
                v += rhs;
            else
                v -= rhs;
        }
        else
            break;
    }
    return v;
}

int rogue_formula_eval(const char* expr, const RogueFormulaContext* ctx, float* out_value,
                       char* err, int err_cap)
{
    if (!expr || !ctx || !out_value)
        return -1;
    FEState st;
    st.s = expr;
    st.ctx = ctx;
    st.err[0] = '\0';
    st.has_err = 0;
    float v = parse_expr(&st);
    skip_ws(&st);
    if (st.has_err)
    {
        if (err && err_cap > 0)
        {
            snprintf(err, err_cap, "%s", st.err);
        }
        return -2;
    }
    if (*st.s)
    {
        if (err && err_cap > 0)
            snprintf(err, err_cap, "trailing");
        return -3;
    }
    *out_value = v;
    if (err && err_cap > 0)
        err[0] = '\0';
    return 0;
}
