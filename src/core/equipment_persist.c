/*
 * NOTE: This file is written in a C89-friendly style for MSVC (no mixed
 * declarations and code, no loop-init declarations).
 */

#include "core/equipment_persist.h"
#include "core/equipment.h"
#include "core/loot_instances.h"
#include "core/loot_item_defs.h"
#include <string.h>
#include <stdio.h>

static unsigned long long fnv1a64(const unsigned char* data, size_t n)
{
	unsigned long long h = 1469598103934665603ULL;
	size_t i;
	for(i = 0; i < n; ++i){
		h ^= data[i];
		h *= 1099511628211ULL;
	}
	return h;
}

int rogue_equipment_serialize(char* buf, int cap)
{
	int off = 0;
	int n;
	int s;
	if(!buf || cap < 16) return -1;
	n = snprintf(buf + off, cap - off, "EQUIP_V1\n");
	if(n < 0 || n >= cap - off) return -1;
	off += n;
	for(s = 0; s < ROGUE_EQUIP__COUNT; ++s){
		int inst = rogue_equip_get((enum RogueEquipSlot)s);
		const RogueItemInstance* it;
		if(inst < 0) continue;
		it = rogue_item_instance_at(inst);
		if(!it) continue;
		/* Retrieve set id via base definition (0 if none) & build synthetic runeword pattern. */
		{
			const RogueItemDef* def = rogue_item_def_at(it->def_index);
			int set_id = def ? def->set_id : 0;
			char pattern[64];
			int i;
			int pat_len = 0;
			pattern[0] = '\0';
			if(it->socket_count > 0){
				for(i=0;i<it->socket_count && i<6; ++i){
					int g = it->sockets[i];
					if(g>=0 && pat_len < (int)sizeof(pattern)-3){
						pattern[pat_len++] = 'A' + (g % 26);
					}
				}
				pattern[pat_len] = '\0';
			}
		n = snprintf(buf + off, cap - off,
					"SLOT %d DEF %d ILVL %d RAR %d PREF %d %d SUFF %d %d DUR %d %d ENCH %d QC %d SOCKS %d %d %d %d %d %d %d LOCKS %d %d FRACT %d SET %d RW %s\n",
					s,
					it->def_index,
					it->item_level,
					it->rarity,
					it->prefix_index,
					it->prefix_value,
					it->suffix_index,
					it->suffix_value,
					it->durability_cur,
					it->durability_max,
					it->enchant_level,
					it->quality,
					it->socket_count,
					it->sockets[0], it->sockets[1], it->sockets[2],
					it->sockets[3], it->sockets[4], it->sockets[5],
					it->prefix_locked, it->suffix_locked, it->fractured,
					set_id,
					(pattern[0]?pattern:"-")
				);
		}
		if(n < 0 || n >= cap - off) return -1;
		off += n;
	}
	if(off < cap) buf[off] = '\0';
	return off;
}

static int parse_int(const char** p)
{
	int sign = 1;
	int v = 0;
	while(**p == ' ') (*p)++;
	if(**p == '-'){
		sign = -1;
		(*p)++;
	}
	while(**p >= '0' && **p <= '9'){
		v = v * 10 + (**p - '0');
		(*p)++;
	}
	return v * sign;
}

int rogue_equipment_deserialize(const char* buf)
{
	const char* p;
	int version = 0;
	if(!buf) return -1;
	p = buf;
	if(strncmp(p, "EQUIP_V1", 8) == 0){
		const char* nl = strchr(p, '\n');
		version = 1;
		if(!nl) return -2;
		p = nl + 1;
	}
	(void)version; /* placeholder for future migration logic */
	while(*p){
		if(strncmp(p, "SLOT", 4) == 0){
			int slot;
			int def_index = -1, item_level = 1, rarity = 0;
			int pidx = -1, pval = 0, sidx = -1, sval = 0;
			int dc = 0, dm = 0, ench = 0, qual = 0;
			int sockc = 0;
			int gems[6];
			int pl = 0, sl = 0, fract = 0;
			int i;
			for(i = 0; i < 6; ++i) gems[i] = -1;
			p += 4;
			slot = parse_int(&p);
			if(slot < 0 || slot >= ROGUE_EQUIP__COUNT) return -3;
			while(*p && *p != '\n'){
				if(strncmp(p, "DEF", 3) == 0){ p += 3; def_index = parse_int(&p); }
				else if(strncmp(p, "ILVL", 4) == 0){ p += 4; item_level = parse_int(&p); }
				else if(strncmp(p, "RAR", 3) == 0){ p += 3; rarity = parse_int(&p); }
				else if(strncmp(p, "PREF", 4) == 0){ p += 4; pidx = parse_int(&p); pval = parse_int(&p); }
				else if(strncmp(p, "SUFF", 4) == 0){ p += 4; sidx = parse_int(&p); sval = parse_int(&p); }
				else if(strncmp(p, "DUR", 3) == 0){ p += 3; dc = parse_int(&p); dm = parse_int(&p); }
				else if(strncmp(p, "ENCH", 4) == 0){ p += 4; ench = parse_int(&p); }
				else if(strncmp(p, "QC", 2) == 0){ p += 2; qual = parse_int(&p); }
				else if(strncmp(p, "SOCKS", 5) == 0){
					p += 5;
					sockc = parse_int(&p);
					for(i = 0; i < 6; ++i){ gems[i] = parse_int(&p); }
				} else if(strncmp(p, "LOCKS", 5) == 0){ p += 5; pl = parse_int(&p); sl = parse_int(&p); }
				else if(strncmp(p, "FRACT", 5) == 0){ p += 5; fract = parse_int(&p); }
				else if(strncmp(p, "SET", 3) == 0){ p += 3; (void)parse_int(&p); }
				else if(strncmp(p, "RW", 2) == 0){ p += 2; /* skip runeword pattern token */ while(*p==' ') p++; while(*p && *p!=' ' && *p!='\n') p++; }
				else { p++; }
			}
			if(def_index >= 0){
				extern int rogue_items_spawn(int def_index,int quantity,float x,float y);
				/* spawn */
				{
					int inst = rogue_items_spawn(def_index, 1, 0, 0);
					if(inst >= 0){
						RogueItemInstance* it = (RogueItemInstance*)rogue_item_instance_at(inst);
						if(it){
							it->item_level = item_level;
							it->rarity = rarity;
							it->prefix_index = pidx;
							it->prefix_value = pval;
							it->suffix_index = sidx;
							it->suffix_value = sval;
							it->durability_cur = dc;
							it->durability_max = dm;
							it->enchant_level = ench;
							it->quality = qual;
							it->socket_count = sockc;
							for(i = 0; i < 6; ++i) it->sockets[i] = gems[i];
							it->prefix_locked = pl;
							it->suffix_locked = sl;
							it->fractured = fract;
							rogue_equip_try((enum RogueEquipSlot)slot, inst);
						}
					}
				}
			}
			if(*p == '\n') p++;
		} else {
			/* skip unknown line */
			while(*p && *p != '\n') p++;
			if(*p == '\n') p++;
		}
	}
	return 0;
}

unsigned long long rogue_equipment_state_hash(void)
{
	char tmp[4096];
	int n = rogue_equipment_serialize(tmp, (int)sizeof tmp);
	if(n < 0) return 0ULL;
	return fnv1a64((const unsigned char*)tmp, (size_t)n);
}
