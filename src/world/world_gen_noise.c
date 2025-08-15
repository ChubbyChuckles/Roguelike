/* Noise & RNG utilities for world generation */
#include "world/world_gen_internal.h"
#include <math.h>

static unsigned int rng_state = 1u;
void rng_seed(unsigned int s){ if(!s) s = 1; rng_state = s; }
unsigned int rng_u32(void){ unsigned int x=rng_state; x^=x<<13; x^=x>>17; x^=x<<5; return rng_state=x; }
double rng_norm(void){ return (double)rng_u32() / (double)0xffffffffu; }
int rng_range(int lo,int hi){ return lo + (int)(rng_norm()*(double)(hi-lo+1)); }

static double hash2(int x,int y){ unsigned int h = (unsigned int)(x*374761393 + y*668265263); h = (h^(h>>13))*1274126177u; return (double)(h & 0xffffff)/ (double)0xffffff; }
static double lerp_d(double a,double b,double t){ return a + (b-a)*t; }
static double smoothstep_d(double t){ return t*t*(3.0-2.0*t); }
double value_noise(double x,double y){ int xi=(int)floor(x), yi=(int)floor(y); double tx=x-xi, ty=y-yi; double v00=hash2(xi,yi), v10=hash2(xi+1,yi), v01=hash2(xi,yi+1), v11=hash2(xi+1,yi+1); double sx=smoothstep_d(tx), sy=smoothstep_d(ty); double a=lerp_d(v00,v10,sx); double b=lerp_d(v01,v11,sx); return lerp_d(a,b,sy); }
double fbm(double x,double y,int octaves,double lacunarity,double gain){ double amp=1.0, freq=1.0, sum=0.0, norm=0.0; for(int i=0;i<octaves;i++){ sum += value_noise(x*freq,y*freq)*amp; norm += amp; freq *= lacunarity; amp *= gain; } return sum / (norm>0?norm:1.0); }
