#include "../graphics/sprite.h"
#include "dialogue.h"
#include "dialogue_util.h"

static RogueDialogueStyle g_style = {
    0xFF222228u, 0xFF1A1A1Fu, 0xFF5F5F8Cu, 0xFFFFDC8Cu, 0xFFFFFFFFu, 0x80000000u, 1, 1, 1, 1,
    0,           0xFFAA8844u, 2,           0,           0x40C8A050u, 0x30FFD080u, 2, 1, 1};

static RogueTexture g_parchment_tex;
static int g_parchment_loaded = 0;

int rogue_dialogue_style_set(const RogueDialogueStyle* style)
{
    if (!style)
        return -1;
    g_style = *style;
    return 0;
}

const RogueDialogueStyle* rogue_dialogue_style_get(void) { return &g_style; }

RogueTexture* rogue_dialogue__parchment_texture(void)
{
    return g_parchment_loaded ? &g_parchment_tex : NULL;
}

int rogue_dialogue_style_load_from_json(const char* path)
{
    if (!path)
        return -1;
    char* buf = NULL;
    int len = 0;
    if (rogue_dialogue__read_all(path, &buf, &len) != 0)
        return -2;
    buf[len] = '\0';
    RogueDialogueStyle st = g_style;
    char tmp[128];
    unsigned int col;
    int iv;
    if (rogue_dialogue__json_extract_string(buf, "panel_color_top", tmp, sizeof tmp) == 0)
    {
        if (rogue_dialogue__parse_color(tmp, &col) == 0)
            st.panel_color_top = col;
    }
    if (rogue_dialogue__json_extract_string(buf, "panel_color_bottom", tmp, sizeof tmp) == 0)
    {
        if (rogue_dialogue__parse_color(tmp, &col) == 0)
            st.panel_color_bottom = col;
    }
    if (rogue_dialogue__json_extract_string(buf, "border_color", tmp, sizeof tmp) == 0)
    {
        if (rogue_dialogue__parse_color(tmp, &col) == 0)
            st.border_color = col;
    }
    if (rogue_dialogue__json_extract_string(buf, "speaker_color", tmp, sizeof tmp) == 0)
    {
        if (rogue_dialogue__parse_color(tmp, &col) == 0)
            st.speaker_color = col;
    }
    if (rogue_dialogue__json_extract_string(buf, "text_color", tmp, sizeof tmp) == 0)
    {
        if (rogue_dialogue__parse_color(tmp, &col) == 0)
            st.text_color = col;
    }
    if (rogue_dialogue__json_extract_string(buf, "text_shadow_color", tmp, sizeof tmp) == 0)
    {
        if (rogue_dialogue__parse_color(tmp, &col) == 0)
            st.text_shadow_color = col;
    }
    if (rogue_dialogue__json_extract_string(buf, "accent_color", tmp, sizeof tmp) == 0)
    {
        if (rogue_dialogue__parse_color(tmp, &col) == 0)
            st.accent_color = col;
    }
    if (rogue_dialogue__json_extract_string(buf, "parchment_texture", tmp, sizeof tmp) == 0)
    {
        if (rogue_texture_load(&g_parchment_tex, tmp))
            g_parchment_loaded = 1;
    }
    if (rogue_dialogue__json_extract_int(buf, "enable_gradient", &iv) == 0)
        st.enable_gradient = iv;
    if (rogue_dialogue__json_extract_int(buf, "enable_text_shadow", &iv) == 0)
        st.enable_text_shadow = iv;
    if (rogue_dialogue__json_extract_int(buf, "show_blink_prompt", &iv) == 0)
        st.show_blink_prompt = iv;
    if (rogue_dialogue__json_extract_int(buf, "show_caret", &iv) == 0)
        st.show_caret = iv;
    if (rogue_dialogue__json_extract_int(buf, "panel_height", &iv) == 0)
        st.panel_height = iv;
    if (rogue_dialogue__json_extract_int(buf, "border_thickness", &iv) == 0)
        st.border_thickness = iv;
    if (rogue_dialogue__json_extract_int(buf, "use_parchment", &iv) == 0)
        st.use_parchment = iv;
    if (rogue_dialogue__json_extract_string(buf, "glow_color", tmp, sizeof tmp) == 0)
    {
        if (rogue_dialogue__parse_color(tmp, &col) == 0)
            st.glow_color = col;
    }
    if (rogue_dialogue__json_extract_string(buf, "rune_strip_color", tmp, sizeof tmp) == 0)
    {
        if (rogue_dialogue__parse_color(tmp, &col) == 0)
            st.rune_strip_color = col;
    }
    if (rogue_dialogue__json_extract_int(buf, "glow_strength", &iv) == 0)
        st.glow_strength = iv;
    if (rogue_dialogue__json_extract_int(buf, "corner_ornaments", &iv) == 0)
        st.corner_ornaments = iv;
    if (rogue_dialogue__json_extract_int(buf, "vignette", &iv) == 0)
        st.vignette = iv;
    g_style = st;
    free(buf);
    return 0;
}
