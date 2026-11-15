#include "result_internal.h"
#include "items.h"
enum {HS_NEW,HS_INPUT,HS_FLASH};
static unsigned row(const struct ResultWorld*r,unsigned owner){for(unsigned i=0;i<10;i++)if(r->table[i].owner==owner)return i;return 10;}
static uint8_t rank(const struct ResultWorld*r,unsigned owner){unsigned i=row(r,owner);return i<10?r->table[i].rank:(uint8_t)(r->table[9].rank+1);}
static uint32_t packed(uint32_t value){uint32_t out=0;for(unsigned i=0;i<8;i++){out|=(value%10)<<(4*i);value/=10;}return out;}
static int insert(struct ResultWorld*r,uint32_t score,unsigned owner){
 /*97ABC clears BYTE(a6), not BYTE(a2). All original handler addresses
  * have a zero high byte, so it does NOT clear earlier matching owners. */
 int at=9;while(at>=0&&(int32_t)score>=(int32_t)r->table[at].packed_score)at--;at++;if(at==10)return 0;
 for(int i=9;i>at;i--){r->table[i]=r->table[i-1];if(r->table[i].packed_score!=score)r->table[i].rank++;}
 struct HighScoreRecord*t=&r->table[at];t->owner=owner;t->packed_score=score;t->rank=rank(r,owner);for(unsigned i=0;i<3;i++)t->name[i]=0xb40;return 1;
}
static int parent_phase(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p){enemy_fault(w);return -1;}if(p->kind==ENTITY_ENEMY)return p->payload.enemy.actor.phase;if(p->kind==ENTITY_GROUP)return p->payload.group.phase;enemy_fault(w);return -1;}
static void retire(struct WeaponWorld*w,struct NativeEntity*e){if(!e->payload.enemy.result.mode&&!result_parent_signal(w,e,0,0,0))return;entity_delete(w->entities,e);}
static int name_draw(struct WeaponWorld*w,struct EnemyActor*q,unsigned count){return result_name(w,q->result.cell+q->result.name_offset,q->result.name,count);}
static int score_draw(struct WeaponWorld*w,struct EnemyActor*q,unsigned index){if(index>=10)return enemy_fault(w);return result_packed_digits(w,q->result.cell+0xc0,w->results->table[index].packed_score);}
static int finish(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct ResultWorld*r=w->results;
 /*98144 original name substitution, exact three glyphs. */
 if(q->result.name[0]==0xba6&&q->result.name[1]==0xb8a&&q->result.name[2]==0xca0){q->result.name[0]=0xc8e;q->result.name[1]=0xc80;q->result.name[2]=0xb42;}
 unsigned index=row(r,q->result.slot+1);uint16_t display_rank=11;
 if(index<10){display_rank=rank(r,q->result.slot+1);struct HighScoreRecord*t=&r->table[index];t->rank=display_rank;t->owner=0;for(unsigned i=0;i<3;i++)t->name[i]=q->result.name[i];}
 q->result.rank=display_rank;if(!result_rank(w,q->result.cell,display_rank)||!name_draw(w,q,3))return 0;
 if(q->result.mode){if(!result_parent_signal(w,e,1,-1,1))return 0;}
 else {int phase=parent_phase(w,e);if(phase<0)return 0;if(!phase){retire(w,e);return !w->fault;}}
 q->result.clock=0;q->actor.state=HS_FLASH;return 1;
}
static int automatic_finish(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;if(q->result.cursor>=3||q->result.selection>=41)return enemy_fault(w);uint16_t tile=result_alphabet[q->result.selection];q->result.name[q->result.cursor]=tile==0xce6?0xb40:tile;return finish(w,e);}
static int input_frame(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct ResultWorld*r=w->results;
 if(!r->input_valid||q->result.slot>1||q->result.cursor>=3||q->result.selection>=41)return enemy_fault(w);
 if(!result_rank(w,q->result.cell,rank(r,q->result.slot+1)))return 0;
 if(q->result.mode){if(((q->result.clock>>4)&3)==3){if(!result_rect(w,q->result.cell+0xc0,0xb40,16,2,0))return 0;}else if(!score_draw(w,q,row(r,q->result.slot+1)))return 0;}
 if(!name_draw(w,q,q->result.cursor))return 0;q->result.blink++;uint16_t selected=(q->result.blink&16)?0xb40:result_alphabet[q->result.selection];
 if(!result_rect(w,q->result.cell+q->result.name_offset+64*q->result.cursor,selected|0x4000,2,2,1))return 0;
 uint8_t input=r->edges[q->result.slot]&0x3c;if(!input)input=r->repeat[q->result.slot]&12;
 if(!input){q->result.idle++;if(q->result.idle>=300)return automatic_finish(w,e);}
 else {q->result.idle=q->result.blink=0;q->result.repeat=0;q->result.direction=input&12;
  if(input&4)q->result.selection=q->result.selection?q->result.selection-1:40;
  if(input&8){q->result.selection++;if(q->result.selection>=41)q->result.selection=0;}
  if(input&16){uint16_t tile=result_alphabet[q->result.selection];if(tile==0xce6){q->result.name[q->result.cursor]=0xb40;return finish(w,e);}q->result.name[q->result.cursor]=tile;if(++q->result.cursor==3)return finish(w,e);}
  if(input&32){if(!result_rect(w,q->result.cell+q->result.name_offset+64*q->result.cursor,0x4b40,2,2,1))return 0;if(q->result.cursor)q->result.cursor--;q->result.name[q->result.cursor]=0xb40;}
 }
 q->result.clock++;if(q->result.clock>=900)return automatic_finish(w,e);if(!q->result.mode){int phase=parent_phase(w,e);if(phase<0)return 0;if(!phase)return automatic_finish(w,e);}return 1;
}
static int flash(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;
 if(!q->result.mode){int phase=parent_phase(w,e);if(phase<0)return 0;if(!phase){retire(w,e);return !w->fault;}}
 q->result.clock++;if((int16_t)q->result.clock>=96){retire(w,e);return !w->fault;}
 if(q->result.clock>=72||!(q->result.clock&8)){if(!result_rank(w,q->result.cell,q->result.rank))return 0;if(q->result.mode&&(!q->result.rank||!score_draw(w,q,q->result.rank-1)))return 0;return name_draw(w,q,3);}
 /* Include the left margin used by the full-size10/11 rank label. */
 if(!result_rect(w,q->result.cell-64,0x4b40,7,2,0))return 0;if(q->result.mode&&!result_rect(w,q->result.cell+0xc0,0x4b40,16,2,0))return 0;return result_rect(w,q->result.cell+q->result.name_offset,0x4b40,6,2,0);
}
void result_highscore_tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;if(!w->results||!w->results->ready){enemy_fault(w);return;}struct EnemyActor*q=&e->payload.enemy;
 if(q->entry){q->entry=0;q->result.clock=q->result.idle=q->result.blink=q->result.selection=q->result.cursor=0;q->result.direction=q->result.repeat=0;for(unsigned i=0;i<3;i++)q->result.name[i]=0xb40;
  if(q->result.mode){unsigned index=row(w->results,q->result.slot+1);if(index>=10){enemy_fault(w);return;}q->result.cell=0xa6+2*index;q->result.name_offset=0x300;}
  else{q->result.name_offset=0xc0;if(!result_rect(w,q->result.cell,0xb40,14,2,0))return;}q->actor.state=HS_INPUT;
 }
 if(q->actor.state==HS_INPUT)input_frame(w,e);else if(q->actor.state==HS_FLASH)flash(w,e);else enemy_fault(w);
}
static int spawn(struct WeaponWorld*w,struct EntityRef parent,unsigned slot,uint16_t cell,int mode){if(slot>1)return enemy_fault(w);struct NativeEntity*e=result_allocate(w,parent,ENTITY_FIRST_CHILD,HS_NEW);if(!e)return 0;e->tick=result_highscore_tick;e->payload.enemy.result.slot=slot;e->payload.enemy.result.cell=cell;e->payload.enemy.result.mode=mode?255:0;return 1;}
int results_highscore_enter(struct WeaponWorld*w,struct NativeEntity*parent,unsigned slot,uint16_t cell){if(!w->results||!w->results->ready||!w->items||slot>1||!parent||(parent->kind!=ENTITY_ENEMY&&parent->kind!=ENTITY_GROUP))return enemy_fault(w);if(!insert(w->results,packed(w->items->scores[slot]),slot+1)){if(parent->kind==ENTITY_GROUP)parent->payload.group.command=0;else parent->payload.enemy.result.ack=0;return 1;}return spawn(w,entity_ref(parent),slot,cell,0);}
int results_highscore_edit(struct WeaponWorld*w,struct EntityRef parent,unsigned slot){if(!w->results||!w->results->ready)return enemy_fault(w);return spawn(w,parent,slot,0,1);}
