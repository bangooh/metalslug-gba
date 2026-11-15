#include "items.h"
#include "interactions.h"
#include "slug_support.h"
#include "../generated/item-programs.h"
_Static_assert(ITEM_HMG==0&&ITEM_FOOD_FIRST==8&&ITEM_TIMED_FOOD==28&&ITEM_ROT_FOOD==29&&ITEM_KIND_COUNT==31,"generated item template indices");
/* $09A75A..$09BB54: semantic states, never executable source addresses. */
enum State {NEW,COUNTED,FLY,WEAPON,CYCLE_SHOW,CYCLE_OUT,CYCLE_IN,SUPPLY,TAKEN,RISE,
 FOOD_WAIT,FOOD_BLINK,FOOD_BLINK_KEEP,TIMED,ROT_WAIT,ROT,THROWN,BOUNCE,
 POPUP_TABLE,POPUP_VALUE,POPUP_RUN,DIGIT,DIGIT_LIFT,DIGIT_HOLD,DIGIT_BLINK,
 COMBO,FREE,CLEANUP};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int fail(struct WeaponWorld*w){w->fault=28;if(w->items)w->items->fault=1;return 0;}
static int16_t word(int v){return (int16_t)(uint16_t)v;}
static void schedule(struct ItemActor*q,enum State s){q->actor.state=(uint8_t)s;q->entry=1;}
static int sound(struct WeaponWorld*w,uint16_t s){return w->sound&&w->sound(w->scene,s,0)?1:fail(w);}
static void palette(struct ItemActor*q,uint16_t p){
 for(unsigned i=q->palette_count;i;i--)if(q->palettes[i-1]==p){q->actor.palette=p;return;}
 if(q->palette_count<3){q->palettes[q->palette_count++]=p;q->actor.palette=p;}
}
static void start(struct ItemActor*q,uint16_t entry){animation_begin(&q->actor.animation,entry);q->actor.movement_flags|=32;}
static uint32_t read_field(void*user,unsigned f,unsigned width){
 struct ItemActor*q=user;
 if(f==0&&width==2)return q->palettes[1];
 if(f==1&&width==1)return q->params[0];
 if(f==2&&width==1)return q->actor.animation.pose;
 if(f==3&&width==2)return q->palettes[0];
 q->actor.animation.fault=3;return 0;
}
static int write_field(void*user,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t value){
 struct ItemActor*q=user;uint32_t old=read_field(user,f,width);if(q->actor.animation.fault)return 0;
 switch(op){case AN_FIELD_SET:break;case AN_FIELD_AND:value&=old;break;case AN_FIELD_OR:value|=old;break;case AN_FIELD_XOR:value^=old;break;default:return 0;}
 if(f==1)q->params[0]=(uint8_t)value;else if(f==2)q->actor.animation.pose=(uint8_t)value;else return 0;return 1;
}
static int bind(void*user,unsigned kind,uint32_t value){
 if(kind!=AN_PALETTE)return 0;((struct ItemActor*)user)->actor.palette=(uint16_t)value;return 1;
}
static int animate(struct WeaponWorld*w,struct ItemActor*q){
 if(q->actor.animation.cursor==65535)return 0; /* Source FFFFFFFF glyph entry retains no drawing. */
 struct AnimationOwner owner={.context=q,.read=read_field,.write=write_field,.bind=bind};
 int done=animation_step(&q->actor.animation,&item_program,&owner);
 if(q->actor.animation.fault)fail(w);return done;
}
static void priority(struct WeaponWorld*w,struct ItemActor*q,uint16_t value,unsigned bits){
 if((uint16_t)(value-0x4000)<0x7fff)value=(uint16_t)((value&0xe01f)|((255-((transform_y_up(&q->actor.transform)+w->camera_y)&255))<<5));
 q->actor.transform.priority=(uint16_t)((value&0xffe3)|bits);
}
static int move(struct WeaponWorld*w,struct ItemActor*q,enum WeaponMove m){
 struct WeaponContact hit;if(!w->move){fail(w);return -1;}
 int r=w->move(w->scene,&q->actor,m,&hit);if(r<0)fail(w);return r;
}
static int physics(struct WeaponWorld*w,struct ItemActor*q){
 if(move(w,q,WMOVE_SYNC)<0)return 0;
 if(q->params[3]!=0&&q->params[3]!=255)return 1;
 if(!w->foot_contact)return fail(w);int r=w->foot_contact(w->scene,&q->actor);
 /* Original foot probe's carry means air, opposite to this C API. */
 if(r<0)return fail(w);return r||move(w,q,WMOVE_ITEM)>=0;
}
static int outside(struct WeaponWorld*w,struct ItemActor*q,const int16_t*b,int force){
 struct WeaponActor*a=&q->actor;
 int16_t x=word((a->transform.x>>8)-w->camera_x),y=word(transform_y_up(&a->transform)+w->camera_y);
 if(force)a->hit.flags|=128;
 if(a->hit.flags&128)return x<b[0]||x>word(320+b[1])||y<word(256-b[3])||y>word(496-b[2]);
 /* $05DDBE/$05DDF2 only latch entry; wide edges latch for NEXT call. */
 if((x>0&&x<320&&y>256&&y<496)||x<=-128||x>=512||y<=0||y>=640)a->hit.flags|=128;
 return 0;
}
static void tail(struct WeaponWorld*w,struct NativeEntity*e,int counted){
 struct ItemActor*q=&e->payload.item;if(counted&&q->counted==1)w->items->count_current++;
 if(outside(w,q,item_bounds,q->params[7]!=0))entity_delete(w->entities,e);
}
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t){
 if(!w->items||!w->entities||!t){fail(w);return 0;}
 struct NativeEntity*e=entity_spawn(w->entities,parent,ENTITY_ITEM,order,tick);if(!e){fail(w);return 0;}
 struct ItemActor*q=&e->payload.item;
 *q=(struct ItemActor){.entry=1,.palettes={65535,65535,65535},.scale_x=255,.scale_y=255,
  .params={255,255,255,255,255,255,255,255},
  .actor={.active=1,.owner=255,.damage_owner=255,.score_owner=255,.platform_ids=255,.ground_slope=-1,.ground_tag=-1,
   .hit={.id=e->index,.hp=-1,.last_attacker=65535,.last_kind=255,.flags=64},
   .animation={.cursor=65535,.drawing=65535,.last_draw_cursor=65535}}};
 q->actor.transform=*t;transform_integer_copy(&q->actor.transform,t);return e;
}
int item_score_add(struct ItemWorld*w,uint8_t owner,uint32_t packed){
 if((int8_t)owner<0)return 1;uint32_t decimal=0,place=1;
 for(unsigned i=0;i<8;i++){unsigned digit=packed&15;if(digit>9){w->fault=2;return 0;}decimal+=digit*place;place*=10;packed>>=4;}
 unsigned slot=owner?1:0;uint32_t old=w->scores[slot];
 w->scores[slot]=old>=99999999||decimal>99999999-old?99999999:old+decimal;return 1;
}
int item_score_popup(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerTransform*t,uint8_t owner,int notice,uint8_t kind,uint8_t special){
 struct NativeEntity*e=allocate(w,parent,ENTITY_FIRST_CHILD,t);if(!e)return 0;
 struct ItemActor*q=&e->payload.item;q->actor.owner=owner;q->popup_kind=kind;q->popup_special=special;
 schedule(q,notice?POPUP_TABLE:POPUP_VALUE);return 1;
}
static int pickup(struct WeaponWorld*w,struct ItemActor*q,unsigned box){
 if(q->actor.hit.invulnerable||!w->items->player_query_enabled)return -1;
 int selected=-1;const uint16_t*b=item_pickup_boxes[box];
 for(unsigned i=0;i<2;i++){
  struct PlayerBodyState*p=w->players[i];
  if(!p||p->life_retired||!(p->actor.lifecycle_flags&128)||(p->actor.hit.flags&9))continue;
  uint16_t dx=(uint16_t)((p->actor.transform.x>>8)-(q->actor.transform.x>>8)+b[0]);
  uint16_t dy=(uint16_t)(transform_y_up(&p->actor.transform)-transform_y_up(&q->actor.transform)+b[2]);
  if(dx>=b[1]||dy>=b[3])continue;
  if(selected<0||(uint16_t)word((p->actor.transform.x>>8)-w->camera_x)>=
    (uint16_t)word((w->players[selected]->actor.transform.x>>8)-w->camera_x))selected=(int)i;
 }
 return selected;
}
static int popup(struct WeaponWorld*w,struct NativeEntity*e,int notice,uint8_t kind,uint8_t special){
 struct ItemActor*q=&e->payload.item;
 return item_score_popup(w,entity_ref(e),&q->actor.transform,q->actor.owner,notice,kind,special);
}
static int give_weapon(struct WeaponWorld*w,struct ItemActor*q,unsigned slot,unsigned weapon,unsigned quantity){
 struct PlayerBodyState*p=w->players[slot];
 if(!player_set_weapon_ammo(p,weapon,quantity,1))return fail(w);
 if(p->action==2&&!sound(w,0x10fd))return 0;
 player_begin_weapon_flash(p);q->taken_variant=0;schedule(q,TAKEN);return 1;
}
static int food_pickup(struct WeaponWorld*w,struct NativeEntity*e){
 struct ItemActor*q=&e->payload.item;int slot=pickup(w,q,1);if(slot<0)return 1;
 q->actor.owner=(uint8_t)slot;if(!sound(w,0x1121))return 0;
 uint8_t index=q->params[0],kind=q->params[4];struct ItemWorld*s=w->items;
 if(kind==2){index=15;s->combo_level=14;s->combo_timer=45;}
 else{
  if(kind){if((int8_t)s->combo_level>(int8_t)index)index=s->combo_level;s->combo_level=(uint8_t)(index+1);s->combo_timer=45;}
  if((int8_t)index>14)index=14;
 }
 if(!popup(w,e,0,index,kind))return 0;schedule(q,FREE);return 1;
}
static int supply(struct WeaponWorld*w,struct NativeEntity*e,unsigned slot){
 struct ItemActor*q=&e->payload.item;struct PlayerBodyState*p=w->players[slot];q->actor.owner=(uint8_t)slot;
 if(q->kind==ITEM_AMMO){
  enum PlayerPickupResult result=player_add_ammo_item(p,q->params[0],q->params[1]);
  if(result!=PICKUP_IGNORED&&!popup(w,e,result==PICKUP_AMMO_NOTICE,result==PICKUP_AMMO_NOTICE?2:3,0))return 0;
  q->taken_variant=2;
 }else if(q->kind==ITEM_GRENADES){
  int ridden=p->action==2;
  if(ridden){if(!w->items->slug)return fail(w);q->params[0]=q->params[1];slug_ammo_add(w->items->slug,q->params[0]);}
  else player_add_grenades(p,q->params[0]);
  if(!popup(w,e,1,(uint8_t)ridden,0))return 0;q->taken_variant=1;
 }else{
  int repaired=0;
  if(p->action==2){if(!w->items->slug)return fail(w);repaired=slug_hp_add(w->items->slug,q->params[0]);if(repaired&&!sound(w,0x1083))return 0;}
  if(!popup(w,e,repaired,3,0))return 0;q->taken_variant=3;
 }
 schedule(q,TAKEN);return sound(w,0x1124);
}
static void cycle_map(struct ItemActor*q){
 if(item_cycles[q->sequence][q->sequence_pos]==255)q->sequence_pos=0;
 start(q,item_cycle_roots[item_cycles[q->sequence][q->sequence_pos]&3]);
}
static uint16_t lifetime(const struct ItemActor*q){return q->params[1]?(uint16_t)(q->params[1]*16):32767;}
static void thrown_velocity(struct ItemActor*q,unsigned bounce){
 const int16_t*v=item_throw[bounce][q->motion_row];q->actor.vx=(q->actor.transform.facing&1)?v[0]:word(-v[0]);
 q->actor.ay=v[1];q->actor.vy=v[2];
}
static int constructor(struct WeaponWorld*w,struct ItemActor*q){
 struct WeaponActor*a=&q->actor;
 if(q->kind<=ITEM_CYCLE){
  palette(q,0x17d);a->hit.invulnerable=8;priority(w,q,0x8000,0);
  if(q->kind==ITEM_CYCLE){q->sequence=q->params[0]<11?q->params[0]:10;q->sequence_pos=0;cycle_map(q);schedule(q,CYCLE_SHOW);}
  else{start(q,item_entries[IAN_HMG+q->kind]);schedule(q,WEAPON);}
 }else if(q->kind<=ITEM_REPAIR){
  static const uint16_t pal[3][2]={{0x17f,0x1a7},{0x17e,0x1a8},{0x180,0x1a9}};
  unsigned n=q->kind-ITEM_AMMO;palette(q,pal[n][0]);palette(q,pal[n][1]);a->hit.invulnerable=8;
  priority(w,q,0x8000,0);start(q,item_entries[IAN_AMMO+n]);
  if(n<2){if(!q->params[0])q->params[0]=10;if(!q->params[1])q->params[1]=n?10:100;}schedule(q,SUPPLY);
 }else if(q->kind<=ITEM_FOOD_LAST){
  const struct ItemFoodData*d=&item_foods[q->kind-ITEM_FOOD_FIRST];q->params[4]=d->kind;start(q,d->entry);
  if(q->params[2]!=255)a->transform.facing^=1;palette(q,d->palette);a->hit.invulnerable=8;
  priority(w,q,0x8000,0);a->timer=lifetime(q);schedule(q,FOOD_WAIT);
 }else if(q->kind==ITEM_TIMED_FOOD||q->kind==ITEM_ROT_FOOD){
  q->params[4]=0;if(q->params[2]!=255)a->transform.facing^=1;
  priority(w,q,0x8000,0);
  if(q->kind==ITEM_TIMED_FOOD){palette(q,0x1ac);a->timer=q->params[1]?(uint16_t)(q->params[1]*2):32767;start(q,item_entries[IAN_TIMED]);schedule(q,TIMED);}
  else{palette(q,0x1c6);unsigned life=lifetime(q);a->timer=(uint16_t)((life>70?life:70)-70);start(q,item_entries[IAN_ROT_WAIT]);schedule(q,ROT_WAIT);}
 }else if(q->kind==ITEM_THROWN_FOOD){
  if(!w->rng)return fail(w);palette(q,0x1ac);a->transform.facing=(uint8_t)(source_random_word(w->rng)&1);
  q->motion_row=(uint8_t)(source_random_word(w->rng)&3);thrown_velocity(q,0);q->params[4]=1;q->params[1]=10;
  a->timer=lifetime(q);a->hit.invulnerable=8;priority(w,q,0x8000,0);start(q,item_foods[7].entry);schedule(q,THROWN);
 }else return fail(w);return 1;
}
static int popup_construct(struct WeaponWorld*w,struct NativeEntity*e,int notice){
 struct ItemActor*q=&e->payload.item;struct WeaponActor*a=&q->actor;const uint8_t*glyphs=0;uint8_t count;
 if(notice){glyphs=item_notices[q->popup_kind&3];count=*glyphs++;}
 else{
  unsigned s=!!q->popup_special,k=q->popup_kind&15;q->packed_score=item_score_bcd[s][k];
  if(!item_score_add(w->items,a->owner,q->packed_score))return fail(w);count=item_score_counts[s][k];
 }
 q->pending=count;transform_integer_offset(&a->transform,8*(count&15),0);a->transform.facing=0;
 if(!w->rng)return fail(w);transform_integer_offset(&a->transform,0,(source_random_word(w->rng)&14)+9);
 int score_x=0,previous_left=0;unsigned digit_index=0;
 static const uint8_t score_left[10]={3,4,3,3,3,3,3,3,3,3};
 static const uint8_t score_right[10]={9,7,9,9,9,9,9,9,9,9};
 do{
  struct NativeEntity*d=allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform);if(!d)return 0;
  struct ItemActor*r=&d->payload.item;r->actor.owner=a->owner;transform_integer_offset(&a->transform,-8,0);
  r->glyph=notice?*glyphs++:(uint8_t)(q->packed_score&15);if(!notice)q->packed_score>>=4;
  if(!notice){
   /* Keep the source eight-unit motion and rightmost anchor. At 75% it
    * advances six GBA pixels, but reviewed digits occupy up to seven.
    * Bounds include the shadow; exactly one empty column separates them. */
   if(r->glyph>9)return fail(w);
   if(digit_index++)score_x+=6+previous_left-score_right[r->glyph]-2;
   r->score_spacing_x=(int8_t)score_x;previous_left=score_left[r->glyph];
  }
  r->actor.timer=(count&7)*2;schedule(r,DIGIT);count--;
 }while((int8_t)count>0);
 a->timer=30;q->watchdog=150;schedule(q,POPUP_RUN);return 1;
}
static void parity(struct WeaponWorld*w,struct ItemActor*q){q->actor.palette=q->palettes[w->frame_phase&1];}
static int digit_tick(struct WeaponWorld*w,struct NativeEntity*e,enum State state){
 struct ItemActor*q=&e->payload.item;struct WeaponActor*a=&q->actor;
 struct NativeEntity*parent=entity_get(w->entities,e->parent);
 if(!parent||parent->kind!=ENTITY_ITEM){schedule(q,FREE);return 1;}
 struct ItemActor*r=&parent->payload.item;
 if(state==DIGIT){
  if(move(w,q,WMOVE_SYNC)<0)return 0;
  if((int16_t)--a->timer<=0){
   if(move(w,q,WMOVE_POINT)<0)return 0;
   int16_t py=word(transform_y_up(&r->actor.transform)+w->camera_y),y=word(transform_y_up(&a->transform)+w->camera_y);
   if(py>y){transform_integer_offset(&a->transform,0,word(py+16-y));r->pending--;schedule(q,DIGIT_LIFT);}
   parity(w,q);animate(w,q);
  }
 }else if(state==DIGIT_LIFT){
  if(move(w,q,WMOVE_POINT)<0)return 0;parity(w,q);animate(w,q);
  if((int16_t)--a->timer<=0)schedule(q,DIGIT_HOLD);
  if(r->pending==255)schedule(q,DIGIT_BLINK);
 }else{
  if(move(w,q,WMOVE_SYNC)<0)return 0;
  if(state==DIGIT_HOLD||(w->frame_phase&1)){parity(w,q);animate(w,q);}
  if(state==DIGIT_HOLD&&r->pending==255)schedule(q,DIGIT_BLINK);
 }
 if(parent->deleting)schedule(q,FREE);return !w->fault;
}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;if(!w->items){fail(w);return;}
 struct ItemActor*q=&e->payload.item;struct WeaponActor*a=&q->actor;enum State state=(enum State)a->state;
 /* Entry aliases run this tick; schedule() elsewhere takes effect next tick. */
 if(q->entry){
  q->entry=0;
  if(state==COUNTED){q->counted=1;if((int8_t)w->items->count_previous>=2){entity_delete_children(pool,e);entity_delete(pool,e);return;}state=FLY;}
  if(state==NEW){if(!constructor(w,q))return;state=(enum State)a->state;q->entry=0;}
  if(state==FLY){palette(q,0x180);start(q,item_entries[IAN_FLY]);}
  else if(state==CYCLE_SHOW){q->scale_x=255;a->timer=80;}
  else if(state==CYCLE_OUT)a->timer=0;
  else if(state==CYCLE_IN){q->sequence_pos++;if(q->sequence_pos>=16){fail(w);return;}cycle_map(q);}
  else if(state==TAKEN){palette(q,(uint16_t)(0x1a6+q->taken_variant));a->timer=2;priority(w,q,0xc000,0);}
  else if(state==RISE){palette(q,0x180);start(q,item_entries[IAN_TAKEN]);}
  else if(state==FOOD_BLINK||state==FOOD_BLINK_KEEP){
   if(state==FOOD_BLINK)a->timer=(uint16_t)((a->timer&255)|(30<<8));priority(w,q,0x8000,0);state=FOOD_BLINK_KEEP;
  }else if(state==ROT){start(q,item_entries[IAN_ROT]);priority(w,q,0x8000,0);}
  else if(state==BOUNCE)thrown_velocity(q,1);
  else if(state==POPUP_TABLE||state==POPUP_VALUE){if(!popup_construct(w,e,state==POPUP_TABLE))return;state=POPUP_RUN;q->entry=0;}
  else if(state==DIGIT){
   /* Original slot colours: P1 Marco, P2 Tarma. */
   palette(q,a->owner?0x1cd:0x1b8);palette(q,a->owner?0x1ce:0x1b9);priority(w,q,0xc000,28);
   a->vx=a->ax=0;a->vy=0x886;a->ay=-0x123;if(item_glyphs[q->glyph&31]!=65535)start(q,item_glyphs[q->glyph&31]);
  }else if(state==DIGIT_LIFT){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);a->vy=0x400;a->timer=2;}
  else if(state==COMBO)w->items->combo_timer=w->items->combo_level=0;
  a->state=(uint8_t)state;
 }
 if(state==FREE||state==CLEANUP){if(state==CLEANUP)entity_delete_children(pool,e);entity_delete(pool,e);return;}
 if(state==COMBO){
  if((int8_t)--w->items->combo_timer<=0)w->items->combo_timer=w->items->combo_level=0;
  if((int8_t)w->items->combo_level>14)w->items->combo_level=14;return;
 }
 if(state>=DIGIT&&state<=DIGIT_BLINK){(void)digit_tick(w,e,state);return;}
 if(state==POPUP_RUN){
  if(move(w,q,WMOVE_SYNC)<0)return;
  if((q->pending==0||q->pending==255)&&(int16_t)--a->timer<=0){
   if(!q->pending){q->pending=255;a->timer=10;}else schedule(q,CLEANUP);
  }
  if((int16_t)--q->watchdog<=0||outside(w,q,item_popup_bounds,1))schedule(q,CLEANUP);return;
 }
 if(state==FLY){
  if(move(w,q,WMOVE_SYNC)<0)return;
  if(animate(w,q)){q->palette_count=0;for(unsigned i=0;i<3;i++)q->palettes[i]=0;schedule(q,NEW);}tail(w,e,1);return;
 }
 if(state==TAKEN||state==RISE){
  if(move(w,q,WMOVE_SYNC)<0)return;int done=animate(w,q);
  if(state==RISE&&done){entity_delete(pool,e);return;}
  if(state==TAKEN&&(int16_t)--a->timer<=0)schedule(q,RISE);tail(w,e,0);return;
 }
 if(state==WEAPON||state==SUPPLY||(state>=CYCLE_SHOW&&state<=CYCLE_IN)){
  if(state==CYCLE_SHOW){if((int16_t)--a->timer<=0)schedule(q,CYCLE_OUT);}
  else if(state==CYCLE_OUT){if(a->timer>=8){fail(w);return;}q->scale_x=item_fade[a->timer++];if(a->timer==7)schedule(q,CYCLE_IN);}
  else if(state==CYCLE_IN){if(a->timer>=8){fail(w);return;}q->scale_x=item_fade[a->timer];if(!--a->timer)schedule(q,CYCLE_SHOW);}
  if(move(w,q,WMOVE_ITEM)<0)return;animate(w,q);int slot=pickup(w,q,0);
  if(slot>=0){
   if(state==SUPPLY){if(!supply(w,e,(unsigned)slot))return;}
   else if(state==WEAPON){static const uint8_t kinds[4]={4,3,1,2};if(!give_weapon(w,q,(unsigned)slot,kinds[q->kind],q->params[0]))return;}
   else{unsigned n=item_cycles[q->sequence][q->sequence_pos]&3;if(!give_weapon(w,q,(unsigned)slot,item_cycle_weapons[n],item_cycle_ammo[n]))return;q->scale_x=255;}
  }
  tail(w,e,1);return;
 }
 if(state==THROWN||state==BOUNCE){
  int landed=move(w,q,WMOVE_FALL);if(landed<0)return;if(landed)schedule(q,state==THROWN?BOUNCE:FOOD_WAIT);
  animate(w,q);if((int16_t)--a->timer<=0)schedule(q,FOOD_BLINK);
  if(state==BOUNCE&&!food_pickup(w,e))return;tail(w,e,1);return;
 }
 if(!physics(w,q))return;
 if(state==FOOD_BLINK_KEEP){
  uint8_t hi=(uint8_t)(a->timer>>8);if(hi&1)animate(w,q);
  a->timer=(uint16_t)((a->timer&255)|((uint16_t)(uint8_t)(hi-1)<<8));if(!hi)schedule(q,FREE);
 }else{
  int done=animate(w,q);
  if(state==TIMED){
   if(done){start(q,item_entries[IAN_TIMED_END]);a->timer=(uint16_t)((a->timer&255)|(20<<8));schedule(q,FOOD_BLINK_KEEP);}
   if((int16_t)--a->timer<=0){start(q,item_entries[IAN_TIMED_CHANGE]);a->timer=32767;}
  }else if(state==ROT){if(done)schedule(q,FREE);}
  else if(state==FOOD_WAIT||state==ROT_WAIT){if((int16_t)--a->timer<=0)schedule(q,state==FOOD_WAIT?FOOD_BLINK:ROT);}
  else{fail(w);return;}
 }
 if(!food_pickup(w,e))return;tail(w,e,1);
}
int item_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,enum ItemKind kind,const struct PlayerTransform*t,const uint8_t params[8],int counted){
 if((unsigned)kind>=ITEM_KIND_COUNT)return fail(w);
 struct NativeEntity*e=allocate(w,parent,order,t);if(!e)return 0;struct ItemActor*q=&e->payload.item;
 if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];q->kind=(uint8_t)kind;schedule(q,counted?COUNTED:NEW);return 1;
}
int item_spawn_drop(struct WeaponWorld*w,const struct PlayerTransform*t,uint8_t type,uint8_t qty,uint8_t life,int at_parent,int counted){
 if(!type)return 1;unsigned n=type&63;if(n>=30)n=10;
 /* Type 0 after masking (e.g. 0x40) selects the original FFFFFFFF entry.
  * Reject explicitly rather than turning it into an unrelated pickup. */
 static const uint8_t kinds[30]={255,ITEM_HMG,ITEM_FLAME,ITEM_SHOTGUN,ITEM_ROCKET,ITEM_AMMO,ITEM_GRENADES,ITEM_REPAIR,
  ITEM_FOOD_FIRST,ITEM_FOOD_FIRST+1,ITEM_FOOD_FIRST+2,ITEM_TIMED_FOOD,ITEM_FOOD_FIRST+3,ITEM_FOOD_FIRST+4,
  ITEM_FOOD_FIRST+5,ITEM_FOOD_FIRST+6,ITEM_ROT_FOOD,ITEM_FOOD_FIRST+7,ITEM_FOOD_FIRST+8,ITEM_FOOD_FIRST+9,
  ITEM_FOOD_FIRST+10,ITEM_FOOD_FIRST+11,ITEM_FOOD_FIRST+12,ITEM_FOOD_FIRST+13,ITEM_FOOD_FIRST+14,ITEM_FOOD_FIRST+15,
  ITEM_FOOD_FIRST+18,ITEM_FOOD_FIRST+19,ITEM_FOOD_FIRST+16,ITEM_FOOD_FIRST+17};
 if(!t||kinds[n]==255)return fail(w);
 struct PlayerTransform placed={0};transform_integer_copy(&placed,t);
 if(!at_parent)transform_integer_offset(&placed,(t->facing&1)?16:-16,0);
 uint8_t params[8]={qty,life,255,255,255,255,255,255};
 return item_spawn(w,w->projectile_group,ENTITY_FIRST_CHILD,(enum ItemKind)kinds[n],&placed,params,counted);
}
int item_spawn_combo_controller(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order){
 const struct PlayerTransform t={0};struct NativeEntity*e=allocate(w,parent,order,&t);if(!e)return 0;schedule(&e->payload.item,COMBO);return 1;
}
int item_spawn_template(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id==316)return item_spawn_combo_controller(w,parent,order);
 if(id==293)return crate_spawn(w,parent,order,t,params);
 if(id<284||id>315||id==292)return 0;
 return item_spawn(w,parent,order,(enum ItemKind)item_template_kinds[id-284],t,params,0);
}
int item_spawn_thrown_food(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerTransform*t,uint8_t owner,uint8_t damage_owner){
 struct NativeEntity*e=allocate(w,parent,ENTITY_FIRST_CHILD,t);if(!e)return 0;
 struct ItemActor*q=&e->payload.item;q->kind=ITEM_THROWN_FOOD;q->actor.owner=owner;q->actor.damage_owner=damage_owner;schedule(q,NEW);return 1;
}
int item_respawn_below(struct WeaponWorld*w,const struct PlayerTransform*t,uint8_t type,uint8_t quantity,uint8_t life){
 if(!t)return fail(w);struct PlayerTransform placed=*t;transform_integer_offset(&placed,0,4);
 return item_spawn_drop(w,&placed,type,quantity,life,1,0);
}
