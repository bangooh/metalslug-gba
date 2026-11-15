#include "tank_internal.h"
#include "m4_internal.h"
#include "slug_support.h"
#include "../generated/rocket-vehicle-programs.h"
/* $06BA2C..$06D654. The source labels $06C272/$06C37C as death/wreck,
 * but both are live non-flat-terrain states, returning to drive/idle. */
enum State {NEW,DRIVE,STOP,TURN,IDLE,ALERT,COUNT,NEXT,FIRE,RECOVER,HURT,SLOPE_DRIVE,SLOPE_IDLE,DEAD,FADE,
 WEAPON_NEW,WEAPON_IDLE,WEAPON_SLOPE,WEAPON_AIM,WEAPON_FIRE,WEAPON_CENTER,WEAPON_HOLD,WEAPON_DEAD,WEAPON_FLY,MUZZLE,FALL,ROCKET,ROCKET_HIT,BLAST,DELETE,DELETE_TREE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void contact_tick(struct EntityPool*,struct NativeEntity*,void*);
static void contact_delete(struct EntityPool*p,struct NativeEntity*e,void*world){(void)world;entity_delete_children(p,e);entity_delete(p,e);}
static void schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.rocket_vehicle;for(unsigned i=0;i<sizeof(e->payload.enemy.rocket_vehicle);i++)p[i]=255;schedule(e,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
static void start_entry(struct EnemyActor*q,uint16_t n){if(n!=65535){animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}}
static void start(struct EnemyActor*q,unsigned n){start_entry(q,rocket_vehicle_entries[n]);}
static void defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:rocket_vehicle_bindings[n].defense;q->actor.defense_count=n==65535?0:rocket_vehicle_bindings[n].count;}
static void attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){rocket_vehicle_bindings[n].attack,rocket_vehicle_bindings[n].count};q->actor.attack=&q->attack_set;}}
static void palette(struct EnemyActor*q){q->actor.palette=rocket_vehicle_palettes[q->rocket_vehicle.variant&3];q->actor.hit_palette=0x7b;}
static struct NativeEntity*contact(struct WeaponWorld*w,struct EnemyActor*q,unsigned i){struct NativeEntity*n=entity_get(w->entities,(struct EntityRef){q->rocket_vehicle.contacts[i][0],q->rocket_vehicle.contacts[i][1]});if(!n||n->kind!=ENTITY_TERRAIN_CONTACT){enemy_fault(w);return 0;}return n;}
static int chassis_read(struct WeaponWorld*w,struct EnemyActor*q,struct SlugSharedState*s){*s=(struct SlugSharedState){.actor=q->actor,.ground_y_up=q->rocket_vehicle.ground,.lifecycle_flags=q->lifecycle_flags};for(unsigned i=0;i<3;i++){struct NativeEntity*n=contact(w,q,i);if(!n)return 0;s->contacts[i]=n->payload.contact;}return 1;}
static void chassis_write(struct WeaponWorld*w,struct EnemyActor*q,const struct SlugSharedState*s){q->actor=s->actor;q->rocket_vehicle.ground=s->ground_y_up;q->lifecycle_flags=s->lifecycle_flags;for(unsigned i=0;i<3;i++){struct NativeEntity*n=contact(w,q,i);if(!n)return;n->payload.contact=s->contacts[i];}}
static int chassis_create(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct SlugSharedState s={.actor=q->actor,.ground_y_up=q->rocket_vehicle.ground,.lifecycle_flags=q->lifecycle_flags};slug_chassis_create(&s,1,w->camera_x,w->camera_y);q->lifecycle_flags=s.lifecycle_flags;
 static const uint8_t order[]={2,0,1};for(unsigned j=0;j<3;j++){unsigned i=order[j];struct NativeEntity*n=entity_spawn(w->entities,entity_ref(e),ENTITY_TERRAIN_CONTACT,ENTITY_FIRST_CHILD,contact_tick);if(!n)return enemy_fault(w);n->payload.contact=s.contacts[i];n->payload.contact.hit.id=n->index;q->rocket_vehicle.contacts[i][0]=n->index;q->rocket_vehicle.contacts[i][1]=n->generation;}return 1;}
static void contact_tick(struct EntityPool*pool,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct NativeEntity*p=entity_get(pool,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return;}struct EnemyActor*q=&p->payload.enemy;struct SlugSharedState s;if(!chassis_read(w,q,&s))return;unsigned i;for(i=0;i<3;i++)if(q->rocket_vehicle.contacts[i][0]==e->index)break;if(i>=3||!slug_contact_tick(&s,w->interactions->terrain,i)){enemy_fault(w);return;}e->payload.contact=s.contacts[i];}
static int ground_move(struct WeaponWorld*w,struct SlugSharedState*s,enum WeaponMove policy){struct WeaponActor*a=&s->actor;struct TerrainMotion m={.transform=&a->transform,.hit=&a->hit,.vx=a->vx,.vy=a->vy,.ax=a->ax,.ay=a->ay,.slope=a->ground_slope,.ground_tag=a->ground_tag,.movement_flags=a->movement_flags,.collision_flags=s->lifecycle_flags,.platform_ids=a->platform_ids,.previous_direction=a->previous_direction};int r=terrain_move_ground_origin(w->interactions->terrain,&m,policy,&s->ground_y_up);a->vx=m.vx;a->vy=m.vy;a->ax=m.ax;a->ay=m.ay;a->ground_slope=m.slope;a->ground_tag=m.ground_tag;a->platform_ids=m.platform_ids;if(r<0)enemy_fault(w);return r;}
static int front_blocked(struct WeaponWorld*w,struct EnemyActor*q){struct NativeEntity*n=contact(w,q,q->rocket_vehicle.direction&1?0:1);return n&&!!(n->payload.contact.hit.events&32);}
static int ground(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;if(q->actor.hit.flags&64)return 0;struct SlugSharedState s;if(!chassis_read(w,q,&s)||ground_move(w,&s,WMOVE_SYNC)<0)return 0;
 int found=slug_ground_contact(&s,w->interactions->terrain,0);if(found<0)return enemy_fault(w);
 if(!found){s.actor.ay=-64;if(ground_move(w,&s,WMOVE_BODY)<0)return 0;chassis_write(w,q,&s);return 0;}
 int block=slug_chassis_wall(&s);if(block)w->interactions->terrain->stop_x=(int16_t)block;if(ground_move(w,&s,WMOVE_GROUND)<0)return 0;
 int16_t x=tk_word((s.contacts[0].transform.x>>8)-(s.contacts[1].transform.x>>8)),y0=tk_word(transform_y_up(&s.contacts[0].transform)+w->camera_y),y1=tk_word(transform_y_up(&s.contacts[1].transform)+w->camera_y),dy=tk_word(y0-y1);
 int16_t mean=tk_word(((uint16_t)tk_word(y0+y1)>>1)-w->camera_y);transform_integer_offset(&s.actor.transform,0,tk_word(mean-transform_y_up(&s.actor.transform)));
 uint8_t angle=enemy_angle(x,dy);if(!(((unsigned)angle+8)>>4&1))angle=(angle+4)&248;q->rocket_vehicle.angle=angle;
 angle=(uint8_t)(angle+32);if((int8_t)angle<0)angle=0;if(angle>=64)angle=64;if(!(s.actor.transform.facing&1))angle=64-angle;q->rocket_vehicle.slope=angle>>2;
 chassis_write(w,q,&s);return front_blocked(w,q);
}
static unsigned phase(const struct EnemyActor*q){return q->rocket_vehicle.slope&7?3:((uint16_t)q->rocket_vehicle.slope&31)>>3;}
static int slanted(const struct EnemyActor*q){return !!(q->rocket_vehicle.slope&7);}
static int mismatch(const struct EnemyActor*q){return !!((q->rocket_vehicle.direction^q->actor.transform.facing)&1);}
static int wall(struct WeaponActor*a){return a->vx<0?!!(a->movement_flags&2):a->vx>0?!!(a->movement_flags&1):0;}
static int outside(struct WeaponWorld*w,struct WeaponActor*a){return interaction_outside(w,a,rocket_vehicle_bounds[0],0)||tk_sx(w,a)<-36;}
static void body_bind(struct EnemyActor*q,unsigned n){q->rocket_vehicle.contact=(uint8_t)n;defense(q,rocket_vehicle_defense[n]);attack(q,rocket_vehicle_attack[q->params[6]!=0][n]);}
static int select_body(struct NativeEntity*e,unsigned table){struct EnemyActor*q=&e->payload.enemy;if(slanted(q)){schedule(e,SLOPE_DRIVE);return 2;}unsigned n=phase(q);q->rocket_vehicle.pose=(uint8_t)n;start_entry(q,rocket_vehicle_tables[table][n]);body_bind(q,n);return 1;}
static int gate(struct WeaponWorld*w,struct EnemyActor*q){q->rocket_vehicle.wait=tk_word(q->rocket_vehicle.wait-1);if(q->rocket_vehicle.wait>0)return 0;int i=enemy_nearest_x(w,&q->actor,0);struct PlayerBodyState*p=w->players[i<0?1:i];int16_t px=p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1,py=p?tk_word(transform_y_up(&p->actor.transform)+w->camera_y):-1,dy=tk_word(py-tk_sy(w,&q->actor));if(dy<=0)dy=tk_word(-dy);if(dy>128)dy=128;unsigned row=(uint16_t)dy>>5;if(row>=5)return enemy_fault(w);int16_t dx=tk_word(px-tk_sx(w,&q->actor));unsigned range=rocket_vehicle_ranges[q->params[0]&3][row];return dx>=-(int)range&&dx<=(int)range;}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(width==1){if(f==RVF_8E)return q->rocket_vehicle.offset_x;if(f==RVF_8F)return q->rocket_vehicle.offset_y;}if(width==2){if(f==RVF_8A)return (uint16_t)q->rocket_vehicle.slope;if(f==RVF_34)return q->selector;}enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(width!=1||op!=AN_FIELD_SET)return enemy_fault(c->w);if(f==RVF_8E)q->rocket_vehicle.offset_x=(uint8_t)v;else if(f==RVF_8F)q->rocket_vehicle.offset_y=(uint8_t)v;else return enemy_fault(c->w);return 1;}
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(rocket_vehicle_bindings)/sizeof(*rocket_vehicle_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)defense(q,n);else if(kind==AN_ATTACK){attack(q,n);hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned kind){struct NativeEntity*n=boss_fx_pattern(w,entity_ref(e),&e->payload.enemy.actor.transform,(enum BossPattern)kind);if(!n)return 0;n->payload.enemy.actor.transform.priority=0x4000;return 1;}
static int callback(void*ctx,unsigned cb,struct Animation*a){(void)a;struct Context*c=ctx;struct WeaponWorld*w=c->w;struct NativeEntity*e=c->e,*n;if(cb==RVC_283CA){hit_begin_attack(&e->payload.enemy.actor.hit);return 1;}
 if(cb==RVC_6D59C){if(!tk_sound(w,0x1033)||!pattern(w,e,BP_ROCKET_VEHICLE_FIRE))return 0;return pattern(w,e,BP_ROCKET_VEHICLE_DEBRIS);}
 if(cb==RVC_6D5CC||cb==RVC_6D602){if(!tk_sound(w,0x1021)||!pattern(w,e,BP_ROCKET_VEHICLE_DEBRIS))return 0;n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,BFX_SMOKE);if(!n)return 0;n->payload.enemy.actor.transform.priority=0x4000;return 1;}return enemy_fault(w);}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=write,.bind=bind,.sound=sound,.call=callback};int done=animation_step(&a->animation,&rocket_vehicle_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
static int body_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;struct NativeEntity*n;int r;
 switch(s){
 case NEW:hit_begin_attack(&a->hit);if(!tk_settle(w,a))return 0;a->transform.facing=q->rocket_vehicle.direction=q->params[1]&1;a->hit.flags|=48;q->rocket_vehicle.variant=q->params[2];palette(q);q->rocket_vehicle.speed=tk_rank(w,q,TKR_BZ_SPEED);q->rocket_vehicle.wait=tk_rank(w,q,TKR_CREW_WAIT);a->hit.hp=tk_rank(w,q,TKR_CREW_HP);interaction_priority(w,a,0x8000,20);q->rocket_vehicle.contact=1;defense(q,rocket_vehicle_defense[1]);q->rocket_vehicle.angle=0;q->rocket_vehicle.slope=8;q->rocket_vehicle.pose=255;q->rocket_vehicle.ground=transform_y_up(&a->transform);if(!chassis_create(w,e))return 0;
  n=child(w,e,WEAPON_NEW);if(!n)return 0;n->payload.enemy.params[7]=q->params[7];n->payload.enemy.params[0]=q->params[0];n->payload.enemy.rocket_vehicle.variant=q->params[2];a->hit.type_flags|=4;schedule(e,IDLE);return 2;
 case DRIVE:q->rocket_vehicle.command=0;a->vx=q->rocket_vehicle.direction&1?q->rocket_vehicle.speed:tk_word(-q->rocket_vehicle.speed);return select_body(e,mismatch(q)?RVT_DRIVE_BACK:RVT_DRIVE);
 case STOP:q->rocket_vehicle.command=0;r=select_body(e,mismatch(q)?RVT_STOP_BACK:RVT_STOP);if(r!=1)return r;a->vx=0;break;
 case TURN:q->rocket_vehicle.command=0;return select_body(e,mismatch(q)?RVT_TURN_BACK:RVT_TURN);
 case IDLE:q->rocket_vehicle.command=0;q->rocket_vehicle.count=(uint8_t)tk_rank(w,q,TKR_BZ_IDLE);if(slanted(q)){schedule(e,SLOPE_DRIVE);return 2;}a->vx=0;return select_body(e,RVT_IDLE);
 case ALERT:m4_reset(a);{int i=enemy_nearest_x(w,a,0);q->rocket_vehicle.target=(uint8_t)(i<0?1:i);}q->rocket_vehicle.command=2;q->rocket_vehicle.count=q->rocket_vehicle.ack=0;return select_body(e,RVT_ALERT);
 case COUNT:q->rocket_vehicle.count=(uint8_t)tk_rank(w,q,TKR_CREW_COUNT);schedule(e,FIRE);return 2;
 case NEXT:q->rocket_vehicle.count--;schedule(e,(int8_t)q->rocket_vehicle.count<=0?RECOVER:FIRE);return 2;
 case FIRE:q->rocket_vehicle.ack=0;q->rocket_vehicle.command=3;q->rocket_vehicle.wait=tk_rank(w,q,TKR_CREW_REPEAT);return select_body(e,RVT_FIRE);
 case RECOVER:q->rocket_vehicle.wait=tk_word(tk_rank(w,q,TKR_CREW_WAIT)+30);q->rocket_vehicle.count=q->rocket_vehicle.ack=0;if(slanted(q)){schedule(e,SLOPE_DRIVE);return 2;}q->rocket_vehicle.command=4;return select_body(e,RVT_RECOVER);
 case HURT:a->hit.flags&=247;a->vx=0;if(slanted(q)){schedule(e,SLOPE_DRIVE);return 2;}q->rocket_vehicle.command=6;return select_body(e,RVT_HURT);
 case SLOPE_DRIVE:case SLOPE_IDLE:if(s==SLOPE_IDLE){m4_reset(a);q->rocket_vehicle.count=(uint8_t)tk_rank(w,q,TKR_BZ_IDLE)>>1;}else a->vx=q->rocket_vehicle.direction&1?q->rocket_vehicle.speed:tk_word(-q->rocket_vehicle.speed);q->rocket_vehicle.command=1;body_bind(q,3);start(q,RVA_SLOPE);break;
 case DEAD:a->display_flags&=253;a->vx=0;q->rocket_vehicle.command=7;start_entry(q,rocket_vehicle_tables[RVT_DEAD][phase(q)]);break;
 case FADE:for(unsigned i=0;i<3;i++){if(q->rocket_vehicle.contacts[i][0]==65535)continue;n=contact(w,q,i);if(!n)return 0;n->tick=contact_delete;}q->rocket_vehicle.wait=60;defense(q,65535);a->display_flags&=253;q->rocket_vehicle.command=7;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void hp_tail(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->hit.flags&1)schedule(e,DEAD);if(outside(w,a))schedule(e,FADE);}
static int body_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int done,blocked,r;
 if(s==FADE){q->rocket_vehicle.wait=tk_word(q->rocket_vehicle.wait-1);if(q->rocket_vehicle.wait<=0)schedule(e,DELETE_TREE);return 1;}
 if(s==DEAD){ground(w,e);if(w->fault)return 0;done=animate(w,e);if(done||outside(w,a))schedule(e,FADE);return 1;}
 if(q->rocket_vehicle.contact>=4)return enemy_fault(w);interaction_body_contact(w,a,&rocket_vehicle_contacts[q->rocket_vehicle.contact]);blocked=ground(w,e);if(w->fault)return 0;
 if((s==DRIVE||s==TURN)&&blocked){q->rocket_vehicle.direction^=1;schedule(e,STOP);}
 if(s==DRIVE||s==IDLE){if(slanted(q)){schedule(e,SLOPE_DRIVE);return 2;}if(phase(q)!=q->rocket_vehicle.pose){r=select_body(e,s==IDLE?RVT_IDLE:mismatch(q)?RVT_DRIVE_BACK:RVT_DRIVE);if(r!=1)return r;}}
 else if(s==STOP||s==TURN||s==HURT){if(slanted(q)||phase(q)!=q->rocket_vehicle.pose)schedule(e,IDLE);}
 else if(s==SLOPE_DRIVE||s==SLOPE_IDLE){if(!slanted(q))schedule(e,s==SLOPE_DRIVE?DRIVE:IDLE);}
 done=animate(w,e);if(w->fault)return 0;
 switch(s){
 case DRIVE:
  if(wall(a)){q->rocket_vehicle.direction^=1;schedule(e,STOP);}else{
   if(tk_sx(w,a)<=48&&!(q->rocket_vehicle.direction&1)){q->rocket_vehicle.direction|=1;a->vx=0;schedule(e,STOP);}if(tk_sx(w,a)>=272&&(q->rocket_vehicle.direction&1)){q->rocket_vehicle.direction&=254;a->vx=0;schedule(e,STOP);}}
  if(gate(w,q)){a->vx=0;schedule(e,STOP);}hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;break;
 case STOP:if(done)schedule(e,IDLE);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;break;
 case TURN:if(done)schedule(e,DRIVE);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;break;
 case IDLE:q->rocket_vehicle.count--;if((int8_t)q->rocket_vehicle.count<=0)schedule(e,TURN);if(gate(w,q))schedule(e,ALERT);break;
 case ALERT:if(q->rocket_vehicle.ack)schedule(e,COUNT);break;
 case FIRE:if(q->rocket_vehicle.ack){q->rocket_vehicle.ack=0;q->rocket_vehicle.command=5;}q->rocket_vehicle.wait=tk_word(q->rocket_vehicle.wait-1);if(q->rocket_vehicle.wait<=0)schedule(e,NEXT);break;
 case RECOVER:if(q->rocket_vehicle.ack)schedule(e,TURN);break;
 case HURT:if(done){a->hit.flags&=247;schedule(e,TURN);}if(a->hit.flags&8){if(!tk_hit_sound(w,a))return 0;a->hit.flags&=247;}hp_tail(w,e);return !w->fault;
 case SLOPE_DRIVE:
  a->hit.flags&=247;q->rocket_vehicle.wait=tk_word(q->rocket_vehicle.wait-1);
  if(front_blocked(w,q)||wall(a)){q->rocket_vehicle.direction^=1;schedule(e,SLOPE_IDLE);}
  else if(tk_sx(w,a)<=16&&!(q->rocket_vehicle.direction&1)){q->rocket_vehicle.direction|=1;schedule(e,SLOPE_IDLE);}
  else if(tk_sx(w,a)>=304&&(q->rocket_vehicle.direction&1)){q->rocket_vehicle.direction&=254;schedule(e,SLOPE_IDLE);}
  hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;hp_tail(w,e);return !w->fault;
 case SLOPE_IDLE:a->hit.flags&=247;q->rocket_vehicle.wait=tk_word(q->rocket_vehicle.wait-1);q->rocket_vehicle.count--;if((int8_t)q->rocket_vehicle.count<=0)schedule(e,SLOPE_DRIVE);hp_tail(w,e);return !w->fault;
 default:return enemy_fault(w);
 }
 q->rocket_vehicle.hurt=0;if(a->hit.flags&8){if(!tk_hit_sound(w,a))return 0;a->hit.flags&=247;q->rocket_vehicle.hurt=1;}hp_tail(w,e);return !w->fault;
}
static int weapon_follow(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;a->transform=p->actor.transform;q->rocket_vehicle.slope=p->rocket_vehicle.slope;transform_integer_offset(&a->transform,tk_faced(a,(int8_t)p->rocket_vehicle.offset_x),(int8_t)p->rocket_vehicle.offset_y);unsigned angle=q->selector&15;if(!angle||angle>=8)a->transform.priority--;if(p->actor.hit.events&128)a->hit.events|=1;return 1;}
static int weapon_select(struct WeaponWorld*w,struct NativeEntity*e,unsigned table){struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;q->rocket_vehicle.slope=p->rocket_vehicle.slope;if(slanted(q)){schedule(e,WEAPON_SLOPE);return 2;}q->rocket_vehicle.pose=(uint8_t)phase(q);start_entry(q,rocket_vehicle_tables[table][q->rocket_vehicle.pose]);return 1;}
static int weapon_lookup(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;unsigned cmd=p->rocket_vehicle.command;if(cmd==q->rocket_vehicle.command)return 1;if(q->rocket_vehicle.command==7){schedule(e,WEAPON_FLY);return 2;}q->rocket_vehicle.command=(uint8_t)cmd;if(cmd>=9)return enemy_fault(w);static const uint8_t states[]={WEAPON_IDLE,WEAPON_SLOPE,WEAPON_AIM,WEAPON_FIRE,WEAPON_CENTER,WEAPON_HOLD,WEAPON_DEAD,WEAPON_FLY,DELETE};schedule(e,states[cmd]);return 2;}
static int fire(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned angle=q->selector;if(angle>=16||q->rocket_vehicle.pose>=4)return enemy_fault(w);if(!tk_sound(w,0x1064))return 0;unsigned shot=a->transform.facing&1?rocket_vehicle_mirror[angle]&15:angle;
 const int16_t*off=rocket_vehicle_offsets[q->rocket_vehicle.pose][angle];int16_t dx=tk_faced(a,off[0]),dy=off[1];struct NativeEntity*n=child(w,e,MUZZLE);if(!n)return 0;n->payload.enemy.selector=(uint16_t)angle;n->payload.enemy.rocket_vehicle.offset_x=(uint8_t)dx;n->payload.enemy.rocket_vehicle.offset_y=(uint8_t)dy;n->payload.enemy.rocket_vehicle.pose=q->rocket_vehicle.pose;
 n=child(w,e,ROCKET);if(!n)return 0;n->payload.enemy.selector=(uint16_t)shot;n->payload.enemy.params[0]=q->params[0];n->payload.enemy.rocket_vehicle.pose=q->rocket_vehicle.pose;transform_integer_offset(&n->payload.enemy.actor.transform,dx,dy);return 1;}
static void target_xy(struct WeaponWorld*w,struct EnemyActor*q,int16_t*x,int16_t*y){struct PlayerBodyState*p=w->players[q->rocket_vehicle.target&1];*x=p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1;*y=p?tk_word(transform_y_up(&p->actor.transform)+w->camera_y):-1;}
static int aim(struct WeaponWorld*w,struct EnemyActor*q){int16_t px,py;target_xy(w,q,&px,&py);struct WeaponActor*a=&q->actor;int16_t dx=tk_word(px-tk_sx(w,a)),dy=tk_word(py-tk_sy(w,a));
 if(q->params[7]){int16_t x=dx<0?tk_word(-dx):dx,y=dy<0?tk_word(-dy):dy,hi=x>y?x:y,lo=x>y?y:x,distance=tk_word(hi+((uint16_t)lo>>1));q->params[0]=distance<112?0:distance<176?1:2;}
 int x=dx;if(x< -256)x=-256;if(x>256)x=256;unsigned row=(q->rocket_vehicle.pose&3)+(py>tk_sy(w,a)?0:4),angle=rocket_vehicle_angles[a->transform.facing&1][row][(x+256)>>4];if(angle>=16)return enemy_fault(w);if(a->transform.facing&1)angle=rocket_vehicle_mirror[angle];if(q->rocket_vehicle.pose==1&&dy>=-8&&dy<=40)angle=tk_faced(a,dx)>0?0:8;angle&=15;if(angle==q->selector)return 1;q->selector=(uint16_t)(q->selector+(angle>q->selector?1:-1));return 0;}
static int parts_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;unsigned s=a->state;int r;
 switch(s){
 case WEAPON_NEW:palette(q);q->params[0]&=3;q->rocket_vehicle.command=0;a->state=WEAPON_IDLE;/* FALLTHROUGH */
 case WEAPON_IDLE:q->selector=8;return weapon_select(w,e,RVT_RIDER_IDLE);
 case WEAPON_SLOPE:q->selector=8;start(q,RVA_RIDER_SLOPE);break;
 case WEAPON_AIM:q->rocket_vehicle.clock=0;p=tk_parent(w,e);if(!p)return 0;q->rocket_vehicle.target=p->rocket_vehicle.target;return weapon_select(w,e,RVT_RIDER_IDLE);
 case WEAPON_FIRE:p=tk_parent(w,e);if(!p)return 0;q->rocket_vehicle.slope=p->rocket_vehicle.slope;if(slanted(q)){schedule(e,WEAPON_SLOPE);return 2;}q->rocket_vehicle.pose=(uint8_t)phase(q);start_entry(q,rocket_vehicle_fire[q->rocket_vehicle.pose][q->selector&15]);if(!fire(w,e))return 0;break;
 case WEAPON_CENTER:case WEAPON_HOLD:q->rocket_vehicle.clock=0;return weapon_select(w,e,RVT_RIDER_IDLE);
 case WEAPON_DEAD:return weapon_select(w,e,RVT_RIDER_DEAD);
 case WEAPON_FLY:a->vx=tk_faced(a,-34);a->vy=1635;a->ay=-109;a->ax=0;interaction_priority(w,a,0x4000,0);break;
 case MUZZLE:interaction_priority(w,a,0x4000,28);a->palette=8;transform_integer_offset(&a->transform,(int8_t)q->rocket_vehicle.offset_x,(int8_t)q->rocket_vehicle.offset_y-8);start(q,RVA_MUZZLE);a->state=FALL;break;
 case ROCKET:{const int16_t*t=rocket_vehicle_motion[a->transform.facing&1][q->rocket_vehicle.pose&3][q->params[0]&7][q->selector&15];a->vx=t[0];a->ay=t[1];a->vy=t[2];a->transform.facing&=254;a->palette=0x173;a->hit.type_flags|=16;start(q,RVA_ROCKET);q->rocket_vehicle.body_collision=(q->selector&15)>0&&(q->selector&15)<8;interaction_priority(w,a,0xd000,q->rocket_vehicle.body_collision?8:28);break;}
 case ROCKET_HIT:if(!tk_sound(w,0x1025))return 0;/* FALLTHROUGH */
 case BLAST:interaction_priority(w,a,0x4000,0);defense(q,65535);boss_fx_transform(w,e,BFX_FIRE);return 0;
 default:r=enemy_fault(w);return r;
 }return !w->fault;
}
static int parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int done,r;
 if(s==WEAPON_FLY){r=interaction_move(w,a,WMOVE_BODY);if(r<0)return 0;if(r)schedule(e,BLAST);animate(w,e);if(tk_sy(w,a)<=192)schedule(e,DELETE);if(entity_parent_deleting(w->entities,e))schedule(e,BLAST);if(outside(w,a))schedule(e,DELETE_TREE);return 1;}
 if(s==FALL){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(animate(w,e))schedule(e,DELETE_TREE);return 1;}
 if(s==ROCKET){r=interaction_move(w,a,q->rocket_vehicle.body_collision?WMOVE_BODY:WMOVE_FALL);if(r<0)return 0;if(r)schedule(e,ROCKET_HIT);q->selector=enemy_angle(a->vx>>4,a->vy>>4)>>3;animate(w,e);if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)schedule(e,BLAST);if(interaction_outside(w,a,rocket_vehicle_bounds[1],1))schedule(e,DELETE_TREE);return 1;}
 p=tk_parent(w,e);if(!p)return 0;
 if(s==WEAPON_AIM||s==WEAPON_CENTER){q->rocket_vehicle.clock=(q->rocket_vehicle.clock+1)&3;if(!q->rocket_vehicle.clock){if(s==WEAPON_AIM){p->rocket_vehicle.ack=0;if(aim(w,q))p->rocket_vehicle.ack=1;}else if(q->selector==8)p->rocket_vehicle.ack=1;else q->selector=(uint16_t)(q->selector+(q->selector<8?1:-1));}}
 r=weapon_lookup(w,e);if(r!=1)return r;if(!weapon_follow(w,e))return 0;
 if(s==WEAPON_SLOPE){if(!slanted(q)){schedule(e,WEAPON_IDLE);return 2;}animate(w,e);return 1;}
 if(s==WEAPON_FIRE){if(animate(w,e))p->rocket_vehicle.ack=1;return 1;}
 if(slanted(q)){schedule(e,WEAPON_SLOPE);return 2;}
 if(phase(q)!=q->rocket_vehicle.pose){r=weapon_select(w,e,s==WEAPON_DEAD?RVT_RIDER_DEAD:RVT_RIDER_IDLE);if(r!=1)return r;}
 if(s==WEAPON_DEAD){a->transform.priority--;animate(w,e);return 1;}
 done=animate(w,e);if(done){r=weapon_select(w,e,RVT_RIDER_IDLE);if(r!=1)return r;}
 if(p->rocket_vehicle.hurt){r=weapon_select(w,e,RVT_RIDER_HURT);if(r!=1)return r;}return 1;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;for(unsigned i=0;i<16;i++){
 if(q->actor.state==DELETE||q->actor.state==DELETE_TREE){if(q->actor.state==DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
 if(q->entry){q->entry=0;int r=q->actor.state<WEAPON_NEW?body_enter(w,e):parts_enter(w,e);if(!r||w->fault)return;if(r==2)continue;}unsigned s=q->actor.state;int r=s<WEAPON_NEW?body_frame(w,e,s):parts_frame(w,e,s);if(r==2&&!w->fault)continue;return;
 }enemy_fault(w);}
int rocket_vehicle_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id!=88&&id!=89)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,NEW);if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];e->payload.enemy.params[6]=id==88;return 1;}
/* $06A98E writes a next-tick controller and lets the old weapon see command7
 * this tick. Initialize the new union only when that scheduled tick begins. */
static void adopt_tick(struct EntityPool*p,struct NativeEntity*e,void*world){init(e,NEW);e->payload.enemy.params[6]=1;tick(p,e,world);}
void rocket_vehicle_adopt(struct NativeEntity*e){e->tick=adopt_tick;}
