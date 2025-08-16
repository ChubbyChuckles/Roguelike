#include "core/skill_maze.h"
#include "util/file_search.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* union-find helper (defined at file scope for C89 compatibility) */
static int rogue__uf_find(int* p,int x){ while(p[x]!=x){ p[x]=p[p[x]]; x=p[x]; } return x; }

static unsigned int xrng(unsigned int* s){ unsigned int x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }
static float frand(unsigned int* s){ return (float)(xrng(s) / (double)0xFFFFFFFFu); }

typedef struct MazeCfg { int rings; int approx; unsigned int seed; } MazeCfg;

static int parse_cfg_json(const char* buf, MazeCfg* cfg){ cfg->rings=5; cfg->approx=120; cfg->seed=1337u; const char* p=buf; while(*p){ while(*p && *p!='"') p++; if(!*p) break; const char* kstart=++p; while(*p && *p!='"') p++; if(!*p) break; char key[64]; int klen=(int)(p-kstart); if(klen>63) klen=63; memcpy(key,kstart,klen); key[klen]='\0'; p++; while(*p && *p!=':') p++; if(!*p) break; p++; while(*p && (unsigned char)*p<=32) p++; if(*p=='-' || (*p>='0'&&*p<='9')){ char* end=NULL; long v=strtol(p,&end,10); if(end!=p){ if(strcmp(key,"rings")==0) cfg->rings=(int)v; else if(strcmp(key,"approx_intersections")==0) cfg->approx=(int)v; else if(strcmp(key,"seed")==0) cfg->seed=(unsigned int)v; p=end; } } else { while(*p && *p!=',' && *p!='}') p++; }
        if(*p==',') p++; }
    if(cfg->rings<2) cfg->rings=2; if(cfg->approx<cfg->rings*8) cfg->approx = cfg->rings*8; return 1; }

int rogue_skill_maze_generate(const char* config_path, RogueSkillMaze* out_maze){ if(!out_maze) return 0; memset(out_maze,0,sizeof *out_maze); char resolved[640]; const char* path=config_path; FILE* f=NULL; if(path){
#if defined(_MSC_VER)
    if(fopen_s(&f,path,"rb")!=0) f=NULL;
#else
    f=fopen(path,"rb");
#endif
    if(!f){ const char* slash1=strrchr(path,'/'); const char* slash2=strrchr(path,'\\'); const char* last=slash2?((slash1&&slash1>slash2)?slash1:slash2):(slash1?slash1:NULL); const char* base= last? last+1: path; if(rogue_file_search_project(base,resolved,sizeof resolved)){
#if defined(_MSC_VER)
            if(fopen_s(&f,resolved,"rb")!=0) f=NULL;
#else
            f=fopen(resolved,"rb");
#endif
        }
    }
 }
 if(!f) return 0; fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET); char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); return 0; } fread(buf,1,(size_t)sz,f); buf[sz]='\0'; fclose(f); MazeCfg cfg; parse_cfg_json(buf,&cfg); free(buf);
 unsigned int rs = cfg.seed?cfg.seed:1337u;
 int rings=cfg.rings; int approx=cfg.approx; /* Build radial grid first */
 /* Determine segments per ring such that total intersections approximates target. Let inner ring have base segments then scale */
 int* segs=(int*)malloc(sizeof(int)*rings); if(!segs){ return 0; }
 int base = 8; segs[0]=base; int total=base; for(int r=1;r<rings;r++){ segs[r] = (int)(segs[r-1]* (1.0 + 0.35)); if(segs[r] < base) segs[r]=base; total += segs[r]; }
 /* Scale to approx */
 double scale = (double)approx / (double)total; total=0; for(int r=0;r<rings;r++){ segs[r] = (int)(segs[r]*scale); if(segs[r]< base) segs[r]=base; total += segs[r]; }
 /* Allocate nodes roughly total + random extra */
 int max_nodes = total + rings*4; RogueSkillMazeNode* nodes=(RogueSkillMazeNode*)malloc(sizeof(RogueSkillMazeNode)* (size_t)max_nodes); int node_count=0; if(!nodes){ free(segs); return 0; }
 /* Build ring nodes */
 float ring_gap = 55.0f; float center_x=0.0f, center_y=0.0f; for(int r=0;r<rings;r++){ float radius = 60.0f + r*ring_gap; int s=segs[r]; for(int i=0;i<s;i++){ float ang = (float)i / (float)s * 6.2831853f; nodes[node_count].x = center_x + cosf(ang)*radius; nodes[node_count].y = center_y + sinf(ang)*radius; nodes[node_count].ring=r+1; nodes[node_count].a=-1; nodes[node_count].b=-1; node_count++; } }
 /* Carve maze: we treat ring arcs and radial spokes as potential edges, then perform randomized DFS to keep some edges (circular maze). */
 int max_edges = node_count * 4; RogueSkillMazeEdge* edges=(RogueSkillMazeEdge*)malloc(sizeof(RogueSkillMazeEdge)*(size_t)max_edges); int edge_count=0; if(!edges){ free(nodes); free(segs); return 0; }
 /* Precompute ring offsets for indexing */
 int* ring_offset=(int*)malloc(sizeof(int)*rings); int acc=0; for(int r=0;r<rings;r++){ ring_offset[r]=acc; acc+=segs[r]; }
 /* Build adjacency candidate list */
 int candidates_cap = max_edges; int cand_count=0; typedef struct { int a,b; float w; } Cand; Cand* cands=(Cand*)malloc(sizeof(Cand)* (size_t)candidates_cap);
 for(int r=0;r<rings;r++){ int s=segs[r]; int start=ring_offset[r]; for(int i=0;i<s;i++){ int a=start+i; int b=start+ ( (i+1)%s ); if(cand_count<candidates_cap){ cands[cand_count].a=a; cands[cand_count].b=b; cands[cand_count].w=frand(&rs); cand_count++; } } }
 /* Radial spokes between rings (pick subset) */
 for(int r=0;r<rings-1;r++){ int sCur=segs[r]; int sNext=segs[r+1]; int startCur=ring_offset[r]; int startNext=ring_offset[r+1]; int spokes = sCur < sNext ? sCur : sNext; for(int i=0;i<spokes;i++){ int a=startCur + i; double pos = (double)i * (double)sNext / (double)sCur; int rel = (int)(pos + 0.5); if(rel>=sNext) rel = sNext-1; int b=startNext + rel; if(a<0||a>=node_count||b<0||b>=node_count) continue; if(cand_count<candidates_cap){ cands[cand_count].a=a; cands[cand_count].b=b; cands[cand_count].w=frand(&rs); cand_count++; } } }
 /* Shuffle candidates by weight */
 for(int i=0;i<cand_count;i++){ for(int j=i+1;j<cand_count;j++){ if(cands[j].w < cands[i].w){ Cand tmp=cands[i]; cands[i]=cands[j]; cands[j]=tmp; } } }
 /* Simple union-find */
 int* parent=(int*)malloc(sizeof(int)*node_count); if(!parent){ free(cands); free(ring_offset); free(segs); free(nodes); free(edges); return 0; }
 for(int i=0;i<node_count;i++) parent[i]=i;
 for(int i=0;i<cand_count;i++){ int a=cands[i].a; int b=cands[i].b; if(a<0||a>=node_count||b<0||b>=node_count) continue; int pa=rogue__uf_find(parent,a); int pb=rogue__uf_find(parent,b); if(pa!=pb || (frand(&rs)<0.25f && edge_count<node_count*3)){ if(edge_count<max_edges){ edges[edge_count].from=a; edges[edge_count].to=b; edge_count++; if(pa!=pb) parent[pa]=pb; } } }
 free(parent); free(cands); free(ring_offset); free(segs);
 out_maze->nodes=nodes; out_maze->node_count=node_count; out_maze->edges=edges; out_maze->edge_count=edge_count; out_maze->rings=rings; return 1; }

void rogue_skill_maze_free(RogueSkillMaze* m){ if(!m) return; free(m->nodes); free(m->edges); m->nodes=NULL; m->edges=NULL; m->node_count=m->edge_count=0; m->rings=0; }
