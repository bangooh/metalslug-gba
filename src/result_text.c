#include "result_internal.h"
int result_rect(struct WeaponWorld*w,uint16_t cell,uint16_t tile,unsigned width,unsigned height,int atlas){return scene_video_fixed_rect(w->video,cell/32,cell%32,tile,width,height,atlas)?1:enemy_fault(w);}
int result_character(struct WeaponWorld*w,uint16_t cell,uint8_t c,uint8_t palette,int large){uint16_t tile=large?(uint16_t)(0xb00+((c&8)<<5)+((c&0x77)<<1)):(uint16_t)(0x400+((c&240)<<1)+(c&15));return result_rect(w,cell,tile|((uint16_t)palette<<12),large?2:1,2,1);}
int result_string(struct WeaponWorld*w,uint16_t cell,unsigned id,uint8_t palette,int large){if(id>=RSTR_COUNT)return enemy_fault(w);const struct ResultBytes*b=&result_strings[id];for(unsigned i=0;i<b->count;i++){uint8_t c=b->bytes[i];if(c==255)return 1;if(!result_character(w,cell,c,palette,large))return 0;cell+=large?64:32;}return enemy_fault(w);}
/*5DAD8 usesFD=new row,FE=end. FF is an ordinary tile in this format. */
int result_stream(struct WeaponWorld*w,uint16_t cell,const struct ResultBytes*b,uint16_t palette){uint16_t start=cell;for(unsigned i=0;i<b->count;i++){unsigned c=b->bytes[i];if(c==254)return 1;if(c==253){cell=++start;continue;}if(!result_rect(w,cell,c|palette,1,1,0))return 0;cell+=32;}return enemy_fault(w);}
/*595AE takes a COUNT inD7, branches toDBRA before reading. It renders
 *8score bytes or3decimal bytes. The final zero is always visible. */
int result_digits(struct WeaponWorld*w,uint16_t cell,uint32_t value,unsigned count){if(!count||count>8)return enemy_fault(w);uint8_t digits[8];for(unsigned i=count;i;i--){digits[i-1]=value%10;value/=10;}unsigned seen=0;for(unsigned i=0;i<count;i++){seen|=digits[i];if(i+1==count)seen=1;uint16_t tile=seen?(uint16_t)(0x3460+digits[i]):0xb40;if(!result_rect(w,cell,tile,1,2,1))return 0;cell+=32;}return 1;}
int result_rank(struct WeaponWorld*w,uint16_t cell,uint16_t rank){
 int index=(int)(rank&255)-1;if(index<0||index>10)index=0;
 /* Also remove a previous wider rank when the displayed place changes. */
 if(!result_rect(w,cell-64,0x300,7,2,0))return 0;
 /* Keep every suffix in the same columns. Full-size10/11 extend left
  * by one digit, so their right edge aligns with ranks1..9. */
 if(index>=9){
  if(!result_character(w,cell-64,'1',4,1)||!result_character(w,cell,(uint8_t)('0'+index-9),4,1))return 0;
 }else if(!result_rect(w,cell,result_rank_glyphs[index]|0x4000,2,2,1))return 0;
 return result_stream(w,cell+0x41,&result_rank_suffixes[index],0x4300);
}
int result_name(struct WeaponWorld*w,uint16_t cell,const uint16_t*name,unsigned count){if(count>3)count=3;for(unsigned i=0;i<count;i++)if(!result_rect(w,cell+64*i,name[i]|0x4000,2,2,1))return 0;return 1;}
/*5D7BE uses two-column glyphs and all eight nibbles, unlike595AE. */
int result_packed_digits(struct WeaponWorld*w,uint16_t cell,uint32_t packed){
 unsigned seen=0;for(unsigned i=0;i<8;i++){
  unsigned n=(packed>>(28-4*i))&15;seen|=n;if(i==7)seen=1;
  /* Padding must stay blank even when the editable @ glyph is present. */
  uint16_t tile=seen?(uint16_t)(result_large_digits[n]|0x4000):0x300;
  if(!result_rect(w,cell+64*i,tile,2,2,seen!=0))return 0;
 }return 1;
}
int result_has_children(struct WeaponWorld*w,const struct NativeEntity*e){if(!w->entities||!e)return enemy_fault(w);return e->next&&w->entities->nodes[e->next].depth>e->depth;}
int result_parent_signal(struct WeaponWorld*w,struct NativeEntity*e,int phase,int value,int add){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p)return enemy_fault(w);uint8_t*field;if(p->kind==ENTITY_GROUP)field=phase?&p->payload.group.phase:&p->payload.group.command;else if(p->kind==ENTITY_ENEMY)field=phase?&p->payload.enemy.actor.phase:&p->payload.enemy.result.ack;else return enemy_fault(w);*field=(uint8_t)(value+(add?*field:0));return 1;}
