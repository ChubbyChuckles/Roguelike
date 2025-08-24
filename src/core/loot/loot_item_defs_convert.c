#include "loot_item_defs_convert.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void rtrim(char* s)
{
    size_t n = strlen(s);
    while (n > 0)
    {
        char c = s[n - 1];
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
        {
            s[--n] = '\0';
        }
        else
            break;
    }
}
static char* ltrim(char* s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

int rogue_item_defs_convert_tsv_to_csv(const char* tsv_path, const char* out_csv_path)
{
    if (!tsv_path || !out_csv_path)
        return -1;
    FILE* fin = NULL;
    FILE* fout = NULL;
    int converted = 0;
#if defined(_MSC_VER)
    fopen_s(&fin, tsv_path, "rb");
#else
    fin = fopen(tsv_path, "rb");
#endif
    if (!fin)
        return -2;
#if defined(_MSC_VER)
    fopen_s(&fout, out_csv_path, "wb");
#else
    fout = fopen(out_csv_path, "wb");
#endif
    if (!fout)
    {
        fclose(fin);
        return -3;
    }
    char line[1024];
    while (fgets(line, sizeof line, fin))
    {
        rtrim(line);
        char* p = ltrim(line);
        if (p[0] == '#' || p[0] == '\0')
        {
            continue;
        }
        for (char* q = p; *q; ++q)
        {
            if (*q == '\t')
                *q = ',';
        }
        fprintf(fout, "%s\n", p);
        converted++;
    }
    fclose(fin);
    fclose(fout);
    return converted;
}
