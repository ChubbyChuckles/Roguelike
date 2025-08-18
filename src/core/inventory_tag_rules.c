#include "core/inventory_tag_rules.h"
#include "core/loot_item_defs.h" /* for rogue_item_def_at */
#include "core/inventory_tags.h"
#include <string.h>
#include <stdlib.h>

static RogueInvTagRule g_rules[ROGUE_INV_TAG_RULE_MAX];
static int g_rule_count = 0;
static uint32_t* g_rule_accent_colors = NULL; /* per def index cached accent */

static int ensure_accent_cache(void){ if(!g_rule_accent_colors){ g_rule_accent_colors=(uint32_t*)calloc(ROGUE_ITEM_DEF_CAP,sizeof(uint32_t)); if(!g_rule_accent_colors) return -1; } return 0; }

int rogue_inv_tag_rules_add(uint8_t min_rarity, uint8_t max_rarity, uint32_t category_mask,
    const char* tag, uint32_t accent_color_rgba){
    if(g_rule_count>=ROGUE_INV_TAG_RULE_MAX) return -1;
    if(!tag||!*tag) return -1;
    if(max_rarity==0) max_rarity=0xFF; /* allow caller to pass 0 for open upper bound */
    RogueInvTagRule* r=&g_rules[g_rule_count];
    r->min_rarity=min_rarity; r->max_rarity=max_rarity; r->category_mask=category_mask; r->accent_color_rgba=accent_color_rgba; memset(r->tag,0,sizeof r->tag); size_t len=strlen(tag); if(len>=sizeof(r->tag)) len=sizeof(r->tag)-1; memcpy(r->tag,tag,len);
    g_rule_count++;
    return 0;
}

int rogue_inv_tag_rules_count(void){ return g_rule_count; }
const RogueInvTagRule* rogue_inv_tag_rules_get(int index){ if(index<0||index>=g_rule_count) return NULL; return &g_rules[index]; }
void rogue_inv_tag_rules_clear(void){ g_rule_count=0; }

static void apply_rules_one(int def_index){ const RogueItemDef* d=rogue_item_def_at(def_index); if(!d) return; ensure_accent_cache();
    for(int i=0;i<g_rule_count;i++){ const RogueInvTagRule* r=&g_rules[i]; if(d->rarity < r->min_rarity) continue; if(r->max_rarity!=0xFF && d->rarity > r->max_rarity) continue; if(r->category_mask && ((r->category_mask & (1u<<d->category))==0)) continue; /* match */
        if(r->tag[0]) rogue_inv_tags_add_tag(def_index, r->tag);
        if(r->accent_color_rgba && g_rule_accent_colors){ if(g_rule_accent_colors[def_index]==0) g_rule_accent_colors[def_index]=r->accent_color_rgba; }
    }
}

void rogue_inv_tag_rules_apply_def(int def_index){ apply_rules_one(def_index); }

uint32_t rogue_inv_tag_rules_accent_color(int def_index){ if(!g_rule_accent_colors) return 0; if(def_index<0||def_index>=ROGUE_ITEM_DEF_CAP) return 0; return g_rule_accent_colors[def_index]; }

/* Persistence format (component id TBD):
 * uint16 rule_count
 * For each rule:
 *  uint8 min_rarity
 *  uint8 max_rarity
 *  uint32 category_mask
 *  uint32 accent_color_rgba
 *  uint8 tag_len
 *  bytes tag (tag_len, no null)
 */
int rogue_inv_tag_rules_write(FILE* f){ if(!f) return -1; uint16_t rc=(uint16_t)g_rule_count; if(fwrite(&rc,sizeof(rc),1,f)!=1) return -1; for(int i=0;i<g_rule_count;i++){ const RogueInvTagRule* r=&g_rules[i]; uint8_t tag_len=(uint8_t)strlen(r->tag); if(fwrite(&r->min_rarity,1,1,f)!=1) return -1; if(fwrite(&r->max_rarity,1,1,f)!=1) return -1; if(fwrite(&r->category_mask,sizeof(r->category_mask),1,f)!=1) return -1; if(fwrite(&r->accent_color_rgba,sizeof(r->accent_color_rgba),1,f)!=1) return -1; if(fwrite(&tag_len,1,1,f)!=1) return -1; if(tag_len>0){ if(fwrite(r->tag,1,tag_len,f)!=tag_len) return -1; } }
    return 0; }

int rogue_inv_tag_rules_read(FILE* f, size_t size){ (void)size; if(!f) return -1; g_rule_count=0; uint16_t rc=0; if(fread(&rc,sizeof(rc),1,f)!=1) return -1; if(rc>ROGUE_INV_TAG_RULE_MAX) rc=ROGUE_INV_TAG_RULE_MAX; for(int i=0;i<(int)rc;i++){ RogueInvTagRule r; memset(&r,0,sizeof r); uint8_t tag_len=0; if(fread(&r.min_rarity,1,1,f)!=1) return -1; if(fread(&r.max_rarity,1,1,f)!=1) return -1; if(fread(&r.category_mask,sizeof(r.category_mask),1,f)!=1) return -1; if(fread(&r.accent_color_rgba,sizeof(r.accent_color_rgba),1,f)!=1) return -1; if(fread(&tag_len,1,1,f)!=1) return -1; if(tag_len>=sizeof(r.tag)) tag_len=sizeof(r.tag)-1; if(tag_len>0){ if(fread(r.tag,1,tag_len,f)!=tag_len) return -1; r.tag[tag_len]='\0'; }
        g_rules[g_rule_count++]=r; }
    /* accent colors need recompute if cache present */
    if(g_rule_accent_colors){ memset(g_rule_accent_colors,0,ROGUE_ITEM_DEF_CAP*sizeof(uint32_t)); }
    return 0; }
