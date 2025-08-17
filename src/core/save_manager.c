#include "core/save_manager.h"
#include "core/persistence.h"
#include "core/app_state.h"
#include "core/skills.h"
#include "core/buffs.h"
#include "core/loot_instances.h"
#include "core/vendor.h"
#include "world/tilemap.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h> /* qsort, malloc */
#include <assert.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

static RogueSaveComponent g_components[ROGUE_SAVE_MAX_COMPONENTS];
static int g_component_count = 0;
static int g_initialized = 0;
static int g_migrations_registered = 0; /* ensure migrations registered once */

/* Migration registry (linear chain for now) */
static RogueSaveMigration g_migrations[16];
static int g_migration_count = 0;
static int g_durable_writes = 0; /* fsync/_commit toggle */
static int g_last_migration_steps = 0; /* metrics */
static int g_last_migration_failed = 0;
static double g_last_migration_ms = 0.0; /* milliseconds */
static int g_debug_json_dump = 0; /* Phase 3.2 debug export toggle */
static uint32_t g_active_write_version = 0; /* version of file currently being written */
static uint32_t g_active_read_version = 0;  /* version of file currently being read */
static int g_compress_enabled = 0; static int g_compress_min_bytes = 64; /* Phase 3.6 */
static uint32_t g_last_tamper_flags = 0; /* Phase 4.3 tamper flags */
static int g_last_recovery_used = 0; /* Phase 4.4 */
static const RogueSaveSignatureProvider* g_sig_provider = NULL; /* v9 */
uint32_t rogue_save_last_tamper_flags(void){ return g_last_tamper_flags; }
int rogue_save_last_recovery_used(void){ return g_last_recovery_used; }
int rogue_save_set_compression(int enabled, int min_bytes){ g_compress_enabled = enabled?1:0; if(min_bytes>0) g_compress_min_bytes=min_bytes; return 0; }
/* String interning table (Phase 3.5) */
#define ROGUE_SAVE_MAX_STRINGS 256
static char* g_intern_strings[ROGUE_SAVE_MAX_STRINGS];
static int g_intern_count = 0;

/* Numeric width compile-time assertions (Phase 3.3) */
#if defined(_MSC_VER)
static_assert(sizeof(uint16_t)==2, "uint16_t must be 2 bytes");
static_assert(sizeof(uint32_t)==4, "uint32_t must be 4 bytes");
static_assert(sizeof(uint64_t)==8, "uint64_t must be 8 bytes");
#else
_Static_assert(sizeof(uint16_t)==2, "uint16_t must be 2 bytes");
_Static_assert(sizeof(uint32_t)==4, "uint32_t must be 4 bytes");
_Static_assert(sizeof(uint64_t)==8, "uint64_t must be 8 bytes");
#endif

/* Unsigned LEB128 style varint (7 bits per byte) for counts & small ids (Phase 3.4) */
static int write_varuint(FILE* f, uint32_t v){
    while(v >= 0x80){ unsigned char b=(unsigned char)((v & 0x7Fu) | 0x80u); if(fwrite(&b,1,1,f)!=1) return -1; v >>= 7; }
    unsigned char b=(unsigned char)(v & 0x7Fu); if(fwrite(&b,1,1,f)!=1) return -1; return 0;
}

/* Minimal SHA256 (public domain style) for overall save footer (Phase 4.1) */
typedef struct { uint32_t h[8]; uint64_t len; unsigned char buf[64]; size_t buf_len; } RogueSHA256Ctx;
static uint32_t rs_rotr(uint32_t x, uint32_t n){ return (x>>n)|(x<<(32-n)); }
static void rogue_sha256_init(RogueSHA256Ctx* c){ static const uint32_t iv[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u}; memcpy(c->h,iv,sizeof iv); c->len=0; c->buf_len=0; }
static void rogue_sha256_block(RogueSHA256Ctx* c, const unsigned char* p){ static const uint32_t K[64]={0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u}; uint32_t w[64]; for(int i=0;i<16;i++){ w[i]=(uint32_t)p[4*i]<<24|(uint32_t)p[4*i+1]<<16|(uint32_t)p[4*i+2]<<8|(uint32_t)p[4*i+3]; } for(int i=16;i<64;i++){ uint32_t s0=rs_rotr(w[i-15],7)^rs_rotr(w[i-15],18)^(w[i-15]>>3); uint32_t s1=rs_rotr(w[i-2],17)^rs_rotr(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; } uint32_t a=c->h[0],b=c->h[1],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7],cc=c->h[2]; for(int i=0;i<64;i++){ uint32_t S1=rs_rotr(e,6)^rs_rotr(e,11)^rs_rotr(e,25); uint32_t ch=(e & f) ^ (~e & g); uint32_t temp1=h+S1+ch+K[i]+w[i]; uint32_t S0=rs_rotr(a,2)^rs_rotr(a,13)^rs_rotr(a,22); uint32_t maj=(a & b) ^ (a & cc) ^ (b & cc); uint32_t temp2=S0+maj; h=g; g=f; f=e; e=d+temp1; d=cc; cc=b; b=a; a=temp1+temp2; } c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d; c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h; }
static void rogue_sha256_update(RogueSHA256Ctx* c, const void* data, size_t len){ const unsigned char* p=data; c->len += len; while(len>0){ size_t space=64-c->buf_len; size_t take = len<space?len:space; memcpy(c->buf+c->buf_len,p,take); c->buf_len+=take; p+=take; len-=take; if(c->buf_len==64){ rogue_sha256_block(c,c->buf); c->buf_len=0; } } }
static void rogue_sha256_final(RogueSHA256Ctx* c, unsigned char out[32]){ uint64_t bit_len = c->len*8; unsigned char pad=0x80; rogue_sha256_update(c,&pad,1); unsigned char z=0; while(c->buf_len!=56){ rogue_sha256_update(c,&z,1); } unsigned char len_be[8]; for(int i=0;i<8;i++){ len_be[7-i]=(unsigned char)(bit_len>>(i*8)); } rogue_sha256_update(c,len_be,8); for(int i=0;i<8;i++){ out[4*i]=(unsigned char)(c->h[i]>>24); out[4*i+1]=(unsigned char)(c->h[i]>>16); out[4*i+2]=(unsigned char)(c->h[i]>>8); out[4*i+3]=(unsigned char)(c->h[i]); } }
static unsigned char g_last_sha256[32];
const unsigned char* rogue_save_last_sha256(void){ return g_last_sha256; }
void rogue_save_last_sha256_hex(char out[65]){ static const char* hx="0123456789abcdef"; for(int i=0;i<32;i++){ out[2*i]=hx[g_last_sha256[i]>>4]; out[2*i+1]=hx[g_last_sha256[i]&0xF]; } out[64]='\0'; }
int rogue_save_set_signature_provider(const RogueSaveSignatureProvider* prov){ g_sig_provider = prov; return 0; }
const RogueSaveSignatureProvider* rogue_save_get_signature_provider(void){ return g_sig_provider; }

/* Replay hash state (v8). We capture event tuples (frame,action,value) and hash them in order. */
typedef struct { uint32_t frame; uint32_t action; int32_t value; } RogueReplayEvent;
#define ROGUE_REPLAY_MAX_EVENTS 4096
static RogueReplayEvent g_replay_events[ROGUE_REPLAY_MAX_EVENTS];
static uint32_t g_replay_event_count = 0;
static unsigned char g_last_replay_hash[32];
void rogue_save_replay_reset(void){ g_replay_event_count=0; memset(g_last_replay_hash,0,32); }
int rogue_save_replay_record_input(uint32_t frame, uint32_t action_code, int32_t value){ if(g_replay_event_count>=ROGUE_REPLAY_MAX_EVENTS) return -1; g_replay_events[g_replay_event_count].frame=frame; g_replay_events[g_replay_event_count].action=action_code; g_replay_events[g_replay_event_count].value=value; g_replay_event_count++; return 0; }
static void rogue_replay_compute_hash(void){ RogueSHA256Ctx sha; rogue_sha256_init(&sha); rogue_sha256_update(&sha,g_replay_events,g_replay_event_count*sizeof(RogueReplayEvent)); rogue_sha256_final(&sha,g_last_replay_hash); }
const unsigned char* rogue_save_last_replay_hash(void){ return g_last_replay_hash; }
void rogue_save_last_replay_hash_hex(char out[65]){ static const char* hx="0123456789abcdef"; for(int i=0;i<32;i++){ out[2*i]=hx[g_last_replay_hash[i]>>4]; out[2*i+1]=hx[g_last_replay_hash[i]&0xF]; } out[64]='\0'; }
uint32_t rogue_save_last_replay_event_count(void){ return g_replay_event_count; }
static int read_varuint(FILE* f, uint32_t* out){
    uint32_t result=0; int shift=0; for(int i=0;i<5;i++){ int c=fgetc(f); if(c==EOF) return -1; unsigned char b=(unsigned char)c; result |= (uint32_t)(b & 0x7F) << shift; if(!(b & 0x80)){ *out=result; return 0; } shift += 7; }
    return -1;
}

int rogue_save_last_migration_steps(void){ return g_last_migration_steps; }
int rogue_save_last_migration_failed(void){ return g_last_migration_failed; }
double rogue_save_last_migration_ms(void){ return g_last_migration_ms; }

/* Simple CRC32 (polynomial 0xEDB88320) */
uint32_t rogue_crc32(const void* data, size_t len){
    static uint32_t table[256]; static int have=0; if(!have){ for(uint32_t i=0;i<256;i++){ uint32_t c=i; for(int k=0;k<8;k++) c = (c & 1)? 0xEDB88320u ^ (c>>1) : (c>>1); table[i]=c; } have=1; }
    uint32_t crc=0xFFFFFFFFu; const unsigned char* p=(const unsigned char*)data; for(size_t i=0;i<len;i++) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc>>8); return crc ^ 0xFFFFFFFFu;
}

static const RogueSaveComponent* find_component(int id){ for(int i=0;i<g_component_count;i++) if(g_components[i].id==id) return &g_components[i]; return NULL; }

void rogue_save_manager_register(const RogueSaveComponent* comp){ if(g_component_count>=ROGUE_SAVE_MAX_COMPONENTS) return; if(find_component(comp->id)) return; g_components[g_component_count++] = *comp; }

static int cmp_comp(const void* a, const void* b){ const RogueSaveComponent*ca=a; const RogueSaveComponent*cb=b; return (ca->id - cb->id); }

/* Core migration registration (defined later) */
static void rogue_register_core_migrations(void);
void rogue_save_manager_init(void){
    if(!g_initialized){
        g_initialized=1; /* ensure deterministic order */
    }
    if(!g_migrations_registered){
        rogue_register_core_migrations();
        g_migrations_registered=1;
    }
}

void rogue_save_register_migration(const RogueSaveMigration* mig){ if(g_migration_count< (int)(sizeof g_migrations/sizeof g_migrations[0])) g_migrations[g_migration_count++]=*mig; }

void rogue_save_manager_reset_for_tests(void){ g_component_count=0; g_initialized=0; g_migration_count=0; g_migrations_registered=0; g_last_migration_steps=0; g_last_migration_failed=0; g_last_migration_ms=0.0; }
int rogue_save_set_debug_json(int enabled){ g_debug_json_dump = enabled?1:0; return 0; }
int rogue_save_intern_string(const char* s){ if(!s) return -1; for(int i=0;i<g_intern_count;i++) if(strcmp(g_intern_strings[i],s)==0) return i; if(g_intern_count>=ROGUE_SAVE_MAX_STRINGS) return -1; size_t len=strlen(s); char* dup=(char*)malloc(len+1); if(!dup) return -1; memcpy(dup,s,len+1); g_intern_strings[g_intern_count]=dup; return g_intern_count++; }
const char* rogue_save_intern_get(int index){ if(index<0||index>=g_intern_count) return NULL; return g_intern_strings[index]; }
int rogue_save_intern_count(void){ return g_intern_count; }

static char slot_path[128];
static const char* build_slot_path(int slot){ snprintf(slot_path,sizeof slot_path,"save_slot_%d.sav", slot); return slot_path; }
static char autosave_path[128];
static const char* build_autosave_path(int logical){ int ring = logical % ROGUE_AUTOSAVE_RING; snprintf(autosave_path,sizeof autosave_path,"autosave_%d.sav", ring); return autosave_path; }

int rogue_save_format_endianness_is_le(void){ uint32_t x=0x01020304u; unsigned char* p=(unsigned char*)&x; return p[0]==0x04; }

static int internal_save_to(const char* final_path){
    qsort(g_components, g_component_count, sizeof(RogueSaveComponent), cmp_comp);
    char tmp_path[160]; snprintf(tmp_path,sizeof tmp_path,"./tmp_save_%u.tmp", (unsigned)time(NULL));
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f,tmp_path,"w+b");
#else
    f=fopen(tmp_path,"w+b");
#endif
    if(!f) return -2;
    RogueSaveDescriptor desc={0}; desc.version=ROGUE_SAVE_FORMAT_VERSION; desc.timestamp_unix=(uint32_t)time(NULL); g_active_write_version = desc.version;
    if(fwrite(&desc,sizeof desc,1,f)!=1){ fclose(f); return -3; }
    desc.section_count=0; desc.component_mask=0;
    /* Write all sections */
    for(int i=0;i<g_component_count;i++){
        const RogueSaveComponent* c=&g_components[i]; long start=ftell(f);
        if(desc.version>=3){
            uint16_t id16=(uint16_t)c->id; uint32_t size_placeholder32=0;
            if(fwrite(&id16,sizeof id16,1,f)!=1){ fclose(f); return -4; }
            if(fwrite(&size_placeholder32,sizeof size_placeholder32,1,f)!=1){ fclose(f); return -4; }
            long payload_start=ftell(f);
            if(desc.version>=6 && g_compress_enabled){
                FILE* mem=NULL;
#if defined(_MSC_VER)
                tmpfile_s(&mem);
#else
                mem = tmpfile();
#endif
                if(!mem){ fclose(f); return -5; }
                if(c->write_fn(mem)!=0){ fclose(mem); fclose(f); return -5; }
                fflush(mem); fseek(mem,0,SEEK_END); long usz=ftell(mem); fseek(mem,0,SEEK_SET);
                unsigned char* ubuf=(unsigned char*)malloc((size_t)usz); if(!ubuf){ fclose(mem); fclose(f); return -5; }
                fread(ubuf,1,(size_t)usz,mem); fclose(mem);
                unsigned char* cbuf=(unsigned char*)malloc((size_t)usz*2+16); if(!cbuf){ free(ubuf); fclose(f); return -5; }
                size_t ci=0; for(long p=0;p<usz;){ unsigned char b=ubuf[p]; int run=1; while(p+run<usz && ubuf[p+run]==b && run<255) run++; cbuf[ci++]=b; cbuf[ci++]=(unsigned char)run; p+=run; }
                int use_compressed = (usz >= g_compress_min_bytes && ci < (size_t)usz);
                if(use_compressed){
                    long payload_pos=ftell(f); fwrite(&usz,sizeof(uint32_t),1,f); if(fwrite(cbuf,1,ci,f)!=(size_t)ci){ free(cbuf); free(ubuf); fclose(f); return -5; }
                    long end=ftell(f); uint32_t stored_size = (uint32_t)(end - payload_pos);
                    uint32_t header_size_field = stored_size | 0x80000000u;
                    fseek(f,start+sizeof(uint16_t),SEEK_SET); fwrite(&header_size_field,sizeof header_size_field,1,f); fseek(f,end,SEEK_SET);
                } else {
                    if(fwrite(ubuf,1,(size_t)usz,f)!=(size_t)usz){ free(cbuf); free(ubuf); fclose(f); return -5; }
                    long end=ftell(f); uint32_t section_size=(uint32_t)(end - payload_start);
                    fseek(f,start+sizeof(uint16_t),SEEK_SET); fwrite(&section_size,sizeof section_size,1,f); fseek(f,end,SEEK_SET);
                }
                free(cbuf); free(ubuf);
            } else {
                if(c->write_fn(f)!=0){ fclose(f); return -5; }
                long end=ftell(f); uint32_t section_size=(uint32_t)(end - payload_start);
                fseek(f,start+sizeof(uint16_t),SEEK_SET); fwrite(&section_size,sizeof section_size,1,f); fseek(f,end,SEEK_SET);
            }
            if(desc.version>=7){
                long end_after_payload = ftell(f); long payload_size = end_after_payload - payload_start; if(payload_size<0){ fclose(f); return -14; }
                fseek(f,payload_start,SEEK_SET); unsigned char* tmp=(unsigned char*)malloc((size_t)payload_size); if(!tmp){ fclose(f); return -14; }
                if(fread(tmp,1,(size_t)payload_size,f)!=(size_t)payload_size){ free(tmp); fclose(f); return -14; }
                uint32_t sec_crc = rogue_crc32(tmp,(size_t)payload_size); free(tmp);
                fseek(f,end_after_payload,SEEK_SET); fwrite(&sec_crc,sizeof sec_crc,1,f);
            }
        } else {
            uint32_t id=(uint32_t)c->id, size_placeholder=0; if(fwrite(&id,sizeof id,1,f)!=1){ fclose(f); return -4; }
            if(fwrite(&size_placeholder,sizeof size_placeholder,1,f)!=1){ fclose(f); return -4; }
            long payload_start=ftell(f); if(c->write_fn(f)!=0){ fclose(f); return -5; }
            long end=ftell(f); uint32_t section_size=(uint32_t)(end - payload_start);
            fseek(f,start+sizeof(uint32_t),SEEK_SET); fwrite(&section_size,sizeof section_size,1,f); fseek(f,end,SEEK_SET);
        }
        desc.section_count++; desc.component_mask |= (1u<<c->id);
    }
    /* Marks end of payload (excludes integrity footers) */
    long payload_end = ftell(f);
    /* Compute descriptor CRC (over payload only) */
    size_t crc_region = (size_t)(payload_end - (long)sizeof desc);
    fseek(f,sizeof desc,SEEK_SET);
    unsigned char chunk[512]; uint32_t crc=0xFFFFFFFFu; static uint32_t table[256]; static int have=0; if(!have){ for(uint32_t i=0;i<256;i++){ uint32_t c=i; for(int k=0;k<8;k++) c = (c & 1)? 0xEDB88320u ^ (c>>1) : (c>>1); table[i]=c; } have=1; }
    size_t remaining=crc_region; while(remaining>0){ size_t to_read= remaining>sizeof(chunk)?sizeof(chunk):remaining; size_t n=fread(chunk,1,to_read,f); if(n!=to_read){ fclose(f); return -13; } for(size_t i=0;i<n;i++){ crc = table[(crc ^ chunk[i]) & 0xFF] ^ (crc>>8); } remaining-=n; }
    desc.checksum = crc ^ 0xFFFFFFFFu;
    /* SHA256 footer (v7+) over same region */
    if(desc.version>=7){ RogueSHA256Ctx sha; rogue_sha256_init(&sha); fseek(f,sizeof desc,SEEK_SET); long bytes_left = payload_end - (long)sizeof desc; while(bytes_left>0){ long take = bytes_left > (long)sizeof(chunk)? (long)sizeof(chunk): bytes_left; size_t n=fread(chunk,1,(size_t)take,f); if(n!=(size_t)take){ fclose(f); return -15; } rogue_sha256_update(&sha,chunk,n); bytes_left -= take; }
        unsigned char digest[32]; rogue_sha256_final(&sha,digest); memcpy(g_last_sha256,digest,32); const char magic[4]={'S','H','3','2'}; fseek(f,0,SEEK_END); fwrite(magic,1,4,f); fwrite(digest,1,32,f);
        /* Optional signature (v9+) signs payload + SHA footer */
        if(desc.version>=9 && g_sig_provider){ fseek(f,sizeof desc,SEEK_SET); long sign_region_end = ftell(f) + (payload_end - (long)sizeof desc) + 4 + 32; /* after writing SHA footer */
            long total_for_sig = sign_region_end - (long)sizeof desc; unsigned char* sig_src=(unsigned char*)malloc((size_t)total_for_sig); if(!sig_src){ fclose(f); return -16; }
            size_t rd=fread(sig_src,1,(size_t)total_for_sig,f); if(rd!=(size_t)total_for_sig){ free(sig_src); fclose(f); return -16; }
            unsigned char sigbuf[1024]; uint32_t slen=sizeof sigbuf; if(g_sig_provider->sign(sig_src,(size_t)total_for_sig,sigbuf,&slen)!=0){ free(sig_src); fclose(f); return -16; }
            free(sig_src); const char smagic[4]={'S','G','N','0'}; fwrite(&slen,sizeof(uint16_t),1,f); fwrite(smagic,1,4,f); fwrite(sigbuf,1,slen,f); }
    }
    long file_end = ftell(f); desc.total_size = (uint64_t)file_end;
    /* Rewrite descriptor with final fields */
    fseek(f,0,SEEK_SET); fwrite(&desc,sizeof desc,1,f); fflush(f);
#if defined(_WIN32)
    if(g_durable_writes){ int fd=_fileno(f); if(fd!=-1) _commit(fd); }
#else
    if(g_durable_writes){ int fd=fileno(f); if(fd!=-1) fsync(fd); }
#endif
    fclose(f);
#if defined(_MSC_VER)
    remove(final_path); rename(tmp_path, final_path);
#else
    rename(tmp_path, final_path);
#endif
    return 0;
}

int rogue_save_manager_save_slot(int slot_index){ if(slot_index<0 || slot_index>=ROGUE_SAVE_SLOT_COUNT) return -1; int rc=internal_save_to(build_slot_path(slot_index)); if(rc==0 && g_debug_json_dump){ char json_path[128]; snprintf(json_path,sizeof json_path,"save_slot_%d.json", slot_index); char buf[2048]; if(rogue_save_export_json(slot_index,buf,sizeof buf)==0){ FILE* jf=NULL; 
#if defined(_MSC_VER)
    fopen_s(&jf,json_path,"wb");
#else
    jf=fopen(json_path,"wb");
#endif
    if(jf){ fwrite(buf,1,strlen(buf),jf); fclose(jf);} }
    } return rc; }
int rogue_save_manager_autosave(int slot_index){ if(slot_index<0) slot_index=0; return internal_save_to(build_autosave_path(slot_index)); }
int rogue_save_manager_set_durable(int enabled){ g_durable_writes = enabled?1:0; return 0; }

/* Internal helper: validate & load entire save file (returns malloc buffer after header) */
static int load_and_validate(const char* path, RogueSaveDescriptor* out_desc, unsigned char** out_buf){
    g_last_tamper_flags = 0; /* reset per attempt */
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) return -2;
    if(fread(out_desc,sizeof *out_desc,1,f)!=1){ fclose(f); return -3; }
    long file_end=0; fseek(f,0,SEEK_END); file_end=ftell(f); if((uint64_t)file_end!=out_desc->total_size){ fclose(f); return -5; }
    size_t rest = file_end - (long)sizeof *out_desc; size_t footer_bytes=0; uint16_t sig_len=0; int has_sig=0;
    if(out_desc->version>=7){ footer_bytes += 4+32; if(out_desc->version>=9 && rest >= (size_t)(4+32+6)){ /* probe signature */ long sig_tail_pos = file_end - 6; fseek(f,sig_tail_pos,SEEK_SET); unsigned char tail[6]; if(fread(tail,1,6,f)==6){ memcpy(&sig_len,tail,2); if(memcmp(tail+2,"SGN0",4)==0){ long sig_start = sig_tail_pos - sig_len; if(sig_start >= (long)sizeof *out_desc + (long)(4+32) && sig_len < 4096){ footer_bytes += 2+4 + sig_len; has_sig=1; } } } }
    }
    if(rest < footer_bytes){ fclose(f); return -5; }
    size_t crc_region = rest - footer_bytes; fseek(f,sizeof *out_desc,SEEK_SET); unsigned char* buf=(unsigned char*)malloc(rest); if(!buf){ fclose(f); return -6; }
    size_t n=fread(buf,1,rest,f); if(n!=rest){ free(buf); fclose(f); return -6; }
    uint32_t crc=rogue_crc32(buf,crc_region); if(crc!=out_desc->checksum){ g_last_tamper_flags |= ROGUE_TAMPER_FLAG_DESCRIPTOR_CRC; free(buf); fclose(f); return -7; }
    if(out_desc->version>=7){ /* SHA footer lives immediately after crc_region */ if(memcmp(buf+crc_region,"SH32",4)!=0){ g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SHA256; free(buf); fclose(f); return ROGUE_SAVE_ERR_SHA256; } RogueSHA256Ctx sha; rogue_sha256_init(&sha); rogue_sha256_update(&sha,buf,crc_region); unsigned char dg[32]; rogue_sha256_final(&sha,dg); if(memcmp(dg,buf+crc_region+4,32)!=0){ g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SHA256; free(buf); fclose(f); return ROGUE_SAVE_ERR_SHA256; } memcpy(g_last_sha256,buf+crc_region+4,32);
        if(out_desc->version>=9 && has_sig){ const unsigned char* p = buf + crc_region + 4 + 32; uint16_t slen=0; memcpy(&slen,p,2); if(slen==sig_len && memcmp(p+2,"SGN0",4)==0){ if(g_sig_provider){ if(g_sig_provider->verify(buf,crc_region+4+32, p+6, slen)!=0){ g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SIGNATURE; free(buf); fclose(f); return ROGUE_SAVE_ERR_SHA256; } } } }
    }
    fclose(f); *out_buf=buf; return 0;
}

int rogue_save_for_each_section(int slot_index, RogueSaveSectionIterFn fn, void* user){
    if(slot_index<0||slot_index>=ROGUE_SAVE_SLOT_COUNT) return -1;
    RogueSaveDescriptor desc; unsigned char* buf=NULL;
    int rc=load_and_validate(build_slot_path(slot_index), &desc, &buf); if(rc!=0) return rc;
    unsigned char* p=buf; size_t total_payload = (size_t)(desc.total_size - sizeof desc);
    for(uint32_t s=0; s<desc.section_count; s++){
        if(desc.version>=3){
            if((size_t)(p-buf)+6 > total_payload){ free(buf); return -8; }
            uint16_t id16; memcpy(&id16,p,2); uint32_t size; memcpy(&size,p+2,4); p+=6;
            if((size_t)(p-buf)+size > total_payload){ free(buf); return -8; }
            if(fn){ int frc=fn(&desc,(uint32_t)id16,p,size,user); if(frc!=0){ free(buf); return frc; } }
            p+=size; if(desc.version>=7){ if((size_t)(p-buf)+4>total_payload){ free(buf); return -8; } p+=4; }
        } else {
            if((size_t)(p-buf)+8 > total_payload){ free(buf); return -8; }
            uint32_t id; memcpy(&id,p,4); uint32_t size; memcpy(&size,p+4,4); p+=8;
            if((size_t)(p-buf)+size > total_payload){ free(buf); return -8; }
            if(fn){ int frc=fn(&desc,id,p,size,user); if(frc!=0){ free(buf); return frc; } }
            p+=size;
        }
    }
    free(buf);
    return 0;
}

int rogue_save_export_json(int slot_index, char* out, size_t out_cap){
    if(!out||out_cap==0) return -1;
    RogueSaveDescriptor d; unsigned char* buf=NULL; int rc=load_and_validate(build_slot_path(slot_index), &d, &buf); if(rc!=0){ return rc; }
    int written=snprintf(out,out_cap,"{\n  \"version\":%u,\n  \"timestamp\":%u,\n  \"sections\":[", d.version,d.timestamp_unix);
    unsigned char* p=buf; size_t total_payload=(size_t)(d.total_size - sizeof d);
    for(uint32_t s=0; s<d.section_count && written<(int)out_cap; s++){
        uint32_t id=0; uint32_t size=0;
        if(d.version>=3){ if((size_t)(p-buf)+6>total_payload) break; uint16_t id16; memcpy(&id16,p,2); memcpy(&size,p+2,4); id=(uint32_t)id16; p+=6; }
        else { if((size_t)(p-buf)+8>total_payload) break; memcpy(&id,p,4); memcpy(&size,p+4,4); p+=8; }
    if((size_t)(p-buf)+size>total_payload) break; p+=size;
    written += snprintf(out+written, out_cap-written, "%s{\"id\":%u,\"size\":%u}", (s==0?"":","), id,size);
    }
    if(written<(int)out_cap) written += snprintf(out+written, out_cap-written, "]\n}\n");
    free(buf);
    if(written>=(int)out_cap) return -2; return 0;
}

int rogue_save_reload_component_from_slot(int slot_index, int component_id){
    if(slot_index<0||slot_index>=ROGUE_SAVE_SLOT_COUNT) return -1;
    const RogueSaveComponent* comp=find_component(component_id);
    if(!comp||!comp->read_fn) return -2;
    RogueSaveDescriptor d; unsigned char* buf=NULL; int rc=load_and_validate(build_slot_path(slot_index), &d, &buf); if(rc!=0) return rc;
    unsigned char* p=buf; size_t total_payload=(size_t)(d.total_size - sizeof d); int applied=-3;
    for(uint32_t s=0; s<d.section_count; s++){
        uint32_t id=0; uint32_t size=0;
        if(d.version>=3){ if((size_t)(p-buf)+6>total_payload) break; uint16_t id16; memcpy(&id16,p,2); memcpy(&size,p+2,4); id=(uint32_t)id16; p+=6; }
        else { if((size_t)(p-buf)+8>total_payload) break; memcpy(&id,p,4); memcpy(&size,p+4,4); p+=8; }
        if((size_t)(p-buf)+size>total_payload) break;
        if((int)id==component_id){
            const unsigned char* payload=p; char tmp[64]; snprintf(tmp,sizeof tmp,"_tmp_section_%d.bin", component_id);
            FILE* tf=NULL;
#if defined(_MSC_VER)
            fopen_s(&tf,tmp,"wb");
#else
            tf=fopen(tmp,"wb");
#endif
            if(!tf){ free(buf); return -4; }
            fwrite(payload,1,size,tf); fclose(tf);
#if defined(_MSC_VER)
            fopen_s(&tf,tmp,"rb");
#else
            tf=fopen(tmp,"rb");
#endif
            if(!tf){ free(buf); return -4; }
            comp->read_fn(tf,size); fclose(tf); remove(tmp); applied=0; break; }
        p+=size;
    }
    free(buf);
    return applied;
}

int rogue_save_manager_load_slot(int slot_index){
    if(slot_index<0 || slot_index>=ROGUE_SAVE_SLOT_COUNT) return -1; FILE* f=NULL;
#ifdef ROGUE_STRICT_ENDIAN
    if(!rogue_save_format_endianness_is_le()){ return -30; }
#endif
#if defined(_MSC_VER)
    fopen_s(&f, build_slot_path(slot_index), "rb");
#else
    f=fopen(build_slot_path(slot_index),"rb");
#endif
    if(!f){ return -2; }
    RogueSaveDescriptor desc; if(fread(&desc,sizeof desc,1,f)!=1){ fclose(f); return -3; } g_active_read_version = desc.version;
    /* naive validation */
    if(desc.version!=ROGUE_SAVE_FORMAT_VERSION){
        /* Load entire payload for potential in-place migration with rollback */
        long file_end_pos=0; fseek(f,0,SEEK_END); file_end_pos=ftell(f); long payload_size = file_end_pos - (long)sizeof desc; if(payload_size<0){ fclose(f); return -4; }
        unsigned char* payload=(unsigned char*)malloc((size_t)payload_size); if(!payload){ fclose(f); return -4; }
        fseek(f,sizeof desc,SEEK_SET); if(fread(payload,1,(size_t)payload_size,f)!=(size_t)payload_size){ free(payload); fclose(f); return -4; }
        unsigned char* rollback=(unsigned char*)malloc((size_t)payload_size); if(!rollback){ free(payload); fclose(f); return -4; }
        memcpy(rollback,payload,(size_t)payload_size);
        g_last_migration_steps=0; g_last_migration_failed=0; g_last_migration_ms=0.0; double t0=(double)clock();
        uint32_t cur = desc.version; int failure=0;
        while(cur < ROGUE_SAVE_FORMAT_VERSION){
            int advanced=0; for(int i=0;i<g_migration_count;i++) if(g_migrations[i].from_version==cur && g_migrations[i].to_version==cur+1){
                if(g_migrations[i].apply_fn){ int mrc=g_migrations[i].apply_fn(payload,(size_t)payload_size); if(mrc!=0){ failure=1; break; } }
                cur=g_migrations[i].to_version; advanced=1; g_last_migration_steps++; break; }
            if(failure) break; if(!advanced){ failure=1; break; }
        }
        g_last_migration_ms = ((double)clock() - t0) * 1000.0 / (double)CLOCKS_PER_SEC;
        if(failure || cur!=ROGUE_SAVE_FORMAT_VERSION){ memcpy(payload,rollback,(size_t)payload_size); g_last_migration_failed=1; free(payload); free(rollback); fclose(f); return failure? ROGUE_SAVE_ERR_MIGRATION_FAIL : ROGUE_SAVE_ERR_MIGRATION_CHAIN; }
        free(rollback);
        /* For now we don't persist upgraded payload back; future phase may write-through */
        /* Reset file pointer to just after header for section iteration; we still rely on on-disk layout (no structural changes yet) */
        fseek(f, sizeof desc, SEEK_SET);
        free(payload);
    }
    /* checksum */
    long file_end=0; fseek(f,0,SEEK_END); file_end=ftell(f); if((uint64_t)file_end!=desc.total_size){ fclose(f); return -5; }
    size_t footer_bytes = 0; if(desc.version>=7){ footer_bytes = 4+32; }
    size_t rest=file_end - (long)sizeof desc; if(rest < footer_bytes){ fclose(f); return -5; }
    size_t hashable = rest - footer_bytes; fseek(f,sizeof desc,SEEK_SET); unsigned char* buf=(unsigned char*)malloc(hashable); if(!buf){ fclose(f); return -6; } size_t n=fread(buf,1,hashable,f); if(n!=hashable){ free(buf); fclose(f); return -6; } uint32_t crc=rogue_crc32(buf,hashable); if(crc!=desc.checksum){ g_last_tamper_flags |= ROGUE_TAMPER_FLAG_DESCRIPTOR_CRC; free(buf); fclose(f); return -7; }
    if(desc.version>=7){ unsigned char footer[36]; if(fread(footer,1,36,f)!=36){ free(buf); fclose(f); return -5; } if(memcmp(footer,"SH32",4)!=0){ g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SHA256; free(buf); fclose(f); return ROGUE_SAVE_ERR_SHA256; } memcpy(g_last_sha256,footer+4,32); /* verify recomputed hash */ RogueSHA256Ctx sha; rogue_sha256_init(&sha); rogue_sha256_update(&sha,buf,hashable); unsigned char dg[32]; rogue_sha256_final(&sha,dg); if(memcmp(dg,g_last_sha256,32)!=0){ g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SHA256; free(buf); fclose(f); return ROGUE_SAVE_ERR_SHA256; } }
    free(buf); fseek(f, sizeof desc, SEEK_SET);
    if(desc.version>=3){
        for(uint32_t s=0;s<desc.section_count;s++){
            uint16_t id16=0; uint32_t size=0; if(fread(&id16,sizeof id16,1,f)!=1){ fclose(f); return -8; } if(fread(&size,sizeof size,1,f)!=1){ fclose(f); return -8; }
            uint32_t id = (uint32_t)id16;
            int compressed = (desc.version>=6 && (size & 0x80000000u)); uint32_t stored_size = size & 0x7FFFFFFFu;
            const RogueSaveComponent* comp=find_component((int)id); long payload_pos=ftell(f);
            if(compressed){
                uint32_t uncompressed_size=0; if(fread(&uncompressed_size,sizeof uncompressed_size,1,f)!=1){ fclose(f); return -11; }
                uint32_t comp_bytes = stored_size - sizeof(uint32_t);
                unsigned char* cbuf=(unsigned char*)malloc(comp_bytes); if(!cbuf){ fclose(f); return -12; }
                if(fread(cbuf,1,comp_bytes,f)!=comp_bytes){ free(cbuf); fclose(f); return -12; }
                unsigned char* ubuf=(unsigned char*)malloc(uncompressed_size); if(!ubuf){ free(cbuf); fclose(f); return -12; }
                /* RLE decode */
                size_t ci=0; size_t ui=0; while(ci<stored_size && ui<uncompressed_size){ unsigned char b=cbuf[ci++]; if(ci>=stored_size){ break; } unsigned char run=cbuf[ci++]; for(int r=0;r<run && ui<uncompressed_size;r++){ ubuf[ui++]=b; } }
                /* feed via temp file to existing read_fn expecting FILE* */
                if(comp && comp->read_fn){ FILE* mf=NULL; 
#if defined(_MSC_VER)
                    tmpfile_s(&mf);
#else
                    mf=tmpfile();
#endif
                    if(!mf){ free(cbuf); free(ubuf); fclose(f); return -12; }
                    fwrite(ubuf,1,uncompressed_size,mf); fflush(mf); fseek(mf,0,SEEK_SET); if(comp->read_fn(mf,uncompressed_size)!=0){ fclose(mf); free(cbuf); free(ubuf); fclose(f); return -9; } fclose(mf); }
                free(cbuf); free(ubuf); /* file already positioned just after compressed payload */
            } else {
                if(comp && comp->read_fn){ if(comp->read_fn(f,stored_size)!=0){ fclose(f); return -9; } }
                fseek(f,payload_pos+stored_size,SEEK_SET);
            }
            if(desc.version>=7){ /* read and verify per-section CRC of uncompressed payload */
                /* We captured uncompressed bytes only for compressed path; for uncompressed, we must re-read */
                long end_after_payload = ftell(f); uint32_t sec_crc=0; if(fread(&sec_crc,sizeof sec_crc,1,f)!=1){ fclose(f); return -10; }
                /* Reconstruct uncompressed bytes */
                if(compressed){ /* Skipping deep verify for compressed section (future enhancement) */ (void)payload_pos; }
                else {
                    long payload_size = stored_size; unsigned char* tmp=(unsigned char*)malloc((size_t)payload_size); if(!tmp){ fclose(f); return -12; }
                    fseek(f,payload_pos,SEEK_SET); if(fread(tmp,1,(size_t)payload_size,f)!=(size_t)payload_size){ free(tmp); fclose(f); return -12; }
                    uint32_t calc=rogue_crc32(tmp,(size_t)payload_size); free(tmp); fseek(f,end_after_payload+4,SEEK_SET); if(calc!=sec_crc){ g_last_tamper_flags |= ROGUE_TAMPER_FLAG_SECTION_CRC; fclose(f); return ROGUE_SAVE_ERR_SECTION_CRC; }
                }
            }
        }
    } else {
        for(uint32_t s=0;s<desc.section_count;s++){
            uint32_t id=0; uint32_t size=0; if(fread(&id,sizeof id,1,f)!=1){ fclose(f); return -8; } if(fread(&size,sizeof size,1,f)!=1){ fclose(f); return -8; }
            const RogueSaveComponent* comp=find_component((int)id); long payload_pos=ftell(f); if(comp && comp->read_fn){ if(comp->read_fn(f,size)!=0){ fclose(f); return -9; } } fseek(f,payload_pos+size,SEEK_SET);
        }
    }
    fclose(f); return 0;
}

/* Recovery: attempt primary slot, on tamper/integrity error fall back to latest autosave ring entry (most recent by timestamp field) */
int rogue_save_manager_load_slot_with_recovery(int slot_index){
    g_last_recovery_used=0; int rc=rogue_save_manager_load_slot(slot_index); if(rc==0) return 0;
    /* Only recover on integrity/tamper related errors */
    if(rc!=ROGUE_SAVE_ERR_SECTION_CRC && rc!=ROGUE_SAVE_ERR_SHA256 && rc!=-7){ return rc; }
    /* If descriptor checksum mismatch (-7) or SHA errors, flags already set; ensure descriptor CRC flag set on generic -7 */
    if(rc==-7) g_last_tamper_flags |= ROGUE_TAMPER_FLAG_DESCRIPTOR_CRC;
    /* Scan autosave ring for any successful load (pick newest timestamp) */
    int best_index=-1; uint32_t best_ts=0; for(int i=0;i<ROGUE_AUTOSAVE_RING;i++){
    const char* path = build_autosave_path(i);
    FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) continue; RogueSaveDescriptor desc; if(fread(&desc,sizeof desc,1,f)!=1){ fclose(f); continue; }
    fclose(f); if(desc.version!=ROGUE_SAVE_FORMAT_VERSION) continue; if(desc.timestamp_unix>best_ts){ best_ts=desc.timestamp_unix; best_index=i; }
    }
    if(best_index<0) return rc; /* no fallback */
    /* Try loading best autosave directly (bypass recovery recursion) */
    const char* path = build_autosave_path(best_index);
    /* Preserve existing tamper flags from failed primary before validation resets them */
    uint32_t prev_flags = g_last_tamper_flags;
    RogueSaveDescriptor d; unsigned char* buf=NULL; int lrc = load_and_validate(path,&d,&buf); if(lrc!=0){ /* restore original flags even if recovery attempt fails */ g_last_tamper_flags |= prev_flags; return rc; }
    /* Replay section iteration and invoke read_fns (duplicated minimal logic from load_slot for reliability) */
    unsigned char* p=buf; size_t total_payload=(size_t)(d.total_size - sizeof d); if(d.version>=3){ for(uint32_t s=0;s<d.section_count;s++){ if((size_t)(p-buf)+6>total_payload){ free(buf); return rc; } uint16_t id16; memcpy(&id16,p,2); uint32_t size; memcpy(&size,p+2,4); p+=6; if((size_t)(p-buf)+size>total_payload){ free(buf); return rc; } const RogueSaveComponent* comp=find_component((int)id16); if(comp && comp->read_fn){ /* feed via temp */ FILE* tf=NULL; 
#if defined(_MSC_VER)
        tmpfile_s(&tf);
#else
        tf=tmpfile();
#endif
        if(!tf){ free(buf); return rc; } fwrite(p,1,size,tf); fflush(tf); fseek(tf,0,SEEK_SET); if(comp->read_fn(tf,size)!=0){ fclose(tf); free(buf); return rc; } fclose(tf); }
        p+=size; if(d.version>=7){ if((size_t)(p-buf)+4>total_payload){ free(buf); return rc; } p+=4; } }
    }
    free(buf); g_last_tamper_flags |= prev_flags; g_last_recovery_used=1; return 1; /* recovered */
}

/* Basic player + world meta adapters bridging existing ad-hoc text save for Phase1 demonstration */
static int write_player_component(FILE* f){
    /* Serialize subset of player progression (minimal Phase 1 binary form) */
    fwrite(&g_app.player.level,sizeof g_app.player.level,1,f);
    fwrite(&g_app.player.xp,sizeof g_app.player.xp,1,f);
    fwrite(&g_app.player.health,sizeof g_app.player.health,1,f);
    fwrite(&g_app.talent_points,sizeof g_app.talent_points,1,f);
    return 0;
}
static int read_player_component(FILE* f, size_t size){
    if(size < sizeof(int)*4) return -1; fread(&g_app.player.level,sizeof g_app.player.level,1,f); fread(&g_app.player.xp,sizeof g_app.player.xp,1,f); fread(&g_app.player.health,sizeof g_app.player.health,1,f); fread(&g_app.talent_points,sizeof g_app.talent_points,1,f); return 0; }

/* ---------------- Additional Phase 1 Components ---------------- */

/* INVENTORY: serialize active item instances (count + each record) */
static int write_inventory_component(FILE* f){
    int count = 0; if(g_app.item_instances){
        for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances[i].active) count++;
    }
    if(g_active_write_version >= 4){ if(write_varuint(f,(uint32_t)count)!=0) return -1; }
    else fwrite(&count, sizeof count,1,f);
    if(count==0) return 0;
    for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances[i].active){
        RogueItemInstance* it=&g_app.item_instances[i];
        fwrite(&it->def_index,sizeof it->def_index,1,f);
        fwrite(&it->quantity,sizeof it->quantity,1,f);
        fwrite(&it->rarity,sizeof it->rarity,1,f);
        fwrite(&it->prefix_index,sizeof it->prefix_index,1,f);
        fwrite(&it->prefix_value,sizeof it->prefix_value,1,f);
        fwrite(&it->suffix_index,sizeof it->suffix_index,1,f);
        fwrite(&it->suffix_value,sizeof it->suffix_value,1,f);
    }
    return 0;
}
static int read_inventory_component(FILE* f, size_t size){
    (void)size; int count=0; if(g_active_read_version >=4){ uint32_t c=0; if(read_varuint(f,&c)!=0) return -1; count=(int)c; }
    else if(fread(&count,sizeof count,1,f)!=1) return -1; for(int i=0;i<count;i++){
        int def_index,quantity,rarity,pidx,pval,sidx,sval; if(fread(&def_index,sizeof def_index,1,f)!=1) return -1;
        fread(&quantity,sizeof quantity,1,f); fread(&rarity,sizeof rarity,1,f); fread(&pidx,sizeof pidx,1,f);
        fread(&pval,sizeof pval,1,f); fread(&sidx,sizeof sidx,1,f); fread(&sval,sizeof sval,1,f);
        int inst = rogue_items_spawn(def_index, quantity, 0.0f,0.0f); if(inst>=0){ rogue_item_instance_apply_affixes(inst, rarity, pidx,pval,sidx,sval); }
    } return 0; }

/* SKILLS: ranks + cooldown state (id ordered) */
static int write_skills_component(FILE* f){
    if(g_active_write_version >=4){ if(write_varuint(f,(uint32_t)g_app.skill_count)!=0) return -1; }
    else fwrite(&g_app.skill_count,sizeof g_app.skill_count,1,f);
    for(int i=0;i<g_app.skill_count;i++){
        const RogueSkillState* st = rogue_skill_get_state(i); int rank = st?st->rank:0; fwrite(&rank,sizeof rank,1,f);
        double cd= st? st->cooldown_end_ms:0; fwrite(&cd,sizeof cd,1,f);
    }
    return 0;
}
static int read_skills_component(FILE* f, size_t size){ (void)size; int count=0; if(g_active_read_version >=4){ uint32_t c=0; if(read_varuint(f,&c)!=0) return -1; count=(int)c; } else if(fread(&count,sizeof count,1,f)!=1) return -1; int limit = (count<g_app.skill_count)?count:g_app.skill_count; for(int i=0;i<limit;i++){ int rank; double cd; fread(&rank,sizeof rank,1,f); fread(&cd,sizeof cd,1,f); const RogueSkillDef* d=rogue_skill_get_def(i); struct RogueSkillState* st=(struct RogueSkillState*)rogue_skill_get_state(i); if(d && st){ if(rank>d->max_rank) rank=d->max_rank; st->rank=rank; st->cooldown_end_ms=cd; } } /* skip any extra */ if(count>limit){ size_t skip = (size_t)(count-limit)*(sizeof(int)+sizeof(double)); fseek(f,(long)skip,SEEK_CUR); } return 0; }

/* BUFFS: active buffs list */
extern RogueBuff g_buffs_internal[]; extern int g_buff_count_internal; /* assume symbols provided elsewhere */
static int write_buffs_component(FILE* f){
    int active_count=0; for(int i=0;i<g_buff_count_internal;i++) if(g_buffs_internal[i].active) active_count++;
    if(g_active_write_version >=4){ if(write_varuint(f,(uint32_t)active_count)!=0) return -1; }
    else fwrite(&active_count,sizeof active_count,1,f);
    for(int i=0;i<g_buff_count_internal;i++) if(g_buffs_internal[i].active){ fwrite(&g_buffs_internal[i], sizeof(RogueBuff),1,f);} return 0; }
static int read_buffs_component(FILE* f, size_t size){ (void)size; int count=0; if(g_active_read_version >=4){ uint32_t c=0; if(read_varuint(f,&c)!=0) return -1; count=(int)c; } else if(fread(&count,sizeof count,1,f)!=1) return -1; for(int i=0;i<count;i++){ RogueBuff b; if(fread(&b,sizeof b,1,f)!=1) return -1; rogue_buffs_apply(b.type,b.magnitude,b.end_ms,0.0); } return 0; }

/* VENDOR: seed + restock timers */
static int write_vendor_component(FILE* f){ fwrite(&g_app.vendor_seed,sizeof g_app.vendor_seed,1,f); fwrite(&g_app.vendor_time_accum_ms,sizeof g_app.vendor_time_accum_ms,1,f); fwrite(&g_app.vendor_restock_interval_ms,sizeof g_app.vendor_restock_interval_ms,1,f); return 0; }
static int read_vendor_component(FILE* f, size_t size){ (void)size; fread(&g_app.vendor_seed,sizeof g_app.vendor_seed,1,f); fread(&g_app.vendor_time_accum_ms,sizeof g_app.vendor_time_accum_ms,1,f); fread(&g_app.vendor_restock_interval_ms,sizeof g_app.vendor_restock_interval_ms,1,f); return 0; }

/* STRING INTERN TABLE (component id 7) */
static int write_strings_component(FILE* f){
    int count = g_intern_count;
    if(g_active_write_version >=4){ if(write_varuint(f,(uint32_t)count)!=0) return -1; }
    else fwrite(&count,sizeof count,1,f);
    for(int i=0;i<count;i++){
        const char* s = g_intern_strings[i]; uint32_t len=(uint32_t)strlen(s);
        if(g_active_write_version >=4){ if(write_varuint(f,len)!=0) return -1; }
        else fwrite(&len,sizeof len,1,f);
        if(fwrite(s,1,len,f)!=len) return -1;
    }
    return 0;
}
static int read_strings_component(FILE* f, size_t size){ (void)size; int count=0; if(g_active_read_version>=4){ uint32_t c=0; if(read_varuint(f,&c)!=0) return -1; count=(int)c; } else if(fread(&count,sizeof count,1,f)!=1) return -1; if(count>ROGUE_SAVE_MAX_STRINGS) return -1; for(int i=0;i<count;i++){ uint32_t len=0; if(g_active_read_version>=4){ if(read_varuint(f,&len)!=0) return -1; } else if(fread(&len,sizeof len,1,f)!=1) return -1; if(len>4096) return -1; char* buf=(char*)malloc(len+1); if(!buf) return -1; if(fread(buf,1,len,f)!=len){ free(buf); return -1; } buf[len]='\0'; g_intern_strings[i]=buf; } g_intern_count=count; return 0; }

/* WORLD META: world seed + generation params subset */
static int write_world_meta_component(FILE* f){ fwrite(&g_app.pending_seed,sizeof g_app.pending_seed,1,f); fwrite(&g_app.gen_water_level,sizeof g_app.gen_water_level,1,f); fwrite(&g_app.gen_cave_thresh,sizeof g_app.gen_cave_thresh,1,f); return 0; }
static int read_world_meta_component(FILE* f, size_t size){ (void)size; fread(&g_app.pending_seed,sizeof g_app.pending_seed,1,f); fread(&g_app.gen_water_level,sizeof g_app.gen_water_level,1,f); fread(&g_app.gen_cave_thresh,sizeof g_app.gen_cave_thresh,1,f); return 0; }

static RogueSaveComponent PLAYER_COMP={ ROGUE_SAVE_COMP_PLAYER, write_player_component, read_player_component, "player" };
static RogueSaveComponent INVENTORY_COMP={ ROGUE_SAVE_COMP_INVENTORY, write_inventory_component, read_inventory_component, "inventory" };
static RogueSaveComponent SKILLS_COMP={ ROGUE_SAVE_COMP_SKILLS, write_skills_component, read_skills_component, "skills" };
static RogueSaveComponent BUFFS_COMP={ ROGUE_SAVE_COMP_BUFFS, write_buffs_component, read_buffs_component, "buffs" };
static RogueSaveComponent VENDOR_COMP={ ROGUE_SAVE_COMP_VENDOR, write_vendor_component, read_vendor_component, "vendor" };
static RogueSaveComponent STRINGS_COMP={ ROGUE_SAVE_COMP_STRINGS, write_strings_component, read_strings_component, "strings" };
static RogueSaveComponent WORLD_META_COMP={ ROGUE_SAVE_COMP_WORLD_META, write_world_meta_component, read_world_meta_component, "world_meta" };
/* Replay component (v8) serializes event count + raw events + precomputed SHA256 of events (for forward compatibility). */
static int write_replay_component(FILE* f){
    /* Compute and serialize replay hash (events may be empty). */
    rogue_replay_compute_hash();
    uint32_t count = g_replay_event_count; fwrite(&count,sizeof count,1,f); if(count){ fwrite(g_replay_events,sizeof(RogueReplayEvent),count,f); }
    fwrite(g_last_replay_hash,1,32,f); return 0; }
static int read_replay_component(FILE* f, size_t size){ if(size < sizeof(uint32_t)+32) return -1; uint32_t count=0; fread(&count,sizeof count,1,f); size_t need = (size_t)count*sizeof(RogueReplayEvent) + 32; if(size < sizeof(uint32_t)+need) return -1; if(count>ROGUE_REPLAY_MAX_EVENTS) return -1; if(count){ fread(g_replay_events,sizeof(RogueReplayEvent),count,f); } else { /* no events */ }
    fread(g_last_replay_hash,1,32,f); g_replay_event_count=count; /* Recompute to verify (optional) */ RogueSHA256Ctx sha; rogue_sha256_init(&sha); rogue_sha256_update(&sha,g_replay_events,count*sizeof(RogueReplayEvent)); unsigned char chk[32]; rogue_sha256_final(&sha,chk); if(memcmp(chk,g_last_replay_hash,32)!=0){ return -1; } return 0; }
static RogueSaveComponent REPLAY_COMP={ ROGUE_SAVE_COMP_REPLAY, write_replay_component, read_replay_component, "replay" };

void rogue_register_core_save_components(void){
    rogue_save_manager_register(&PLAYER_COMP);
    rogue_save_manager_register(&WORLD_META_COMP);
    rogue_save_manager_register(&INVENTORY_COMP);
    rogue_save_manager_register(&SKILLS_COMP);
    rogue_save_manager_register(&BUFFS_COMP);
    rogue_save_manager_register(&VENDOR_COMP);
    rogue_save_manager_register(&STRINGS_COMP);
    /* v8+ replay component always registered when available */
#if ROGUE_SAVE_FORMAT_VERSION >= 8
    rogue_save_manager_register(&REPLAY_COMP);
#endif
}

/* Migration definitions */
static int migrate_v2_to_v3(unsigned char* data, size_t size){ (void)data; (void)size; return 0; }
static int migrate_v3_to_v4(unsigned char* data, size_t size){ (void)data; (void)size; return 0; }
static int migrate_v4_to_v5(unsigned char* data, size_t size){ (void)data; (void)size; return 0; }
static int migrate_v5_to_v6(unsigned char* data, size_t size){ (void)data; (void)size; return 0; }
static int migrate_v6_to_v7(unsigned char* data, size_t size){ (void)data; (void)size; return 0; }
static int migrate_v7_to_v8(unsigned char* data, size_t size){ (void)data; (void)size; return 0; }
static int migrate_v8_to_v9(unsigned char* data, size_t size){ (void)data; (void)size; return 0; }
static RogueSaveMigration MIG_V2_TO_V3 = {2u,3u,migrate_v2_to_v3,"v2_to_v3_tlv_header"};
static RogueSaveMigration MIG_V3_TO_V4 = {3u,4u,migrate_v3_to_v4,"v3_to_v4_varint_counts"};
static RogueSaveMigration MIG_V4_TO_V5 = {4u,5u,migrate_v4_to_v5,"v4_to_v5_string_intern"};
static RogueSaveMigration MIG_V5_TO_V6 = {5u,6u,migrate_v5_to_v6,"v5_to_v6_section_compress"};
static RogueSaveMigration MIG_V6_TO_V7 = {6u,7u,migrate_v6_to_v7,"v6_to_v7_integrity"};
static RogueSaveMigration MIG_V7_TO_V8 = {7u,8u,migrate_v7_to_v8,"v7_to_v8_replay_hash"};
static RogueSaveMigration MIG_V8_TO_V9 = {8u,9u,migrate_v8_to_v9,"v8_to_v9_signature_opt"};
static void rogue_register_core_migrations(void){ if(!g_migrations_registered){ rogue_save_register_migration(&MIG_V2_TO_V3); rogue_save_register_migration(&MIG_V3_TO_V4); rogue_save_register_migration(&MIG_V4_TO_V5); rogue_save_register_migration(&MIG_V5_TO_V6); rogue_save_register_migration(&MIG_V6_TO_V7); rogue_save_register_migration(&MIG_V7_TO_V8); rogue_save_register_migration(&MIG_V8_TO_V9); } }
