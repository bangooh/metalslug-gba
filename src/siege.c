#include "scene_placements.h"
#include "siege_internal.h"
/* $0832EC..$084D42: native scene props and their deferred transitions. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void sg_schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
void sg_reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
void sg_screen(struct WeaponWorld*w,struct WeaponActor*a,int x,int y){transform_integer_offset(&a->transform,sg_word(x-sg_sx(w,a)),sg_word(y-sg_sy(w,a)));}
uint16_t sg_random(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
uint16_t sg_bounded(struct WeaponWorld*w,uint16_t n){if(!w->rng){enemy_fault(w);return 0;}return source_random(w->rng,n);}
uint16_t sg_masked(struct WeaponWorld*w,uint16_t n){if(!w->rng){enemy_fault(w);return 0;}return source_random_mask(w->rng,n);}
void sg_start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void sg_start(struct EnemyActor*q,enum SiegeAnimation n){sg_start_entry(q,siege_entries[n]);}
void sg_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:siege_bindings[n].defense;q->actor.defense_count=n==65535?0:siege_bindings[n].count;}
void sg_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){siege_bindings[n].attack,siege_bindings[n].count};q->actor.attack=&q->attack_set;}hit_begin_attack(&q->actor.hit);}
int sg_patch(struct WeaponWorld*w,unsigned n){if(n>=sizeof(siege_patches)/sizeof(*siege_patches)||!scene_video_patch(w->video,&siege_patches[n]))return enemy_fault(w);return 1;}
int sg_stamp(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){if(n>=SGS_COUNT||!w->interactions)return enemy_fault(w);const struct SiegeStamp*t=&siege_stamps[n];return terrain_stamp(w->interactions->terrain,&e->payload.enemy.actor.transform,t->dx,t->dy,t->width,t->height,t->cells)?1:enemy_fault(w);}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct WeaponActor*a=&c->e->payload.enemy.actor;if(width==1){if(f==SGF_20)return a->phase;if(f==SGF_3A)return a->transform.facing;}enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct WeaponActor*a=&c->e->payload.enemy.actor;if(op==AN_FIELD_XOR)v^=read(ctx,f,width);else if(op==AN_FIELD_AND)v&=read(ctx,f,width);else if(op==AN_FIELD_OR)v|=read(ctx,f,width);else if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(width!=1)return enemy_fault(c->w);if(f==SGF_20)a->phase=(uint8_t)v;else if(f==SGF_3A)a->transform.facing=(uint8_t)v;else return enemy_fault(c->w);return 1;}
static int bind(void*ctx,unsigned kind,uint32_t value){struct Context*c=ctx;unsigned n=value==0xffffffffu?65535:value;if(n!=65535&&n>=sizeof(siege_bindings)/sizeof(*siege_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)sg_defense(&c->e->payload.enemy,n);else if(kind==AN_ATTACK)sg_attack(&c->e->payload.enemy,n);else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;if(n>=sizeof(siege_callback_patch)/sizeof(*siege_callback_patch))return enemy_fault(c->w);return sg_patch(c->w,siege_callback_patch[n]);}
int sg_animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner owner={.context=&c,.read=read,.write=write,.bind=bind,.sound=sound,.call=call};int r=animation_step(&a->animation,&siege_program,&owner);if(a->animation.fault)enemy_fault(w);return r;}
struct EnemyActor*sg_parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;}
static void init(struct NativeEntity*e,unsigned state){uint8_t*p=(uint8_t*)&e->payload.enemy.siege;for(unsigned i=0;i<sizeof(e->payload.enemy.siege);i++)p[i]=255;sg_schedule(e,state);}
struct NativeEntity*sg_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned state,enum EntityInsert order){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),order,&e->payload.enemy.actor.transform,tick);if(n)init(n,state);return n;}
struct NativeEntity*sg_pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){if(n>=SGP_COUNT){enemy_fault(w);return 0;}struct NativeEntity*p=sg_child(w,e,SG_PATTERN,ENTITY_LAST_CHILD);if(p)p->payload.enemy.siege.pattern=(uint16_t)n;return p;}
int sg_score(struct WeaponWorld*w,struct WeaponActor*a,uint32_t n){return w->items&&item_score_add(w->items,a->score_owner,n)?1:enemy_fault(w);}
int sg_hurt(struct WeaponWorld*w,struct WeaponActor*a,int force){if(!(a->hit.flags&1)&&a->hit.last_kind==1&&(force||(a->hit.events&1)))return interaction_sound(w,0x108d);return 1;}
int sg_cull(struct WeaponWorld*w,struct NativeEntity*e,int tree){if(sg_sx(w,&e->payload.enemy.actor)>=sg_word(-e->payload.enemy.siege.margin))return 0;if(tree)entity_delete_children(w->entities,e);entity_delete(w->entities,e);return 1;}
static void setup(struct EnemyActor*q,int palette,int margin,int hp,enum SiegeAnimation anim){q->siege.margin=(int16_t)margin;q->scale_x=q->scale_y=255;q->actor.transform.facing=0;if(palette>=0)q->actor.palette=(uint16_t)palette;if(hp>=0)q->actor.hit.hp=(int16_t)hp;sg_start(q,anim);}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;q->entry=0;
 switch(a->state){
 case SG_DRIFT:case SG_FLICKER:case SG_SINE:
  setup(q,0xa4,-1,-1,SGA_DRIFT);a->transform.priority=0;sg_reset(a);sg_defense(q,65535);
  if(a->state==SG_FLICKER)q->siege.timer=0;else a->vx=sg_sx(w,a)<160?128:-128;
  if(sg_sx(w,a)>=160)a->transform.facing=1;break;
 case SG_GATE:setup(q,0xa7,48,30,SGA_GATE);break;
 case SG_GATE_BROKEN:if(!sg_score(w,a,0x100))return 0;sg_defense(q,65535);sg_start(q,SGA_GATE_BROKEN);break;
 case SG_ARCH:setup(q,0xab,48,-1,SGA_ARCH);break;
 case SG_COLUMN:
  setup(q,0xaa,48,60,q->dead?SGA_COLUMN_B:SGA_COLUMN_A);a->transform.facing=(uint8_t)q->siege.config;a->phase=(uint8_t)(q->siege.config>>8);a->transform.priority=0x8000;sg_reset(a);
  if(interaction_move(w,a,WMOVE_POINT)<0)return 0;if(a->transform.facing&1)transform_integer_offset(&a->transform,32,0);if(q->dead==2)sg_attack(q,SGB_COLUMN);break;
 case SG_COLUMN_HIT1:sg_start(q,q->dead?SGA_COLUMN_HIT1_B:SGA_COLUMN_HIT1_A);break;
 case SG_COLUMN_HIT2:sg_start(q,q->dead?SGA_COLUMN_HIT2_B:SGA_COLUMN_HIT2_A);break;
 case SG_COLUMN_WRECK:
  if(interaction_move(w,a,WMOVE_SYNC)<0||!sg_score(w,a,0x2000)||!sg_spawn_pow(w,e))return 0;sg_attack(q,65535);sg_defense(q,65535);
  if(sg_sy(w,a)<=368&&!sg_pattern(w,e,q->dead?SGP_COLUMN_B:SGP_COLUMN_A))return 0;sg_start(q,q->dead?SGA_COLUMN_WRECK_B:SGA_COLUMN_WRECK_A);
  if(a->phase!=1){if(a->transform.facing&1)transform_integer_offset(&a->transform,-24,0);int ok=sg_stamp(w,e,q->dead?SGS_COLUMN_B:SGS_COLUMN_A);if(a->transform.facing&1)transform_integer_offset(&a->transform,24,0);if(!ok)return 0;}
  sg_reset(a);a->ay=-3;a->vx=a->transform.facing&1?-64:64;q->siege.timer=90;a->damage_timer=240;break;
 case SG_COLUMN_BLAST:if(!sg_pattern(w,e,SGP_COLUMN_BLAST))return 0;q->siege.timer=30;sg_reset(a);break;
 case SG_REACTIVE:setup(q,0xa5,16,-1,SGA_REACTIVE);/* fall through */
 case SG_REACTIVE_CLEAR:a->hit.flags&=247;a->state=SG_REACTIVE_CLEAR;break;
 case SG_REACTIVE_ACTIVE:if(!interaction_sound(w,0x10a9))return 0;a->animation.freeze=0;q->siege.timer=sg_bounded(w,10)+35;break;
 case SG_SOLID_A:case SG_SOLID_B:setup(q,a->state==SG_SOLID_A?0xa5:0xa7,0,-1,a->state==SG_SOLID_A?SGA_SOLID_A:SGA_SOLID_B);a->transform.priority=0x8000;sg_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;break;
 case SG_GUARD_ROOT:
  setup(q,0xa7,256,-1,SGA_GUARD_ROOT);n=sg_child(w,e,SG_GUARD_FOLLOW,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.actor.transform.x=n->payload.enemy.actor.transform.y=0;n->payload.enemy.actor.transform.facing=0;if(!sg_spawn_guards(w,e))return 0;break;
 case SG_GUARD_FOLLOW:setup(q,0xa7,-1,-1,SGA_GUARD_FOLLOW);break;
 case SG_GUARD:a->palette=q->siege.variant?0x166:0xaa;q->scale_x=q->scale_y=255;q->siege.margin=32;sg_start(q,q->dead&1?SGA_GUARD_B:SGA_GUARD_A);break;
 case SG_BOX:setup(q,0xa6,48,20,SGA_BOX);break;
 case SG_BOX_WRECK:sg_defense(q,65535);if(!sg_pattern(w,e,SGP_BOX))return 0;sg_start(q,SGA_BOX_WRECK);break;
 case SG_TILE_A:case SG_TILE_B:q->dead=a->state==SG_TILE_B;sg_start(q,q->dead?SGA_TILE_B:SGA_TILE_A);q->siege.margin=48;a->transform.facing=0;a->hit.hp=20;break;
 case SG_BOUNCE:setup(q,0x89,80,100,SGA_BOUNCE);a->transform.priority=0x8000;sg_reset(a);if(terrain_weapon_ground_test(w->interactions->terrain,a)<0)return enemy_fault(w);a->state=SG_BOUNCE_ACTIVE;break;
 case SG_BOUNCE_ACTIVE:break;
 case SG_BOUNCE_HIT:sg_start(q,SGA_BOUNCE_HIT);break;
 case SG_BOUNCE_WRECK:if(!sg_score(w,a,0x2000))return 0;sg_defense(q,65535);if(!sg_pattern(w,e,SGP_BOUNCE_FIRE)||!sg_pattern(w,e,SGP_BOUNCE_DEBRIS))return 0;sg_start(q,SGA_BOUNCE_WRECK);sg_reset(a);a->vy=640;a->ay=-64;break;
 case SG_ROW8_ROOT:case SG_ROW9_ROOT:{
  int row8=a->state==SG_ROW8_ROOT;if(row8){q->siege.margin=240;q->dead=0;}
  for(unsigned i=0;i<(row8?8u:9u);i++){n=sg_child(w,e,row8?SG_ROW8_WAIT:SG_ROW9,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.dead=(uint8_t)i;transform_integer_offset(&n->payload.enemy.actor.transform,(int)i*(row8?32:16),0);}entity_delete(w->entities,e);return 0;}
 case SG_ROW8_WAIT:break;
 case SG_ROW8:q->siege.margin=32;a->hit.hp=10;sg_start(q,SGA_ROW8);break;
 case SG_ROW8_BROKEN:{
  if(q->dead>=8)return enemy_fault(w);if(!sg_score(w,a,0x500))return 0;sg_defense(q,65535);struct EnemyActor*p=sg_parent(w,e);if(!p)return 0;p->dead|=(uint8_t)(1u<<q->dead);
  if(!sg_pattern(w,e,SGP_ROW)||!sg_patch(w,siege_patch_row8[q->dead][0])||!sg_patch(w,siege_patch_row8[q->dead][2]))return 0;a->phase=0;break;}
 case SG_SINGLE:setup(q,0xa4,96,1,SGA_SINGLE);break;
 case SG_SINGLE_HIT:sg_defense(q,65535);sg_start(q,SGA_SINGLE_HIT);break;
 case SG_SINGLE_TILE:q->siege.margin=64;a->hit.hp=1;sg_start(q,SGA_SINGLE_TILE);break;
 case SG_SINGLE_TILE_BREAK:sg_defense(q,65535);if(!sg_patch(w,SGV_SINGLE))return 0;entity_delete(w->entities,e);return 0;
 case SG_ROW9:q->siege.margin=0;a->hit.hp=1;sg_start(q,SGA_ROW9);break;
 case SG_BREAKABLE:q->siege.margin=48;a->hit.hp=1;sg_start(q,SGA_BREAKABLE);break;
 case SG_REWARD:q->siege.margin=32;a->hit.hp=10;q->dead=(uint8_t)a->transform.priority;sg_defense(q,SGB_REWARD);break;
 case SG_REWARD_DECOR:setup(q,0xa8,64,-1,SGA_REWARD_DECOR);a->transform.priority=0x8000;sg_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int done=0,r;
 if(s<=SG_SINE){
  if(s==SG_SINE){q->siege.angle=(q->siege.angle+8)&255;a->vy=sg_word((128*(int32_t)boss_trig[0][q->siege.angle])>>8);}
  if(interaction_move(w,a,s==SG_FLICKER?WMOVE_SYNC:WMOVE_POINT)<0)return 0;sg_animate(w,e);
  if(s==SG_FLICKER){q->siege.timer=sg_word(q->siege.timer+1);if((q->siege.timer&2)&&!(sg_random(w)&15)){int dx=sg_bounded(w,80);transform_integer_offset(&a->transform,a->transform.facing&1?-dx:dx,0);}}
  if(interaction_outside(w,a,boss_fx_bounds[8],0))entity_delete(w->entities,e);return !w->fault;
 }
 if(s==SG_GUARD_FOLLOW){struct EnemyActor*p=sg_parent(w,e);if(!p)return 0;transform_integer_copy(&a->transform,&p->actor.transform);if(sg_sx(w,a)<-128)entity_delete(w->entities,e);else sg_animate(w,e);return !w->fault;}
 if(s==SG_ROW8_WAIT){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(sg_sx(w,a)<=320){sg_schedule(e,SG_ROW8);return 2;}return 1;}
 if(s==SG_ROW8_BROKEN){
  struct EnemyActor*p=sg_parent(w,e);if(!p)return 0;if(q->dead>=8)return enemy_fault(w);
  if(!(a->phase&16)&&(q->dead==0||(p->dead&(1u<<(q->dead-1))))){if(q->dead&&!sg_patch(w,siege_patch_row8[q->dead][1]))return 0;a->phase|=16;}
  if(!(a->phase&1)&&(q->dead==7||(p->dead&(1u<<(q->dead+1))))){if(q->dead!=7&&!sg_patch(w,siege_patch_row8[q->dead][3]))return 0;a->phase|=1;}
  /* Source compares $21, not its $20 completion bits. */
  if(q->dead==0x11)entity_delete(w->entities,e);return !w->fault;
 }
 if(s==SG_GATE)w->enemies->intro_gate=0;
 if(s==SG_COLUMN_WRECK&&q->siege.timer==80&&!interaction_sound(w,0x1054))return 0;
 if(s==SG_REACTIVE_CLEAR)a->animation.freeze=1;
 r=interaction_move(w,a,s==SG_COLUMN_WRECK||s==SG_COLUMN_BLAST||s==SG_BOUNCE_WRECK?WMOVE_POINT:WMOVE_SYNC);if(r<0)return 0;
 if(s==SG_BOUNCE_WRECK&&r&&a->vy<=0)sg_reset(a);
 if(s!=SG_REWARD&&(s!=SG_GUARD_ROOT||sg_sx(w,a)<=288)&&(s!=SG_GUARD||sg_sx(w,a)<=320))done=sg_animate(w,e);
 switch(s){
 case SG_GATE:case SG_BOX:case SG_TILE_A:case SG_TILE_B:case SG_BOUNCE_ACTIVE:case SG_COLUMN:case SG_COLUMN_HIT1:case SG_COLUMN_HIT2:case SG_ROW8:case SG_BREAKABLE:case SG_REWARD:
  if(s>=SG_COLUMN&&s<=SG_COLUMN_HIT2&&q->dead==2)hit_begin_attack(&a->hit);
  if(a->hit.flags&8){if(!sg_hurt(w,a,s==SG_ROW8))return 0;a->hit.flags&=247;if(s==SG_BOUNCE_ACTIVE)sg_schedule(e,SG_BOUNCE_HIT);}
  if(s==SG_COLUMN&&a->hit.hp<=40)sg_schedule(e,SG_COLUMN_HIT1);
  if(s==SG_COLUMN_HIT1&&a->hit.hp<=20)sg_schedule(e,SG_COLUMN_HIT2);
  if(a->hit.flags&1){
   if(s==SG_GATE){if(!sg_stamp(w,e,SGS_GATE)||!interaction_sound(w,0x1037))return 0;sg_schedule(e,SG_GATE_BROKEN);}
   else if(s==SG_COLUMN_HIT2)sg_schedule(e,SG_COLUMN_WRECK);
   else if(s==SG_BOX){if(!interaction_sound(w,0x102e))return 0;sg_schedule(e,SG_BOX_WRECK);}
   else if(s==SG_TILE_A||s==SG_TILE_B){if(!interaction_sound(w,0x1027)||!sg_patch(w,q->dead?SGV_TILE_B:SGV_TILE_A))return 0;if(q->dead)transform_integer_offset(&a->transform,0,-16);if(!sg_pattern(w,e,SGP_TILE))return 0;entity_delete(w->entities,e);return 1;}
   else if(s==SG_BOUNCE_ACTIVE){if(!interaction_sound(w,0x1023))return 0;sg_schedule(e,SG_BOUNCE_WRECK);}
   else if(s==SG_ROW8){if(!sg_stamp(w,e,SGS_ROW)||!interaction_sound(w,0x1030))return 0;sg_schedule(e,SG_ROW8_BROKEN);}
   else if(s==SG_BREAKABLE){
    if(!sg_score(w,a,0x1000)||!interaction_sound(w,0x1030))return 0;sg_defense(q,65535);
    if(!sg_patch(w,SGV_BREAK_A)||!sg_patch(w,SGV_BREAK_B)||!sg_pattern(w,e,SGP_BREAK)||!sg_pattern(w,e,SGP_ROW)||!sg_stamp(w,e,SGS_ROW)||!sg_stamp(w,e,SGS_BREAK))return 0;entity_delete(w->entities,e);return 1;
   }else if(s==SG_REWARD){if(q->dead>=8)return enemy_fault(w);if(!sg_score(w,a,0x2000)||!interaction_sound(w,0x1027))return 0;sg_defense(q,65535);if(!sg_patch(w,siege_patch_reward[q->dead][0])||!sg_pattern(w,e,SGP_REWARD))return 0;entity_delete(w->entities,e);return 1;}
  }break;
 case SG_COLUMN_WRECK:if(!--q->siege.timer)sg_schedule(e,SG_COLUMN_BLAST);break;
 case SG_COLUMN_BLAST:if(!--q->siege.timer){entity_delete(w->entities,e);return 1;}break;
 case SG_GATE_BROKEN:if(done){entity_delete(w->entities,e);return 1;}break;
 case SG_REACTIVE_CLEAR:if(a->hit.flags&8)sg_schedule(e,SG_REACTIVE_ACTIVE);break;
 case SG_REACTIVE_ACTIVE:if(!--q->siege.timer)sg_schedule(e,SG_REACTIVE_CLEAR);break;
 case SG_GUARD:if(a->hit.flags&8){if(!interaction_sound(w,0x10a6)||!sg_pattern(w,e,SGP_GUARD))return 0;entity_delete(w->entities,e);return 1;}break;
 case SG_BOUNCE_HIT:if(done)sg_schedule(e,SG_BOUNCE_ACTIVE);break;
 case SG_SINGLE:if(a->hit.flags&8)sg_schedule(e,SG_SINGLE_HIT);break;
 case SG_SINGLE_TILE:if(a->hit.flags&8)sg_schedule(e,SG_SINGLE_TILE_BREAK);break;
 case SG_ROW9:if(a->hit.flags&8){if(q->dead>=9)return enemy_fault(w);if(!sg_patch(w,siege_patch_row9[q->dead][0]))return 0;entity_delete(w->entities,e);return 1;}break;
 default:break;
 }sg_cull(w,e,s==SG_GUARD_ROOT);return !w->fault;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;
 for(unsigned budget=0;budget<8;budget++){
  unsigned s=q->actor.state;if(s==SG_DELETE||s==SG_DELETE_TREE){if(s==SG_DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
  if(q->entry){int r=s<SG_FINALE?enter(w,e):s<SG_PATTERN?sg_finale_enter(w,e):sg_effects_enter(w,e);if(!r||w->fault)return;if(r==2)continue;}
  if(!e->active||e->deleting)return;s=q->actor.state;int r=s<SG_FINALE?frame(w,e,s):s<SG_PATTERN?sg_finale_frame(w,e,s):sg_effects_frame(w,e,s);if(r!=2)return;
 }enemy_fault(w);
}
int siege_spawn_scene(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,enum SiegeEntry id,uint16_t config,uint8_t flags){
 static const uint8_t states[]={SG_GATE,SG_ARCH,SG_COLUMN,SG_COLUMN,SG_COLUMN,SG_REACTIVE,SG_SOLID_A,SG_SOLID_B,SG_GUARD_ROOT,SG_BOX,SG_TILE_A,SG_TILE_B,SG_BOUNCE,SG_ROW8_ROOT,SG_SINGLE,SG_SINGLE_TILE,SG_ROW9_ROOT,SG_BREAKABLE,SG_FINALE,SG_GUARDIAN,SG_REWARD,SG_REWARD_DECOR};
 if((unsigned)id>=sizeof(states))return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,states[id]);struct EnemyActor*q=&e->payload.enemy;q->siege.config=config;q->actor.display_flags|=flags;
 if(id>=SIEGE_COLUMN0&&id<=SIEGE_COLUMN2)q->dead=(uint8_t)(id-SIEGE_COLUMN0);return 1;
}
int siege_spawn_template(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if((id<56||id>58)&&id!=253)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,id==253?SG_GUARD:SG_DRIFT+id-56);struct EnemyActor*q=&e->payload.enemy;if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];
 if(id==253){q->siege.variant=1;q->dead=q->params[0];q->actor.transform.facing=q->params[1];q->actor.transform.priority=0xc000;}return 1;
}

/* Original $02942A placement conversion; the main simulation stores world
 * coordinates while this source list is relative to the secondary view. */
int siege_spawn_scene3_record(struct WeaponWorld*w,struct EntityRef parent,unsigned index){
 /* Source5DCCE always chooses a global group and FIRST child. */
 (void)parent;return scene_placement_spawn(w,3,index);
}
