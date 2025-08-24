#include "loot_item_defs_sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LineBuf
{
    char* text;
} LineBuf;

static int cmp_lines(const void* a, const void* b)
{
    const LineBuf* A = (const LineBuf*) a;
    const LineBuf* B = (const LineBuf*) b;
    const char* ca = strchr(A->text, ',');
    const char* cb = strchr(B->text, ',');
    size_t la = ca ? (size_t) (ca - A->text) : strlen(A->text);
    size_t lb = cb ? (size_t) (cb - B->text) : strlen(B->text);
    size_t m = la < lb ? la : lb;
    int r = strncmp(A->text, B->text, m);
    if (r == 0)
    {
        if (la < lb)
            return -1;
        if (la > lb)
            return 1;
    }
    return r;
}

int rogue_item_defs_sort_cfg(const char* in_path, const char* out_path)
{
    if (!in_path || !out_path)
        return -1;
    FILE* fin = NULL;
    FILE* fout = NULL;
    int data_count = 0;
    int cap = 0;
    LineBuf* lines = NULL;
#if defined(_MSC_VER)
    fopen_s(&fin, in_path, "rb");
#else
    fin = fopen(in_path, "rb");
#endif
    if (!fin)
        return -2;
    char** preface = NULL;
    int pre_count = 0;
    int pre_cap = 0;
    int seen_data = 0;
    char buf[1024];
    while (fgets(buf, sizeof buf, fin))
    {
        size_t len = strlen(buf);
        char* copy = (char*) malloc(len + 1);
        if (!copy)
        {
            fclose(fin);
            return -3;
        }
        memcpy(copy, buf, len + 1);
        int is_comment = (copy[0] == '#');
        int is_blank = 1;
        for (size_t i = 0; i < len; i++)
        {
            if (copy[i] != ' ' && copy[i] != '\t' && copy[i] != '\r' && copy[i] != '\n')
            {
                is_blank = 0;
                break;
            }
        }
        if (is_comment || is_blank)
        {
            if (!seen_data)
            {
                if (pre_count >= pre_cap)
                {
                    pre_cap = pre_cap ? pre_cap * 2 : 8;
                    preface = (char**) realloc(preface, sizeof(char*) * pre_cap);
                }
                preface[pre_count++] = copy;
                continue;
            }
        }
        if (data_count >= cap)
        {
            cap = cap ? cap * 2 : 16;
            lines = (LineBuf*) realloc(lines, sizeof(LineBuf) * cap);
        }
        lines[data_count++].text = copy;
        seen_data = 1;
    }
    /* Handle potential final line without newline if file didn't end with newline and not captured
     * by fgets? Already handled since fgets returns line regardless, so no action. */
    fclose(fin);
    if (data_count > 1)
    {
        qsort(lines, data_count, sizeof(LineBuf), cmp_lines);
    }
    /* Debug counts could be printed here if needed in future (removed in final version). */
#if defined(_MSC_VER)
    fopen_s(&fout, out_path, "wb");
#else
    fout = fopen(out_path, "wb");
#endif
    if (!fout)
    {
        for (int i = 0; i < data_count; i++)
            free(lines[i].text);
        for (int i = 0; i < pre_count; i++)
            free(preface[i]);
        free(lines);
        free(preface);
        return -4;
    }
    for (int i = 0; i < pre_count; i++)
    {
        fputs(preface[i], fout);
        free(preface[i]);
    }
    for (int i = 0; i < data_count; i++)
    {
        fputs(lines[i].text, fout);
        free(lines[i].text);
    }
    /* Ensure data is flushed and file closed before attempting to read it elsewhere */
    fflush(fout);
    fclose(fout);
    free(lines);
    free(preface);
    return data_count;
}
