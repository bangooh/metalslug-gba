#include "enemies.h"
#include "items.h"
#include "../generated/slug-projectile-programs.h"
static int16_t word(int v){return (int16_t)(uint16_t)v;}
int enemy_fault(struct WeaponWorld*w){if(!w->fault)w->fault=30;return 0;}
struct NativeEntity*enemy_allocate(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,EntityTick tick){
 if(!w->enemies||!w->interactions||!w->entities||!t){enemy_fault(w);return 0;}
 struct NativeEntity*e=entity_spawn(w->entities,parent,ENTITY_ENEMY,order,tick);if(!e){enemy_fault(w);return 0;}
 struct EnemyActor*q=&e->payload.enemy;
 *q=(struct EnemyActor){.entry=1,.captor_done=255,.scale_x=255,.scale_y=255,.gib_delays=255,.dead=255,.support_slope=65535,.source_flags=t->display_group,
  .params={255,255,255,255,255,255,255,255},.sound_command=65535,
  .infantry={.origin=-1,.radius=65535,.cooldown=65535,.lifetime=65535,.probability=65535,
   .target=-2,.flags=255,.mood=255,.abilities=255,.weapon=255,.anchor=255,.key=255,
   .retreat=65535,.advance=65535,.fear=65535,.leap=65535,.crouch=65535,.distance=65535},
  .actor={.active=1,.owner=255,.damage_owner=255,.score_owner=255,.palette=65535,.platform_ids=255,.ground_slope=-1,.ground_tag=-1,
   .phase=255,.variant=255,.ttl=255,.raw_surface=255,.origin_y_up=-1,.trail_drawing=65535,
   .hit={.id=e->index,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
   .animation={.cursor=65535,.drawing=65535,.last_draw_cursor=65535}}};
 q->actor.transform=*t;transform_integer_copy(&q->actor.transform,t);return e;
}
void enemy_schedule(struct EnemyActor*q,unsigned state){q->actor.state=(uint8_t)state;q->entry=1;}
void enemy_start(struct EnemyActor*q,enum EnemyAnimation a){animation_begin(&q->actor.animation,enemy_entries[a]);q->actor.movement_flags|=32;}
void enemy_defense(struct EnemyActor*q,unsigned b){
 if(b==65535){q->actor.defense=0;q->actor.defense_count=0;return;}
 q->actor.defense=enemy_bindings[b].defense;q->actor.defense_count=enemy_bindings[b].count;
}
void enemy_attack_binding(struct EnemyActor*q,unsigned b){
 if(b==65535){q->actor.attack=0;return;}
 q->attack_set=(struct HitAttackSet){enemy_bindings[b].attack,enemy_bindings[b].count};q->actor.attack=&q->attack_set;
}
int enemy_attack(struct WeaponWorld*w,struct WeaponActor*a){
 if(!a->attack)return 1;if(!w->attack)return enemy_fault(w);
 a->hit.x=word(a->transform.x>>8);a->hit.y=transform_y_up(&a->transform);a->hit.flip=a->transform.facing;
 for(unsigned i=0;i<a->attack->count;i++)if(!w->attack(w->scene,&a->hit,&a->attack->records[i],0))return enemy_fault(w);return 1;
}
int enemy_swapped_move(struct WeaponWorld*w,struct EnemyActor*q,enum WeaponMove m){
 int16_t old=q->actor.ground_slope;q->actor.ground_slope=(int16_t)q->support_slope;
 int r=interaction_move(w,&q->actor,m);q->support_slope=(uint16_t)q->actor.ground_slope;q->actor.ground_slope=old;return r;
}
int enemy_ground_status(struct WeaponWorld*w,struct EnemyActor*q,int point){
 return w->interactions&&terrain_weapon_ground_status(w->interactions->terrain,&q->actor,&q->grounded,point)?1:enemy_fault(w);
}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read_field(void*ctx,unsigned field,unsigned width){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(field==ENF_3B&&width==1)return q->actor.animation.pose;
 if(field==ENF_3A&&width==1)return q->actor.transform.facing;
 if(field==ENF_20&&width==1)return q->infantry.signal;
 if(field==ENF_70&&width==2)return q->selector;
 if(field==ENF_74&&width==2)return q->sound_command;
 if(field==ENF_28&&width==2)return (uint16_t)q->actor.vx;
 enemy_fault(c->w);return 0;
}
static int write_field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t value){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(f==ENF_3A&&width==1){
  if(op==AN_FIELD_XOR)q->actor.transform.facing^=(uint8_t)value;
  else if(op==AN_FIELD_AND)q->actor.transform.facing&=(uint8_t)value;
  else if(op==AN_FIELD_SET)q->actor.transform.facing=(uint8_t)value;
  else return enemy_fault(c->w);return 1;
 }
 if(op!=AN_FIELD_SET)return enemy_fault(c->w);
 if(f==ENF_3B&&width==1)q->actor.animation.pose=(uint8_t)value;
 else if(f==ENF_20&&width==1)q->infantry.signal=(uint8_t)value;
 else if(f==ENF_74&&width==2)q->sound_command=(uint16_t)value;
 else if(f==ENF_28&&width==2)q->actor.vx=(int16_t)value;
 else return enemy_fault(c->w);return 1;
}
static int bind(void*ctx,unsigned kind,uint32_t b){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(kind==AN_ATTACK&&b==0xffffffffu){enemy_attack_binding(q,65535);hit_begin_attack(&q->actor.hit);return 1;}
 if(kind==AN_ATTACK){
  if(b>=sizeof(enemy_bindings)/sizeof(*enemy_bindings)||!enemy_bindings[b].attack)return enemy_fault(c->w);
  enemy_attack_binding(q,b);hit_begin_attack(&q->actor.hit);return 1;
 }
 if(kind!=AN_DEFENSE)return enemy_fault(c->w);
 if(b==0xffffffffu){enemy_defense(q,65535);return 1;}
 if(b>=sizeof(enemy_bindings)/sizeof(*enemy_bindings)||!enemy_bindings[b].defense)return enemy_fault(c->w);
 enemy_defense(q,b);return 1;
}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned callback,struct Animation*animation){
 (void)animation;struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(callback){
 case ENC_5080A:return allen_emit(c->w,c->e,ALLEN_FLASH_LEVEL);
 case ENC_50816:return allen_emit(c->w,c->e,ALLEN_FLASH_DIAGONAL);
 case ENC_50822:return allen_emit(c->w,c->e,ALLEN_BULLET_LEVEL);
 case ENC_5087C:return allen_emit(c->w,c->e,ALLEN_BULLET_DIAGONAL);
 case ENC_5063E:return allen_emit(c->w,c->e,ALLEN_GRENADE);
 case ENC_4A0D4:a->palette=c->w->enemies->blood_enabled?0xe:0x2f;return 1;
 case ENC_4A0EA:a->palette=0x1a;return 1;
 case ENC_4A128:a->palette=0x185;return 1;
 case ENC_4A110:a->palette=0x15c;return 1;
 case ENC_4A108:a->palette=0x14;return 1;
 case ENC_4A120:a->palette=0xe;return 1;
 case ENC_4A118:a->palette=0x21;return 1;
 case ENC_2B6114:transform_integer_offset(&a->transform,0,36);return 1;
 case ENC_2B611C:transform_integer_offset(&a->transform,0,-36);return 1;
 case ENC_283D8:return enemy_attack(c->w,a);
 case ENC_5E8DA:if(!enemy_attack(c->w,a))return 0;return !(a->hit.events&2)||sound(ctx,0x10fa,0);
 case ENC_56066:return enemy_spawn_ordnance(c->w,entity_ref(c->e),&a->transform,ENEMY_BOUNCE,0,q->source_flags);
 case ENC_56438:return enemy_spawn_ordnance(c->w,entity_ref(c->e),&a->transform,ENEMY_MORTAR,0,q->source_flags);
 case ENC_55BCE:return enemy_spawn_ordnance(c->w,entity_ref(c->e),&a->transform,ENEMY_GRENADE,a->variant,q->source_flags);
 case ENC_56844:{int ok=enemy_spawn_ordnance(c->w,entity_ref(c->e),&a->transform,ENEMY_ROLLER,0,q->source_flags);q->infantry.origin=0;return ok;}
 case ENC_4A1BA:return sound(ctx,q->sound_command,0);
 case ENC_283CA:hit_begin_attack(&a->hit);return 1;
 case ENC_4B052:transform_integer_offset(&a->transform,0,2);a->vx=a->transform.facing&1?8:-8;a->vy=0x180;a->ay=-24;a->ax=0;return 1;
 case ENC_267E2:
  a->vx=a->vy=a->ax=a->ay=0;a->transform.x=(a->transform.x>>8)*256;a->transform.y=(512-(int32_t)transform_y_up(&a->transform))*256;return 1;
 default:return enemy_fault(c->w);
 }
}
static int delay(void*ctx,unsigned f,unsigned pose,uint8_t*value){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(f!=ENF_76||q->gib_delays>=8||pose>=6)return enemy_fault(c->w);*value=human_gib_delays[q->gib_delays][pose];return 1;
}
int enemy_animate(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;
 struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read_field,.write=write_field,.bind=bind,.call=call,.sound=sound,.delay=delay};
 int done=animation_step(&a->animation,&enemy_program,&o);if(a->animation.fault)enemy_fault(w);return done;
}
void enemy_world_frame(struct EnemyWorld*w){
 struct DeathPositionRing*r=&w->deaths;r->first=r->end;r->end=r->write;r->count=0;
 struct EnemyZones*z=&w->zones;z->first=z->end;z->end=z->write;z->count=0;
}
void enemy_publish_death(struct WeaponWorld*w,const struct WeaponActor*a){
 struct DeathPositionRing*r=&w->enemies->deaths;r->records[r->write]=(struct DeathPosition){word((a->transform.x>>8)-w->camera_x),word(transform_y_up(&a->transform)+w->camera_y)};
 if(r->count<4){r->write=(r->write+1)&7;r->count++;}
}
int enemy_near_death(struct WeaponWorld*w,const struct WeaponActor*a,int*behind){
 const struct DeathPositionRing*r=&w->enemies->deaths;
 for(unsigned i=r->first;i!=r->end;i=(i+1)&7){
  int16_t dx=word(r->records[i].x-word((a->transform.x>>8)-w->camera_x)),dy=word(r->records[i].y_up-word(transform_y_up(&a->transform)+w->camera_y));
  if((uint16_t)word(dx+64)>=128||(uint16_t)word(dy+48)>=96)continue;
  *behind=(!(a->transform.facing&1))^(dx<0);return 1;
 }return 0;
}
int enemy_player_live(const struct PlayerBodyState*p){return p&&!p->life_retired&&(p->actor.lifecycle_flags&128);}
static uint16_t distance(int16_t dx,int16_t dy){
 /* $056B12 complements negative words without adding one. */
 uint16_t x=dx<0?(uint16_t)~dx:(uint16_t)dx,y=dy<0?(uint16_t)~dy:(uint16_t)dy;
 return (uint16_t)(x>=y?x+(y>>1):y+(x>>1));
}
int enemy_nearest_player(struct WeaponWorld*w,const struct WeaponActor*a,uint16_t*d){
 int slot=-1;*d=65535;
 for(unsigned i=0;i<2;i++)if(enemy_player_live(w->players[i])){
  const struct PlayerTransform*t=&w->players[i]->actor.transform;
  /* Preserve the original P1 self-Y subtraction at $056B52. */
  uint16_t n=distance(word((a->transform.x>>8)-(t->x>>8)),i?word(transform_y_up(&a->transform)-transform_y_up(t)):0);
  if(slot<0||n<=*d){slot=(int)i;*d=n;}
 }return slot;
}
int enemy_nearest_x(struct WeaponWorld*w,const struct WeaponActor*a,const struct HitRect*filter){
 int slot=-1;int16_t best=0;
 for(unsigned i=0;i<2;i++)if(enemy_player_live(w->players[i])){
  const struct PlayerTransform*t=&w->players[i]->actor.transform;int16_t dx=word((t->x>>8)-(a->transform.x>>8)),dy=word(transform_y_up(t)-transform_y_up(&a->transform));
  if(filter){int16_t x=a->transform.facing&1?word(-dx):dx,y=a->transform.facing&2?word(-dy):dy;
   if(x<filter->left||x>filter->right||y<filter->bottom||y>filter->top)continue;}
  int16_t n=dx<0?word(-dx):dx;if(slot<0||n<best){slot=(int)i;best=n;}
 }return slot;
}
uint8_t enemy_angle(int16_t vx,int16_t vy){
 int16_t hx=vx>>1,hy=vy>>1;uint8_t sx=hx<0?255:0,sy=hy<0?255:0;
 uint8_t x=(uint8_t)hx^sx,y=(uint8_t)hy^sy,swap=((uint8_t)(x-y)&128)?255:0;
 if(x>=y){uint8_t t=x;x=y;y=t;}uint8_t angle;
 if(!x)angle=swap&64;else{angle=slug_atan_exp[(uint8_t)(slug_atan_log[x]-slug_atan_log[y])];angle=(uint8_t)((angle^swap)-swap+32);}
 uint8_t sign=sx^sy;return (uint8_t)((angle^sign)-sign+(sx&128));
}
int enemy_damage_score(struct WeaponWorld*w,const struct HitDamage*event){
 if(event->target>=ENTITY_CAPACITY||event->attacker>=ENTITY_CAPACITY)return enemy_fault(w);
 struct NativeEntity*t=&w->entities->nodes[event->target],*s=&w->entities->nodes[event->attacker];
 if(!t->active||!s->active)return enemy_fault(w);
 if(t->kind==ENTITY_PLAYER||t->kind==ENTITY_SLUG)return 1;
 struct WeaponActor*target=entity_weapon_actor(t);if(!target)return enemy_fault(w);
 if(!target->hit.last_kind)return 1;uint8_t owner;
 if(s->kind==ENTITY_PLAYER||s->kind==ENTITY_PLAYER_ARM)owner=(s->kind==ENTITY_PLAYER?&s->payload.player.actor:&s->payload.arm)->owner;
 else{struct WeaponActor*a=entity_weapon_actor(s);if(!a)return enemy_fault(w);owner=a->owner;}
 if((int8_t)owner<0)return 1;target->score_owner=owner;
 if(!(target->display_flags&2))return 1;
 return w->items&&item_score_add(w->items,owner,enemy_hit_scores[target->hit.last_kind&31])?1:enemy_fault(w);
}
