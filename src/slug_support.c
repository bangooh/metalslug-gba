#include "slug_support.h"
#include "../generated/slug-support-data.h"
static int16_t word(int32_t v){return (int16_t)(uint16_t)v;}
static int fault(struct SlugSharedState*s,unsigned code){if(!s->fault)s->fault=(uint8_t)code;return 0;}
static void set_y(struct PlayerTransform*t,int16_t y){transform_integer_offset(t,0,word(y-transform_y_up(t)));}
static void set_xy(struct PlayerTransform*t,int16_t x,int16_t y){
 transform_integer_offset(t,word(x-(t->x>>8)),word(y-transform_y_up(t)));
}
static struct TerrainMotion contact_motion(struct SlugContact*c){
 return (struct TerrainMotion){.transform=&c->transform,.hit=&c->hit,.vx=c->vx,.vy=c->vy,.ax=c->ax,.ay=c->ay,
  .slope=c->slope,.ground_tag=c->ground_tag,.collision_flags=c->lifecycle_flags,.platform_ids=c->platform_ids,
  .previous_direction=c->previous_direction};
}
static void contact_commit(struct SlugContact*c,const struct TerrainMotion*m){
 c->vx=m->vx;c->vy=m->vy;c->ax=m->ax;c->ay=m->ay;c->slope=m->slope;c->ground_tag=m->ground_tag;c->platform_ids=m->platform_ids;
}
static int foot(struct SlugSharedState*s,struct TerrainWorld*w,struct SlugContact*c){
 struct TerrainMotion m=contact_motion(c);int result=terrain_foot_contact(w,&m);contact_commit(c,&m);
 if(result<0)fault(s,1);return result;
}
static int move_contact(struct SlugSharedState*s,struct TerrainWorld*w,struct SlugContact*c,enum WeaponMove policy){
 struct TerrainMotion m=contact_motion(c);struct WeaponContact hit;
 int result=terrain_move(w,&m,policy,&hit);contact_commit(c,&m);if(result<0)fault(s,1);return result;
}
void slug_chassis_create(struct SlugSharedState*s,int alternate,int16_t camera_x,int16_t camera_y){
 if(alternate)s->lifecycle_flags|=8;
 for(unsigned i=0;i<3;i++){
  struct SlugContact*c=&s->contacts[i];*c=(struct SlugContact){.platform_ids=255,.slope=-1,.ground_tag=-1,
   /* $05DC1C sentinels are read by the body before the first child tick. */
   .surface_y=word(-1-camera_y),.old_y=word(-1-camera_y),.old_x=word(-1+camera_x),
   .correction=-1,.direction_mask=65535,.previous_direction=-1,
   .hit={.flags=64,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255},.lifecycle_flags=alternate?0:4};
  c->transform=s->actor.transform;transform_integer_copy(&c->transform,&s->actor.transform);set_y(&c->transform,s->ground_y_up);
 }
}
static int contact_enter(struct SlugSharedState*s,struct TerrainWorld*w,unsigned index){
 struct SlugContact*c=&s->contacts[index];c->entered=1;c->offset_x=slug_contact_offsets[index];
 if(index==2)transform_integer_offset(&c->transform,c->offset_x,0);
 c->hit.type_flags|=2;c->transform.priority=0xf010;c->ax=c->ay=0;c->correction=0;c->drop_through=0;
 int grounded=foot(s,w,c);if(grounded<0)return 0;c->grounded=(uint8_t)grounded;
 if(index<2){
  transform_integer_offset(&c->transform,c->offset_x,0);
  c->previous_direction=index?-1:127;c->direction_mask=index?0x8000:0;
 }
 return 1;
}
/* $03050C uses both saved positions, including the already-updated B
 * contact and the not-yet-updated A contact. Do not snapshot the whole trio. */
static void contact_sync_flags(struct SlugSharedState*s,struct SlugContact*c){
 int equal=s->contacts[0].old_y==s->contacts[1].old_y;
 int correction=s->contacts[0].correction|s->contacts[1].correction|s->contacts[2].correction;
 if(equal||correction)c->lifecycle_flags&=(uint8_t)~48u;else c->lifecycle_flags|=48;
}
static void contact_velocity_x(struct SlugSharedState*s,struct SlugContact*c){
 c->vx=word(word((s->actor.transform.x>>8)+c->offset_x-(c->transform.x>>8))*256);
 if(c->direction_mask!=65535&&(((uint16_t)s->actor.vx&0x8000)^c->direction_mask)==0)
  c->hit.flags=(uint8_t)((c->hit.flags&~48u)|(s->actor.hit.flags&48));
 else /* $0304AE is NEG.B #$30 -> $D0, deliberately not NOT -> $CF. */
  c->hit.flags=(uint8_t)((c->hit.flags&0xd0)|(c->lifecycle_flags&48));
}
static int contact_too_far(struct SlugSharedState*s,struct SlugContact*c){
 int16_t delta=word(transform_y_up(&c->transform)-s->ground_y_up);
 if(s->actor.ground_slope==-1){
  if(c->vx&&((uint16_t)(c->offset_x^c->vx)&0x8000))return delta>=0;
  return delta>=24;
 }
 return delta>18||delta<-18;
}
static int16_t contact_surface_target(const struct SlugSharedState*s,const struct SlugContact*c){
 int16_t target=s->ground_y_up,slope=s->actor.ground_slope;
 if(slope){int16_t off=slope<0?word(-c->offset_x):c->offset_x;off=word(off-1);
  if(slope==0x1000||slope==-0x1000)off=(int16_t)(off>>2);target=word(target+off);}
 return target;
}
static void contact_velocity_y(struct SlugSharedState*s,struct SlugContact*c){
 if(s->lifecycle_flags&8){c->vy=word(c->vy-0xc0);return;}
 c->vy=s->actor.vy;int16_t distance=word(transform_y_up(&s->contacts[1].transform)-transform_y_up(&s->contacts[0].transform));
 if(distance<0)distance=word(-distance);
 if((uint16_t)distance<28)c->vy=word(c->vy+word(word(s->ground_y_up-transform_y_up(&c->transform))*16));
}
static int contact_frame(struct SlugSharedState*s,struct TerrainWorld*w,unsigned index){
 struct SlugContact*c=&s->contacts[index];
 if(s->actor.hit.flags&8)c->correction=0;
 c->old_x=(int16_t)(c->transform.x>>8);c->old_y=transform_y_up(&c->transform);
 if(index==2){
  set_xy(&c->transform,(int16_t)(s->actor.transform.x>>8),s->ground_y_up);
  int grounded=foot(s,w,c);if(grounded<0)return 0;c->grounded=(uint8_t)grounded;
 }else{
  /* $02ABCC always clears C; every side contact synchronizes here. */
  struct TerrainMotion m=contact_motion(c);if(!terrain_synchronize(w,&m))return fault(s,1);contact_commit(c,&m);
  contact_velocity_x(s,c);
  if(c->grounded){
   contact_sync_flags(s,c);
   int changed=move_contact(s,w,c,WMOVE_GROUND);if(changed<0)return 0;if(changed)c->hit.events|=16;
   int grounded=foot(s,w,c);if(grounded<0)return 0;if(!grounded)c->grounded=0;
   if(s->actor.ground_slope==-1)c->grounded=0;
   if(contact_too_far(s,c)){c->vy=0;c->grounded=0;c->drop_through=255;}
   if(!c->grounded)c->vx=0;
  }
  if(!c->grounded){
   if(s->actor.ground_slope==-1){
    contact_velocity_y(s,c);m=contact_motion(c);
    int shape=terrain_shape_at(w,&m,(int16_t)(c->transform.x>>8),transform_y_up(&c->transform));
    if(shape<0)return fault(s,1);
    if(!shape){shape=terrain_shape_at(w,&m,(int16_t)(c->transform.x>>8),word(transform_y_up(&c->transform)-4));
     if(shape<0)return fault(s,1);if(!shape)c->drop_through=0;}
    contact_commit(c,&m);
   }else{
    int16_t delta=word(contact_surface_target(s,c)-transform_y_up(&c->transform));
    if(delta>=0)c->drop_through=0;
    c->vy=word(c->vy+word(word(delta*64)-(c->vy>>1)));
   }
   /* $027C2A clears vertical velocity on contact; $027D50 preserves it. */
   int landed=move_contact(s,w,c,c->drop_through?WMOVE_FALL:WMOVE_BODY);if(landed<0)return 0;
   if(landed){c->grounded=1;c->drop_through=0;}
   else if((uint16_t)c->vy<8){int grounded=foot(s,w,c);if(grounded<0)return 0;
    if(grounded){c->grounded=1;c->drop_through=0;}}
  }
 }
 c->surface_y=word(transform_y_up(&c->transform)+c->correction);c->correction=0;return 1;
}
int slug_contact_tick(struct SlugSharedState*s,struct TerrainWorld*w,unsigned index){
 if(s->fault||w->fault)return 0;if(index>=3)return fault(s,1);
 struct SlugContact*c=&s->contacts[index];c->hit.events&=1;
 if(!c->entered&&!contact_enter(s,w,index))return 0;
 if(!contact_frame(s,w,index))return 0;c->hit.flags&=(uint8_t)~64u;c->hit.events&=0xfa;return 1;
}
int slug_contacts_tick(struct SlugSharedState*s,struct TerrainWorld*w){
 static const uint8_t order[3]={1,0,2};
 for(unsigned i=0;i<3;i++)if(!slug_contact_tick(s,w,order[i]))return 0;
 return 1;
}

int slug_ground_contact(struct SlugSharedState*s,struct TerrainWorld*w,unsigned*contact_count){
 struct WeaponActor*a=&s->actor;
 struct TerrainMotion m={.transform=&a->transform,.hit=&a->hit,.collision_flags=s->lifecycle_flags,.platform_ids=a->platform_ids};
 if(contact_count)*contact_count=0;
 if(a->hit.type_flags&64){int found=terrain_foot_contact(w,&m);a->platform_ids=m.platform_ids;
  if(found<0)fault(s,1);if(contact_count&&found>0)*contact_count=3;return found;}
 s->lifecycle_flags&=(uint8_t)~8u;
 int found=terrain_contact_at(w,&m,(int16_t)(a->transform.x>>8),s->ground_y_up);a->platform_ids=m.platform_ids;
 if(found<0){fault(s,1);return -1;}
 if(found){if(contact_count)*contact_count=3;}
 else{
  unsigned count=s->contacts[0].grounded+s->contacts[1].grounded+s->contacts[2].grounded;
  if(contact_count)*contact_count=count;
  if(count>=2){s->lifecycle_flags|=8;s->ground_y_up=transform_y_up(&a->transform);found=1;}
 }
 for(unsigned i=0;i<3;i++){
  struct PlayerTransform*t=&s->contacts[i].transform;
  int16_t y=word((uint16_t)word(transform_y_up(t)+w->camera_y)&511);
  set_y(t,word(y-w->camera_y));
 }
 return found;
}
int16_t slug_slope_product(const struct SlugSharedState*s,int16_t*angle){
 int16_t dx=word((s->contacts[0].transform.x>>8)-(s->contacts[1].transform.x>>8));
 int16_t dy=word(s->contacts[0].surface_y-s->contacts[1].surface_y);
 if(angle)*angle=dy>64?64:dy<-64?-64:dy;
 return word((int32_t)dx*dy); /* callers compare only the low MULS word */
}
unsigned slug_slope_index(const struct SlugSharedState*s){
 int16_t slope=slug_slope_product(s,0);return slope>=900?3:slope<=-900?4:slope>=90?1:slope<=-90?2:0;
}
int slug_update_angle(struct SlugSharedState*s){
 int16_t angle;(void)slug_slope_product(s,&angle);if(angle==s->angle)return 0;
 s->angle=angle;return slug_flat_angles[angle+64]<1;
}
void slug_damage_reset(struct SlugSharedState*s){s->damage_cursor=0;s->damage_elapsed=0;}
int slug_damage_display(struct SlugSharedState*s,unsigned program,int*finished){
 if(finished)*finished=0;if(s->fault)return 0;if(program>=3)return fault(s,2);
 s->damage_program=(uint8_t)program;const struct SlugDisplayProgram*p=&slug_display_programs[program];
 for(unsigned budget=0;budget<p->count;budget++){
  if(s->damage_cursor>=p->count)return fault(s,2);
  const struct SlugDisplayCommand*c=&slug_display_commands[p->first+s->damage_cursor];
  if(c->op==SDISPLAY_END){if(finished)*finished=1;return 1;}
  if(c->next<p->first||c->next>=p->first+p->count)return fault(s,2);
  if(c->op==SDISPLAY_WAIT){
   s->actor.display_flags=(uint8_t)((s->actor.display_flags&~8u)|c->value);
   s->damage_elapsed++;
   if((int8_t)s->damage_elapsed>=(int8_t)c->duration){s->damage_elapsed=0;s->damage_cursor=(uint16_t)(c->next-p->first);}
   return 1;
  }
  if(c->op==SDISPLAY_PALETTE){if(c->value>=4)return fault(s,2);s->actor.palette=s->palette_slots[c->value];}
  else if(c->op!=SDISPLAY_JUMP)return fault(s,2);
  s->damage_cursor=(uint16_t)(c->next-p->first);
 }
 return fault(s,2);
}
int slug_update_chassis(struct SlugSharedState*s,int preserve_index,unsigned*index){
 if(s->fault)return 0;unsigned old=s->slope_index;hit_begin_attack(&s->actor.hit);
 if(s->actor.owner<2)s->previous_owner=s->actor.owner;s->turn_timer=0;
 if(!(s->actor.hit.flags&1)&&(uint16_t)s->actor.hit.hp<=1&&!slug_damage_display(s,0,0))return 0;
 s->slope_index=(uint16_t)slug_slope_index(s);if(s->cooldown)s->cooldown--;
 int16_t y0=s->contacts[0].surface_y,y1=s->contacts[1].surface_y,dy=word(y0-y1),base=y1;
 int negative=dy<0;if(negative)base=y0;dy=word(dy*32);
 int16_t dx=word((s->contacts[0].transform.x>>8)-(s->contacts[1].transform.x>>8));
 int quotient=dx?dy/dx:0;
 /* DIVS overflow leaves the destination word unchanged on the 68000. */
 int16_t rise=quotient>32767||quotient<-32768?dy:word(quotient);rise=(int16_t)(rise>>1);
 if(negative)rise=word(-rise);set_y(&s->actor.transform,word(base+rise));
 /* The $02AA0E wrapper restores $94 as well as its returned index. */
 if(preserve_index)s->slope_index=(uint16_t)old;if(index)*index=s->slope_index;return 1;
}
void slug_chassis_sync_flags(struct SlugSharedState*s){
 /* $0304C4 loads $78 twice: the centre contact is intentionally excluded. */
 int equal=transform_y_up(&s->contacts[0].transform)==transform_y_up(&s->contacts[1].transform);
 if(equal||(s->contacts[0].correction|s->contacts[1].correction))s->actor.hit.flags&=(uint8_t)~48u;
 else s->actor.hit.flags|=48;
}
int slug_chassis_wall(const struct SlugSharedState*s){
 return (s->contacts[0].hit.events&32)?1:(s->contacts[1].hit.events&32)?-1:0;
}
int slug_chassis_step_mismatch(const struct SlugSharedState*s,int16_t camera_y){
 if((s->contacts[0].hit.events|s->contacts[1].hit.events)&16)return 0;
 /* $03078A SUBX/ADD/EOR forms the magnitude using unsigned borrow.
  * Reconstruct source screen words before that comparison. */
 uint16_t a=(uint16_t)word(transform_y_up(&s->contacts[0].transform)+camera_y);
 uint16_t b=(uint16_t)word(transform_y_up(&s->contacts[1].transform)+camera_y);
 unsigned difference=a<b?b-a:a-b;
 switch(difference){case 0:case 1:case 2:case 7:case 8:case 9:case 31:case 32:case 33:case 34:return 0;default:return 1;}
}
int slug_chassis_higher_previous(const struct SlugSharedState*s,int16_t camera_y,int16_t*height){
 if(slug_chassis_wall(s))return 0;
 int first=word(s->contacts[0].old_y+camera_y)>=word(s->contacts[1].old_y+camera_y);
 *height=s->contacts[first?0:1].old_y;return first?1:-1;
}
int slug_angle_sprite(const struct SlugSharedState*s,uint8_t*frame){
 /* $02E0BE etc.: signed shift/negate, then an unsigned upper bound. */
 uint16_t position=(uint16_t)word(32-(s->angle>>1));if(position>64)position=64;
 *frame=(uint8_t)(position>>2);return slug_angle_sprites[position];
}
int slug_cannon_ready(const struct SlugSharedState*s,struct PlayerSourceInput input,unsigned layout){
 if(!s->ammo||s->cannon_count>=2||s->cooldown)return 0;
 unsigned mask=layout==4?PB_FIRE:layout==0?PB_GRENADE:0xf0; /* $05CDFC: ANY action button */
 return !!(input.actions&mask);
}
void slug_ammo_consume(struct SlugSharedState*s){if(s->ammo)s->ammo--;}
void slug_ammo_add(struct SlugSharedState*s,uint8_t quantity){if(quantity!=255)s->ammo=(uint8_t)(s->ammo+quantity);}
int slug_hp_add(struct SlugSharedState*s,int16_t quantity){
 if(s->actor.hit.hp>=48)return 0;int16_t hp=word(s->actor.hit.hp+quantity);s->actor.hit.hp=hp>48?48:hp;return 1;
}
void slug_turn_timer(struct SlugSharedState*s,uint8_t layer2){
 if(layer2&PB_FIRE){if(s->turn_timer!=20)s->turn_timer++;}else s->turn_timer=s->turn_timer==20?255:0;
}
int slug_mount_candidate(struct SlugSharedState*s,struct PlayerBodyState*p){
 if(!s->present||p->character>1||(p->actor.action_flags&1)||!(p->status_flags&32)||!(p->status_flags&2)||p->vy>=0)return 0;
 s->actor.damage_owner=(uint8_t)(p->character+1);p->actor.action_flags|=1;slug_rider_refresh(s,p);return 1;
}
void slug_rider_refresh(struct SlugSharedState*s,const struct PlayerBodyState*p){s->rider_heartbeat=255;s->actor.owner=p->actor.owner;}
int slug_rider_absent(struct SlugSharedState*s){
 if(!s->rider_heartbeat){s->actor.damage_owner=255;return 1;}s->rider_heartbeat=0;return 0;
}
int slug_ridden_by(const struct SlugSharedState*s,unsigned player){return player<2&&s->actor.damage_owner==player+1;}
int slug_rider_parameter(const struct SlugSharedState*s,uint8_t owner,uint32_t*value){
 if(!s->present||s->previous_owner!=owner){*value=0;return 0;}
 *value=s->rider_parameter==0xffffffff?0x10000:s->rider_parameter;return 1;
}
unsigned slug_impact_side(const struct SlugSharedState*s,int16_t camera_x){
 if(!(s->actor.hit.flags&8))return 0;if(!s->actor.defense)return 1;
 const struct HitRect*r=&s->actor.defense[0].rect;int16_t x=word((s->actor.transform.x>>8)-camera_x);
 int16_t centre=word(word(x+r->left)+word(x+r->right));centre=(int16_t)(centre>>1);
 return (uint16_t)word(s->actor.hit.impact_x-camera_x)<(uint16_t)centre?1:2;
}
