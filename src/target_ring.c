#include "target_ring.h"
static unsigned next(unsigned n){return (n+1)&7;}
void target_ring_reset(struct TargetRing*r){r->first=r->write=r->end=r->count=0;}
void target_ring_frame(struct TargetRing*r){r->first=r->end;r->end=r->write;r->count=0;}
uint8_t target_ring_new_key(struct TargetRing*r){r->last_key=(uint8_t)(((r->last_key+1)&63)+64);return r->last_key;}
static void advance(struct TargetRing*r){if(r->count<4){r->write=(uint8_t)next(r->write);r->count++;}}
void target_ring_register(struct TargetRing*r,int16_t screen_x,int16_t x,int16_t y,uint8_t key,int8_t*owner){
 uint8_t status=254;
 for(unsigned i=r->first;i!=r->end;i=next(i))if(r->records[i].key==key){
  *owner=(int8_t)r->records[i].owner;if(*owner>=0)status=255;
 }
 /* SUB.W #$10 then SUB.W #$130: accept screen X in [16,320). */
 if((uint16_t)(screen_x-16)>=304)return;
 for(unsigned i=r->end;i!=r->write;i=next(i))if(r->records[i].key==key){
  if((int8_t)r->records[i].owner<0){r->records[i].x=x;r->records[i].y_up=y;r->records[i].owner=(uint8_t)*owner;}return;
 }
 r->records[r->write]=(struct TargetRingRecord){x,y,key,status};advance(r);
}
int target_ring_find(const struct TargetRing*r,unsigned player,int16_t x,int16_t y,int16_t*key){
 for(unsigned i=r->first;i!=r->end;i=next(i)){
  const struct TargetRingRecord*p=&r->records[i];
  if(p->owner!=254||(player>1&&(int8_t)p->key>=0))continue;
  /* It returns the FIRST pending record even outside range. The negative
   * key tells the caller not to claim; searching later records is wrong. */
  *key=(int16_t)(0xff00u|p->key);
  if((uint16_t)(x-p->x+24)<48&&(uint16_t)(y-p->y_up+32)<64)*key=p->key;
  return 1;
 }
 return 0;
}
static void renew(struct TargetRing*r,unsigned player,uint8_t key,int following){
 uint8_t owner=(uint8_t)(player<2?player:2);
 for(unsigned i=r->end;i!=r->write;i=next(i))if(r->records[i].key==key){r->records[i].owner=owner;return;}
 /* $08F598/$08F656 return immediately after updating the first matching
  * new record. Only a missing record appends without initializing XY. */
 r->records[r->write].key=key;r->records[r->write].owner=following?255:owner;advance(r);
}
int target_ring_claim(struct TargetRing*r,unsigned player,uint8_t key){
 for(unsigned i=r->first;i!=r->end;i=next(i))if(r->records[i].key==key&&r->records[i].owner==254){
  r->records[i].owner=255;renew(r,player,key,0);return 1;
 }
 return 0;
}
int target_ring_follow(struct TargetRing*r,unsigned player,uint8_t key,int16_t*x,int16_t*y){
 for(unsigned i=r->first;i!=r->end;i=next(i))if(r->records[i].key==key&&(int8_t)r->records[i].owner>=0){
  *x=r->records[i].x;*y=r->records[i].y_up;renew(r,player,key,1);return 1;
 }
 return 0;
}
