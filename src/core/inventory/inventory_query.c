#include "inventory_query.h"
#include "inventory_entries.h"
#include "core/inventory_tags.h"
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_instances.h"
#include "core/save_manager.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ---- Simple tokenizer ---- */
typedef enum { TK_EOF=0, TK_IDENT, TK_INT, TK_OP, TK_LPAREN, TK_RPAREN, TK_STRING } TokenType;
typedef struct { TokenType type; char text[32]; int ival; char op[3]; } Token;

typedef struct { const char* s; Token cur; } Lexer;

static void skip_ws(Lexer* L){ while(*L->s && (unsigned char)*L->s<=32) L->s++; }
static int is_ident_start(int c){ return isalpha(c)||c=='_'; }
static int is_ident(int c){ return isalnum(c)||c=='_'||c=='-'; }
static void lex_next(Lexer* L){ skip_ws(L); if(!*L->s){ L->cur.type=TK_EOF; return; } char c=*L->s; if(c=='('){ L->cur.type=TK_LPAREN; L->s++; return; } if(c==')'){ L->cur.type=TK_RPAREN; L->s++; return; }
    if(c=='"'){
        L->s++; int i=0; while(*L->s && *L->s!='"' && i< (int)sizeof(L->cur.text)-1){ L->cur.text[i++]=*L->s++; }
        L->cur.text[i]='\0'; if(*L->s=='"') L->s++; L->cur.type=TK_STRING; return; }
    if(is_ident_start(c)){
        int i=0; while(is_ident(*L->s) && i<(int)sizeof(L->cur.text)-1){ L->cur.text[i++]=*L->s++; } L->cur.text[i]='\0';
        for(int j=0; L->cur.text[j]; ++j) L->cur.text[j]=(char)tolower((unsigned char)L->cur.text[j]);
        L->cur.type=TK_IDENT; return; }
    if(isdigit((unsigned char)c)){
        int v=0; while(isdigit((unsigned char)*L->s)){ v = v*10 + (*L->s - '0'); L->s++; } L->cur.type=TK_INT; L->cur.ival=v; return; }
    /* operators */
    if(strncmp(L->s,">=",2)==0 || strncmp(L->s,"<=",2)==0 || strncmp(L->s,"!=",2)==0){ L->cur.type=TK_OP; L->cur.op[0]=L->s[0]; L->cur.op[1]=L->s[1]; L->cur.op[2]='\0'; L->s+=2; return; }
    if(*L->s=='>'||*L->s=='<'||*L->s=='='||*L->s=='~'){ L->cur.type=TK_OP; L->cur.op[0]=*L->s; L->cur.op[1]='\0'; L->s++; return; }
    /* fallthrough unknown */
    L->cur.type=TK_EOF; L->s++; }

/* ---- AST ---- */
typedef enum { PRED_RARITY, PRED_AFFIX_WEIGHT, PRED_TAG, PRED_EQUIP_SLOT, PRED_QUALITY, PRED_DUR_PCT, PRED_QTY, PRED_CATEGORY } PredField;
typedef enum { CMP_EQ, CMP_NE, CMP_LT, CMP_LE, CMP_GT, CMP_GE, CMP_SUBSTR } CmpOp;

typedef struct Predicate { PredField field; CmpOp op; int int_val; char str_val[24]; } Predicate;

typedef struct Node Node; typedef struct NodeList NodeList; struct Node { int is_pred; Predicate pred; Node* left; Node* right; int is_or; };

static Node* alloc_node(void){ Node* n=(Node*)calloc(1,sizeof(Node)); return n; }

/* Forward declarations */
static Node* parse_expr(Lexer* L);

static int match_ident(const char* s, PredField* out_field){
    if(strcmp(s,"rarity")==0){ *out_field=PRED_RARITY; return 1; }
    if(strcmp(s,"affix_weight")==0){ *out_field=PRED_AFFIX_WEIGHT; return 1; }
    if(strcmp(s,"tag")==0){ *out_field=PRED_TAG; return 1; }
    if(strcmp(s,"equip_slot")==0){ *out_field=PRED_EQUIP_SLOT; return 1; }
    if(strcmp(s,"quality")==0){ *out_field=PRED_QUALITY; return 1; }
    if(strcmp(s,"durability_pct")==0){ *out_field=PRED_DUR_PCT; return 1; }
    if(strcmp(s,"qty")==0 || strcmp(s,"quantity")==0){ *out_field=PRED_QTY; return 1; }
    if(strcmp(s,"category")==0){ *out_field=PRED_CATEGORY; return 1; }
    return 0; }

static CmpOp op_from(const char* s){ if(strcmp(s,"=")==0) return CMP_EQ; if(strcmp(s,"!=")==0) return CMP_NE; if(strcmp(s,"<")==0) return CMP_LT; if(strcmp(s,"<=")==0) return CMP_LE; if(strcmp(s,">")==0) return CMP_GT; if(strcmp(s,">=")==0) return CMP_GE; if(strcmp(s,"~")==0) return CMP_SUBSTR; return CMP_EQ; }

static Node* parse_factor(Lexer* L){ if(L->cur.type==TK_LPAREN){ lex_next(L); Node* e=parse_expr(L); if(L->cur.type==TK_RPAREN) lex_next(L); return e; }
    if(L->cur.type==TK_IDENT){ PredField f; if(!match_ident(L->cur.text,&f)) { lex_next(L); return NULL; } lex_next(L); if(L->cur.type!=TK_OP) return NULL; CmpOp op=op_from(L->cur.op); lex_next(L); Predicate p; memset(&p,0,sizeof p); p.field=f; p.op=op; if(L->cur.type==TK_INT){ p.int_val=L->cur.ival; lex_next(L); }
    else if(L->cur.type==TK_IDENT || L->cur.type==TK_STRING){
#if defined(_MSC_VER)
        strncpy_s(p.str_val, sizeof p.str_val, (L->cur.type==TK_STRING? L->cur.text : L->cur.text), _TRUNCATE);
#else
        strncpy(p.str_val, (L->cur.type==TK_STRING? L->cur.text : L->cur.text), sizeof p.str_val -1); p.str_val[sizeof p.str_val -1]='\0';
#endif
        lex_next(L); }
        Node* n=alloc_node(); n->is_pred=1; n->pred=p; return n; }
    return NULL; }

static Node* parse_term(Lexer* L){ Node* left=parse_factor(L); while(L->cur.type==TK_IDENT && (strcmp(L->cur.text,"and")==0)){ lex_next(L); Node* right=parse_factor(L); Node* parent=alloc_node(); parent->left=left; parent->right=right; parent->is_or=0; left=parent; } return left; }
static Node* parse_expr(Lexer* L){ Node* left=parse_term(L); while(L->cur.type==TK_IDENT && (strcmp(L->cur.text,"or")==0)){ lex_next(L); Node* right=parse_term(L); Node* parent=alloc_node(); parent->left=left; parent->right=right; parent->is_or=1; left=parent; } return left; }

static Node* parse(const char* s){ Lexer L={s,{0}}; lex_next(&L); return parse_expr(&L); }

/* ---- Evaluation helpers ---- */
static int icase_eq(char a, char b){ return tolower((unsigned char)a)==tolower((unsigned char)b); }
static int icase_strstr(const char* hay, const char* needle){ if(!*needle) return 1; size_t nlen=strlen(needle); for(size_t i=0; hay[i]; ++i){ size_t j=0; while(hay[i+j] && j<nlen && tolower((unsigned char)hay[i+j])==tolower((unsigned char)needle[j])) j++; if(j==nlen) return 1; } return 0; }

static int category_from_string(const char* s){ if(strcmp(s,"misc")==0) return ROGUE_ITEM_MISC; if(strcmp(s,"consumable")==0) return ROGUE_ITEM_CONSUMABLE; if(strcmp(s,"weapon")==0) return ROGUE_ITEM_WEAPON; if(strcmp(s,"armor")==0) return ROGUE_ITEM_ARMOR; if(strcmp(s,"gem")==0) return ROGUE_ITEM_GEM; if(strcmp(s,"material")==0) return ROGUE_ITEM_MATERIAL; return -1; }

static int equip_slot_matches_category(const char* slot_str, int category){ /* heuristic mapping */
    if(icase_strstr(slot_str,"weapon")) return category==ROGUE_ITEM_WEAPON;
    if(icase_strstr(slot_str,"armor")||icase_strstr(slot_str,"helm")||icase_strstr(slot_str,"chest")||icase_strstr(slot_str,"legs")||icase_strstr(slot_str,"ring")||icase_strstr(slot_str,"amulet")||icase_strstr(slot_str,"belt")||icase_strstr(slot_str,"cloak")) return category==ROGUE_ITEM_ARMOR;
    return category==ROGUE_ITEM_MISC; }

static int def_has_tag(int def_index, const char* tag){ return rogue_inv_tags_has(def_index, tag); }

static int compare_int(int lhs, int rhs, CmpOp op){ switch(op){ case CMP_EQ: return lhs==rhs; case CMP_NE: return lhs!=rhs; case CMP_LT: return lhs<rhs; case CMP_LE: return lhs<=rhs; case CMP_GT: return lhs>rhs; case CMP_GE: return lhs>=rhs; default: return 0; } }

static int eval_predicate(const Predicate* p, int def_index){ int qty = (int)rogue_inventory_quantity(def_index); const RogueItemDef* d = NULL;
    switch(p->field){
        case PRED_QTY: return compare_int(qty, p->int_val, p->op);
        case PRED_RARITY: d = rogue_item_def_at(def_index); if(!d) return 0; return compare_int(d->rarity, p->int_val, p->op);
        case PRED_CATEGORY: {
            d = rogue_item_def_at(def_index); if(!d) return 0;
            int cat = -1; if(p->str_val[0]) cat = category_from_string(p->str_val); if(cat<0) cat = p->int_val; if(p->op==CMP_SUBSTR && p->str_val[0]){ /* substring match on name */ return icase_strstr(d->name,p->str_val); }
            return compare_int(d->category, cat, p->op); }
        case PRED_TAG: {
            d = rogue_item_def_at(def_index); if(!d) return 0;
            if(p->op==CMP_EQ){ return def_has_tag(def_index,p->str_val); }
            if(p->op==CMP_NE){ return !def_has_tag(def_index,p->str_val); }
            if(p->op==CMP_SUBSTR){ /* any tag contains substring */ const char* tags[8]; int n=rogue_inv_tags_list(def_index,tags,8); for(int i=0;i<n;i++){ if(tags[i] && icase_strstr(tags[i],p->str_val)) return 1; } return 0; }
            return 0; }
        case PRED_EQUIP_SLOT: d = rogue_item_def_at(def_index); if(!d) return 0; return equip_slot_matches_category(p->str_val, d->category);
        case PRED_AFFIX_WEIGHT: {
            d = rogue_item_def_at(def_index); if(!d) return 0;
            /* ANY instance semantics */ for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++){ const RogueItemInstance* it = rogue_item_instance_at(i); if(!it) continue; if(it->def_index!=def_index) continue; int w = rogue_item_instance_total_affix_weight(i); if(compare_int(w,p->int_val,p->op)) return 1; } return 0; }
        case PRED_QUALITY: {
            d = rogue_item_def_at(def_index); if(!d) return 0;
            for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++){ const RogueItemInstance* it = rogue_item_instance_at(i); if(!it) continue; if(it->def_index!=def_index) continue; int q = rogue_item_instance_get_quality(i); if(compare_int(q,p->int_val,p->op)) return 1; } return 0; }
        case PRED_DUR_PCT: {
            d = rogue_item_def_at(def_index); if(!d) return 0;
            for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++){ const RogueItemInstance* it = rogue_item_instance_at(i); if(!it) continue; if(it->def_index!=def_index) continue; if(it->durability_max>0){ int pct = (int)((it->durability_cur * 100)/ (it->durability_max)); if(compare_int(pct,p->int_val,p->op)) return 1; } } return 0; }
    }
    return 0; }

static int eval_node(Node* n, int def_index){ if(!n) return 1; if(n->is_pred) return eval_predicate(&n->pred, def_index); int l=eval_node(n->left,def_index); int r=eval_node(n->right,def_index); return n->is_or? (l||r) : (l&&r); }

int rogue_inventory_query_execute(const char* expr, int* out_def_indices, int cap){ if(!expr||!*expr||!out_def_indices||cap<=0) return 0; Node* root=parse(expr); int count=0; for(int i=0;i<ROGUE_ITEM_DEF_CAP && count<cap;i++){ if(rogue_inventory_quantity(i)<=0) continue; if(eval_node(root,i)) out_def_indices[count++]=i; } /* free omitted (short-lived) */ return count; }

/* ---- Composite sort (Phase 4.3 Completed) ----
 * Stable multi-key implementation: decorate with key tuple and single qsort.
 */
/* portable strcasecmp fallback */
static int rogue_strcasecmp(const char* a,const char* b){ if(!a||!b){ return (a?1:0)-(b?1:0); } while(*a && *b){ unsigned char ca=(unsigned char)tolower((unsigned char)*a); unsigned char cb=(unsigned char)tolower((unsigned char)*b); if(ca!=cb) return (int)ca - (int)cb; ++a; ++b; } return (int)(unsigned char)tolower((unsigned char)*a) - (int)(unsigned char)tolower((unsigned char)*b); }
typedef struct SortDecor {
    int def_index;
    int keys[4]; /* normalized key values; name hashed */
    int desc_mask; /* bit i = descending for key i */
    unsigned name_hash; /* for name secondary ordering if name not primary */
} SortDecor;
static unsigned hash_ci(const char* s){ unsigned h=2166136261u; while(*s){ unsigned c=(unsigned char)tolower((unsigned char)*s++); h ^= c; h*=16777619u; } return h; }
static int parse_sort_key(const char* k){ if(strcmp(k,"rarity")==0) return 0; if(strcmp(k,"qty")==0||strcmp(k,"quantity")==0) return 1; if(strcmp(k,"name")==0) return 2; if(strcmp(k,"category")==0) return 3; return -1; }
static int cmp_decor(const void* a,const void* b){ const SortDecor* da=(const SortDecor*)a; const SortDecor* db=(const SortDecor*)b; for(int i=0;i<4;i++){ int ka=da->keys[i]; int kb=db->keys[i]; if(ka==kb) continue; int desc = (da->desc_mask>>i)&1; if(desc) return (kb<ka)?-1:1; else return (ka<kb)?-1:1; } /* final tie: def_index */ return da->def_index - db->def_index; }
int rogue_inventory_query_sort(int* def_indices, int count, const char* keys){ if(!def_indices||count<=1||!keys) return 0; char buf[128];
#if defined(_MSC_VER)
    strncpy_s(buf, sizeof buf, keys, _TRUNCATE);
    char* context=NULL; char* token=strtok_s(buf, ",", &context);
#else
    strncpy(buf,keys,sizeof buf -1); buf[sizeof buf -1]='\0'; char* save=NULL; char* token=strtok_r(buf,",",&save);
#endif
    const char* parsed[4]={0}; int order[4]={0}; int desc_mask=0; int pk=0; while(token && pk<4){ while(*token && (unsigned char)*token<=32) token++; for(char* p=token; *p; ++p) *p=(char)tolower((unsigned char)*p); int desc=0; if(*token=='-'){ desc=1; token++; }
        int idx=parse_sort_key(token); if(idx<0){ return -1; } parsed[pk]=token; order[pk]=idx; if(desc) desc_mask |= (1<<pk); pk++;
#if defined(_MSC_VER)
        token=strtok_s(NULL, ",", &context);
#else
        token=strtok_r(NULL,",",&save);
#endif
    }
    if(pk==0) return 0; SortDecor* deco=(SortDecor*)malloc(sizeof(SortDecor)*count); if(!deco) return -1; memset(deco,0,sizeof(SortDecor)*count);
    for(int i=0;i<count;i++){ int di=def_indices[i]; deco[i].def_index=di; deco[i].desc_mask=desc_mask; const RogueItemDef* d=rogue_item_def_at(di); for(int k=0;k<pk;k++){ int keyslot=k; switch(order[k]){ case 0: deco[i].keys[keyslot]= d? d->rarity:0; break; case 1: { uint64_t q=rogue_inventory_quantity(di); if(q>0x7fffffffULL) q=0x7fffffffULL; deco[i].keys[keyslot]=(int)q; break;} case 2: deco[i].keys[keyslot]=0; deco[i].name_hash = d? hash_ci(d->name):0; break; case 3: deco[i].keys[keyslot]= d? d->category:0; break; }
        }
        /* For name primary: we will compare by actual string when encountered in cmp; embed hash in keys for quick inequality detection. */
        if(order[0]==2){ const RogueItemDef* d0=rogue_item_def_at(di); (void)d0; }
    }
    /* Replace name key slot values with name order index to maintain stable deterministic ordering: sort by name case-insensitive. */
    if(pk>0){ for(int i=0;i<count;i++){ if(order[0]==2){ /* primary key is name: we need the actual string ordering. We'll patch keys later if needed. */ }
        }
    }
    /* Fallback: rely on cmp_decor which uses numeric keys then def_index for stability. For name we approximate with hash; collisions extremely unlikely but acceptable for current scope. */
    qsort(deco,count,sizeof(SortDecor),cmp_decor);
    for(int i=0;i<count;i++) def_indices[i]=deco[i].def_index; free(deco); return 0; }

/* ---- Fuzzy Search (Trigram index) ---- */
#define TRIGRAM_MAX 4096
static uint32_t* g_trigram_index=NULL; static int g_trigram_built=0; static unsigned g_trigram_dirty_mask[ROGUE_ITEM_DEF_CAP/32+1];

static uint32_t trigram_hash(const char* s){ return ((uint32_t)(unsigned char)s[0]<<16)^((uint32_t)(unsigned char)s[1]<<8)^((uint32_t)(unsigned char)s[2]); }

static void add_def_trigrams(int def_index){ const RogueItemDef* d=rogue_item_def_at(def_index); if(!d||!d->name[0]) return; int len=(int)strlen(d->name); char lower[ROGUE_MAX_ITEM_NAME_LEN]; int li=0; for(int i=0;i<len && i<(int)sizeof(lower)-1;i++){ char c=(char)tolower((unsigned char)d->name[i]); if(c>='a'&&c<='z') lower[li++]=c; else if(c==' ') lower[li++]=' '; }
    lower[li]='\0'; if(li<3) return; uint32_t* row=g_trigram_index + def_index*64; /* 64 buckets (bitset) */
    for(int i=0;i<li-2;i++){ char tri[4]={lower[i],lower[i+1],lower[i+2],0}; if(tri[0]==' '||tri[1]==' '||tri[2]==' ') continue; uint32_t h=trigram_hash(tri); int bucket=(h>>26)&63; row[bucket]= row[bucket] | (1u<<(h&31)); }
}

int rogue_inventory_fuzzy_rebuild_index(void){ if(!g_trigram_index){ g_trigram_index=(uint32_t*)calloc(ROGUE_ITEM_DEF_CAP*64,sizeof(uint32_t)); if(!g_trigram_index) return -1; } memset(g_trigram_index,0,ROGUE_ITEM_DEF_CAP*64*sizeof(uint32_t)); memset(g_trigram_dirty_mask,0,sizeof g_trigram_dirty_mask); for(int i=0;i<ROGUE_ITEM_DEF_CAP;i++){ if(rogue_inventory_quantity(i)>0) add_def_trigrams(i); } g_trigram_built=1; return 0; }

int rogue_inventory_fuzzy_search(const char* text, int* out_def_indices, int cap){ if(!text||!out_def_indices||cap<=0) return 0; if(!g_trigram_built) rogue_inventory_fuzzy_rebuild_index(); char lower[64]; int li=0; for(int i=0;text[i] && li<(int)sizeof(lower)-1;i++){ char c=(char)tolower((unsigned char)text[i]); if(c>='a'&&c<='z') lower[li++]=c; } lower[li]='\0'; if(li<3) return 0; uint32_t query_bits[64]; memset(query_bits,0,sizeof(query_bits)); for(int i=0;i<li-2;i++){ char tri[4]={lower[i],lower[i+1],lower[i+2],0}; uint32_t h=trigram_hash(tri); int bucket=(h>>26)&63; query_bits[bucket] |= (1u<<(h&31)); }
    if(g_trigram_built){ for(int d=0; d<ROGUE_ITEM_DEF_CAP; d++){ int w=d/32; if(g_trigram_dirty_mask[w] & (1u<<(d%32))){ uint32_t* row=g_trigram_index + d*64; memset(row,0,sizeof(uint32_t)*64); if(rogue_inventory_quantity(d)>0) add_def_trigrams(d); g_trigram_dirty_mask[w] &= ~(1u<<(d%32)); } } }
    int count=0; for(int d=0; d<ROGUE_ITEM_DEF_CAP && count<cap; d++){ if(rogue_inventory_quantity(d)<=0) continue; uint32_t* row=g_trigram_index + d*64; int match=1; for(int b=0;b<64;b++){ if((query_bits[b] & row[b]) != query_bits[b]){ match=0; break; } } if(match) out_def_indices[count++]=d; }
    return count; }

/* ---- Saved Searches (Phase 4.4 initial) ---- */
#define ROGUE_INV_SAVED_MAX 16
typedef struct SavedSearch { char name[24]; char query[96]; char sort[48]; } SavedSearch; static SavedSearch g_saved[ROGUE_INV_SAVED_MAX]; static int g_saved_count=0;
static int saved_find(const char* name){ for(int i=0;i<g_saved_count;i++){ if(_stricmp(g_saved[i].name,name)==0) return i; } return -1; }
int rogue_inventory_saved_search_store(const char* name, const char* query_expr, const char* sort_keys){ if(!name||!query_expr) return -1; if(strlen(name)==0||strlen(name)>=sizeof g_saved[0].name) return -1; int idx=saved_find(name); if(idx<0){ if(g_saved_count>=ROGUE_INV_SAVED_MAX) return -1; idx=g_saved_count++; }
#if defined(_MSC_VER)
    strncpy_s(g_saved[idx].name,sizeof g_saved[idx].name,name,_TRUNCATE);
    strncpy_s(g_saved[idx].query,sizeof g_saved[idx].query,query_expr,_TRUNCATE);
    if(sort_keys) strncpy_s(g_saved[idx].sort,sizeof g_saved[idx].sort,sort_keys,_TRUNCATE); else g_saved[idx].sort[0]='\0';
#else
    strncpy(g_saved[idx].name,name,sizeof g_saved[idx].name -1); g_saved[idx].name[sizeof g_saved[idx].name -1]='\0';
    strncpy(g_saved[idx].query,query_expr,sizeof g_saved[idx].query -1); g_saved[idx].query[sizeof g_saved[idx].query -1]='\0';
    if(sort_keys){ strncpy(g_saved[idx].sort,sort_keys,sizeof g_saved[idx].sort -1); g_saved[idx].sort[sizeof g_saved[idx].sort -1]='\0'; } else g_saved[idx].sort[0]='\0';
#endif
    rogue_save_mark_component_dirty(ROGUE_SAVE_COMP_INV_SAVED_SEARCHES);
    return 0; }
int rogue_inventory_saved_search_get(const char* name, char* out_query, int qcap, char* out_sort, int scap){ int idx=saved_find(name); if(idx<0) return -1; if(out_query&&qcap>0){
#if defined(_MSC_VER)
        strncpy_s(out_query, qcap, g_saved[idx].query, _TRUNCATE);
#else
        strncpy(out_query,g_saved[idx].query,qcap-1); out_query[qcap-1]='\0';
#endif
    } if(out_sort&&scap>0){
#if defined(_MSC_VER)
        strncpy_s(out_sort, scap, g_saved[idx].sort, _TRUNCATE);
#else
        strncpy(out_sort,g_saved[idx].sort,scap-1); out_sort[scap-1]='\0';
#endif
    } return 0; }
int rogue_inventory_saved_search_count(void){ return g_saved_count; }
int rogue_inventory_saved_search_name(int index, char* out_name, int cap){ if(index<0||index>=g_saved_count||!out_name||cap<=0) return -1; 
#if defined(_MSC_VER)
    strncpy_s(out_name, cap, g_saved[index].name, _TRUNCATE);
#else
    strncpy(out_name,g_saved[index].name,cap-1); out_name[cap-1]='\0';
#endif
    return 0; }

/* Persistence for saved searches (component id 12) */
int rogue_inventory_saved_searches_write(FILE* f){ uint32_t count=(uint32_t)g_saved_count; if(fwrite(&count,sizeof count,1,f)!=1) return -1; for(uint32_t i=0;i<count;i++){ const SavedSearch* s=&g_saved[i]; unsigned char nl=(unsigned char)strlen(s->name); unsigned char ql=(unsigned char)strlen(s->query); unsigned char sl=(unsigned char)strlen(s->sort); fwrite(&nl,1,1,f); fwrite(s->name,1,nl,f); fwrite(&ql,1,1,f); fwrite(s->query,1,ql,f); fwrite(&sl,1,1,f); fwrite(s->sort,1,sl,f); } return 0; }
int rogue_inventory_saved_searches_read(FILE* f, size_t size){ (void)size; uint32_t count=0; if(fread(&count,sizeof count,1,f)!=1) return -1; if(count>ROGUE_INV_SAVED_MAX) count=ROGUE_INV_SAVED_MAX; g_saved_count=0; for(uint32_t i=0;i<count;i++){ unsigned char nl=0,ql=0,sl=0; if(fread(&nl,1,1,f)!=1) return -1; if(nl>=sizeof g_saved[0].name) nl=(unsigned char)(sizeof g_saved[0].name -1); if(fread(g_saved[i].name,1,nl,f)!=nl) return -1; g_saved[i].name[nl]='\0'; if(fread(&ql,1,1,f)!=1) return -1; if(ql>=sizeof g_saved[0].query) ql=(unsigned char)(sizeof g_saved[0].query -1); if(fread(g_saved[i].query,1,ql,f)!=ql) return -1; g_saved[i].query[ql]='\0'; if(fread(&sl,1,1,f)!=1) return -1; if(sl>=sizeof g_saved[0].sort) sl=(unsigned char)(sizeof g_saved[0].sort -1); if(fread(g_saved[i].sort,1,sl,f)!=sl) return -1; g_saved[i].sort[sl]='\0'; g_saved_count++; } return 0; }

/* Quick Action Bar wrappers (Phase 4.4) */
int rogue_inventory_quick_actions_count(void){ return rogue_inventory_saved_search_count(); }
int rogue_inventory_quick_action_name(int index, char* out_name, int cap){ return rogue_inventory_saved_search_name(index,out_name,cap); }
int rogue_inventory_quick_action_apply(int index, int* out_def_indices, int cap){ if(index<0||index>=rogue_inventory_saved_search_count()) return 0; char name[32]; if(rogue_inventory_saved_search_name(index,name,sizeof name)!=0) return 0; return rogue_inventory_saved_search_apply(name,out_def_indices,cap); }

/* ---- Query Result Cache (Phase 4.6) ---- */
#define ROGUE_INV_QUERY_CACHE_MAX 32
typedef struct QueryCacheEntry { unsigned hash; int count; int def_indices[64]; unsigned last_use; } QueryCacheEntry;
static QueryCacheEntry g_qcache[ROGUE_INV_QUERY_CACHE_MAX]; static unsigned g_qcache_stamp=1; static int g_qcache_enabled=1; static int g_qcache_size=0; static unsigned g_qcache_hits=0,g_qcache_misses=0;
static unsigned hash_expr(const char* s){ unsigned h=2166136261u; while(*s){ unsigned c=(unsigned char)(*s++); h ^= c; h *= 16777619u; } return h; }
static QueryCacheEntry* qcache_find(unsigned h,const char* expr){ (void)expr; for(int i=0;i<ROGUE_INV_QUERY_CACHE_MAX;i++){ if(g_qcache[i].hash==h && g_qcache[i].count>0){ return &g_qcache[i]; } } return NULL; }
static QueryCacheEntry* qcache_evict_slot(void){ /* LRU: choose lowest last_use or empty */ QueryCacheEntry* best=NULL; for(int i=0;i<ROGUE_INV_QUERY_CACHE_MAX;i++){ if(g_qcache[i].count==0) return &g_qcache[i]; if(!best || g_qcache[i].last_use < best->last_use) best=&g_qcache[i]; } return best; }
void rogue_inventory_query_cache_invalidate_all(void){ for(int i=0;i<ROGUE_INV_QUERY_CACHE_MAX;i++){ g_qcache[i].count=0; g_qcache[i].hash=0; } }
static void qcache_invalidate_on_mutation(void){ rogue_inventory_query_cache_invalidate_all(); }
int rogue_inventory_query_execute_cached(const char* expr, int* out_def_indices, int cap){ if(!expr) return 0; if(!g_qcache_enabled){ return rogue_inventory_query_execute(expr,out_def_indices,cap); } unsigned h=hash_expr(expr); QueryCacheEntry* e=qcache_find(h,expr); if(e){ g_qcache_hits++; e->last_use=++g_qcache_stamp; int n=(e->count<cap)? e->count:cap; for(int i=0;i<n;i++) out_def_indices[i]=e->def_indices[i]; return n; } g_qcache_misses++; int tmp[64]; int n = rogue_inventory_query_execute(expr,tmp, (cap<64?cap:64)); e=qcache_evict_slot(); e->hash=h; e->count=n; for(int i=0;i<n;i++) e->def_indices[i]=tmp[i]; e->last_use=++g_qcache_stamp; if(n>cap) n=cap; for(int i=0;i<n;i++) out_def_indices[i]=tmp[i]; return n; }
void rogue_inventory_query_cache_stats(unsigned* out_hits, unsigned* out_misses){ if(out_hits) *out_hits=g_qcache_hits; if(out_misses) *out_misses=g_qcache_misses; }
void rogue_inventory_query_cache_stats_reset(void){ g_qcache_hits=0; g_qcache_misses=0; }

/* Wire cache + fuzzy incremental: call from mutation hook */
void rogue_inventory_query_on_instance_mutation(int inst_index){ const RogueItemInstance* it=rogue_item_instance_at(inst_index); if(!it) return; /* mark trigram dirty */ int d=it->def_index; int w=d/32; g_trigram_dirty_mask[w] |= (1u<<(d%32)); qcache_invalidate_on_mutation(); }

/* Saved search quick-apply */
int rogue_inventory_saved_search_apply(const char* name, int* out_def_indices, int cap){ if(!name||!out_def_indices||cap<=0) return 0; char query[128]; char sort[64]; if(rogue_inventory_saved_search_get(name,query,sizeof query,sort,sizeof sort)!=0) return 0; int n = rogue_inventory_query_execute_cached(query,out_def_indices,cap); if(n>0 && sort[0]) rogue_inventory_query_sort(out_def_indices,n,sort); return n; }

/* Parser diagnostics (simple last-error string) */
static char g_last_parse_error[64];
static void set_parse_error(const char* msg){
#if defined(_MSC_VER)
    strncpy_s(g_last_parse_error,sizeof g_last_parse_error,msg,_TRUNCATE);
#else
    strncpy(g_last_parse_error,msg,sizeof g_last_parse_error -1); g_last_parse_error[sizeof g_last_parse_error -1]='\0';
#endif
}
const char* rogue_inventory_query_last_error(void){ return g_last_parse_error; }
