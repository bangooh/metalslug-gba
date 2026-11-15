#include "mission_result_internal.h"
int mr_text(struct WeaponWorld*w,uint16_t cell,unsigned id,uint8_t palette,int large){
 if(id>=MRSTR_COUNT)return enemy_fault(w);const struct ResultBytes*b=&mission_result_strings[id];
 for(unsigned i=0;i<b->count;i++){uint8_t c=b->bytes[i];if(c==255)return 1;if(!result_character(w,cell,c,palette,large))return 0;cell+=large?64:32;}return enemy_fault(w);
}
/*4768A always draws tens;4772A leaves the previous tens untouched at zero. */
int mr_two_digits(struct WeaponWorld*w,uint16_t cell,uint16_t value,int zero){if(value>99)value=99;unsigned tens=value/10,ones=value%10;if((zero||tens)&&!result_rect(w,cell,mission_result_digits[tens],2,2,1))return 0;return result_rect(w,cell+64,mission_result_digits[ones],2,2,1);}
int mr_frame(struct WeaponWorld*w,struct NativeEntity*e){
 static const uint16_t rows[][4]={{0x68,0x23ba,1,1},{0x88,0x23bf,15,1},{0x268,0x23bb,1,1},{0x93,0x23cc,1,1},{0xb3,0x23cd,13,1},{0x253,0x23ce,1,1},{0x7b,0x23bc,1,1},{0x9b,0x23cb,15,1},{0x27b,0x23bd,1,1},{0x69,0x23be,1,18},{0x269,0x23ca,1,18}};
 for(unsigned i=0;i<sizeof(rows)/sizeof(rows[0]);i++)if(!result_rect(w,rows[i][0]+e->payload.enemy.mission_result.cell,rows[i][1],rows[i][2],rows[i][3],0))return 0;entity_delete(w->entities,e);return 1;
}
static int line(struct WeaponWorld*w,struct MissionResultActor*m,unsigned row){
 unsigned length=m->lengths[row];if(!length)return 1;unsigned offset=m->names[row],prefix=m->prefixes[row];if(prefix>=64||offset+length>sizeof(mission_name_bytes)||length>=16)return enemy_fault(w);uint16_t cell=0x94+row+(m->slot?0x220:0);
 for(unsigned i=0;i<4;i++){if(!result_rect(w,cell,mission_roster_prefix[prefix][i]|0x2300,1,1,0))return 0;cell+=32;}
 for(unsigned i=0;i<length;i++){if(!result_rect(w,cell,mission_name_bytes[offset+i]|0x2300,1,1,0))return 0;cell+=32;}return 1;
}
int mr_roster(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*p=mr_parent(w,e);if(!p)return 0;if(!p->actor.phase)return 1;p->actor.phase=0;struct MissionResultActor*m=&e->payload.enemy.mission_result;
 if(m->full)for(unsigned i=0;i<7;i++){m->names[i]=m->names[i+1];m->lengths[i]=m->lengths[i+1];m->prefixes[i]=m->prefixes[i+1];}
 if(!w->rng||m->index>6)return enemy_fault(w);unsigned index=m->index;
 m->prefixes[index]=source_random(w->rng,64);const struct ResultBytes*weights=&mission_name_weights[1];unsigned length=weights->bytes[source_random(w->rng,weights->count)];unsigned bank=length-2;if(bank>=10)return enemy_fault(w);
 const struct MissionNameBank*b=&mission_name_banks[1][bank];if(!b->count||b->length!=length)return enemy_fault(w);m->lengths[index]=length;m->names[index]=b->first+length*source_random(w->rng,b->count);
 if(++m->index>6){m->full=255;m->index=6;}
 if(!result_rect(w,0x94+(m->slot?0x220:0),0x2320,15,7,0))return 0;for(unsigned i=0;i<7;i++)if(!line(w,m,i))return 0;return 1;
}
