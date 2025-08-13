#include "entities/enemy.h"
#include "util/log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int load_sheet(const char* path, RogueTexture* tex, RogueSprite frames[], int* out_count){
	if(!rogue_texture_load(tex,path)){
		/* Attempt implicit ../ fallback if not already containing ../ and initial load failed */
		if(strncmp(path,"../",3)!=0){
			char alt[256];
			snprintf(alt,sizeof alt,"../%s", path);
			if(!rogue_texture_load(tex,alt)) return 0; /* still fail */
		} else return 0;
	}
	int frame_size = tex->h; /* assume square */
	int count = tex->w / frame_size; if(count>8) count=8; if(count<1) count=1;
	for(int i=0;i<count;i++){ frames[i].tex=tex; frames[i].sx=i*frame_size; frames[i].sy=0; frames[i].sw=frame_size; frames[i].sh=tex->h; }
	*out_count = count; return 1;
}

int rogue_enemy_load_config(const char* path, RogueEnemyTypeDef types[], int* inout_type_count){
	FILE* f=NULL;
#if defined(_MSC_VER)
	fopen_s(&f,path,"rb");
#else
	f=fopen(path,"rb");
#endif
	if(!f){
		/* Try fallback prefixes so running from build/ works (mirror player sheet behavior) */
		const char* prefixes[] = { "../", "../../", "../../../" };
		char attempt[512];
		for(size_t i=0;i<sizeof(prefixes)/sizeof(prefixes[0]) && !f;i++){
#if defined(_MSC_VER)
			_snprintf_s(attempt,sizeof attempt,_TRUNCATE,"%s%s", prefixes[i], path);
			fopen_s(&f,attempt,"rb");
#else
			snprintf(attempt,sizeof attempt,"%s%s", prefixes[i], path);
			f=fopen(attempt,"rb");
#endif
			if(f){ ROGUE_LOG_INFO("Opened enemy cfg via fallback path: %s", attempt); break; }
		}
	}
	if(!f){ ROGUE_LOG_WARN("enemy cfg open fail: %s", path); return 0; }
	char line[512]; int loaded=0; int cap=*inout_type_count;
	while(fgets(line,sizeof line,f)){
		char* p=line; while(*p==' '||*p=='\t') p++;
		if(*p=='#' || *p=='\n' || *p=='\0') continue;
		/* Format: ENEMY,name,group_min,group_max,patrol_radius,aggro_radius,speed,pop_target,xp_reward,loot_chance,idle.png,run.png,death.png */
		if(strncmp(p,"ENEMY",5)!=0) continue; p+=5; if(*p==',') p++;
	char name[32]; int gmin,gmax,pr,ar; float spd; int pop_target=0,xp_reward=1; float loot_chance=0.0f; char idlep[128], runp[128], deathp[128];
#if defined(_MSC_VER)
	int n=sscanf_s(p,"%31[^,],%d,%d,%d,%d,%f,%d,%d,%f,%127[^,],%127[^,],%127[^\r\n]", name,(unsigned)_countof(name),&gmin,&gmax,&pr,&ar,&spd,&pop_target,&xp_reward,&loot_chance,idlep,(unsigned)_countof(idlep),runp,(unsigned)_countof(runp),deathp,(unsigned)_countof(deathp));
#else
	int n=sscanf(p,"%31[^,],%d,%d,%d,%d,%f,%d,%d,%f,%127[^,],%127[^,],%127[^\r\n]", name,&gmin,&gmax,&pr,&ar,&spd,&pop_target,&xp_reward,&loot_chance,idlep,runp,deathp);
#endif
		if(n!=12){ ROGUE_LOG_WARN("enemy cfg parse fail: %s", line); continue; }
		if(loaded>=cap) break;
		RogueEnemyTypeDef* t=&types[loaded]; memset(t,0,sizeof *t);
		#if defined(_MSC_VER)
			strncpy_s(t->name, sizeof t->name, name, _TRUNCATE);
		#else
			strncpy(t->name,name,sizeof t->name -1);
			t->name[sizeof t->name -1]='\0';
		#endif
		t->group_min=gmin; t->group_max=gmax; t->patrol_radius=pr; t->aggro_radius=ar; t->speed=spd; t->weight=(pop_target>0?pop_target:1); t->pop_target=(pop_target>0?pop_target:gmax*4); t->xp_reward=(xp_reward>0?xp_reward:1); t->loot_chance=(loot_chance<0?0:(loot_chance>1?1:loot_chance));
		if(!load_sheet(idlep,&t->idle_tex,t->idle_frames,&t->idle_count)) ROGUE_LOG_WARN("enemy idle sheet load fail: %s", idlep);
		if(!load_sheet(runp,&t->run_tex,t->run_frames,&t->run_count)) ROGUE_LOG_WARN("enemy run sheet load fail: %s", runp);
		if(!load_sheet(deathp,&t->death_tex,t->death_frames,&t->death_count)) ROGUE_LOG_WARN("enemy death sheet load fail: %s", deathp);
		loaded++;
	}
	fclose(f);
	if(loaded>0){ ROGUE_LOG_INFO("Loaded %d enemy type(s)", loaded); }
	*inout_type_count = loaded; return loaded>0;
}
