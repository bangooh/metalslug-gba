#include "scene_video.h"
#include "bosses.h"
/* Template 132, $07BA7C..$07CEE6. Root -> moving support -> body/turret
 * -> smoke/shell children. Animation changes the root's intro phase. */
enum State {ROOT,WAIT,INTRO,WAIT_DEATH,ARM,ARM_IDLE,SWING_LEFT,SWING_RIGHT,SWING_FAR,
 BODY,TURRET,TURRET_IDLE,RECOIL,SMOKE,PUFF_LEFT,PUFF_RIGHT,PUFF,DEATH_SWING,DEATH_FALL,
 DESTROYED,FREE_PARENT,SHELL,IMPACT,SHELL_SMOKE,SHELL_FIRE,DELAY_FIRE,TRAIL,FLASH,SPARK,BURST,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int callback(struct WeaponWorld*,struct NativeEntity*,unsigned);
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static int root_position(struct WeaponWorld*w,struct WeaponActor*a){
 if(!w->secondary_view_valid)return enemy_fault(w);
 /* $07BA86 calls $096A5A, then subtracts one from screen X. Its
  * screen-up Y is 385-256+source_camera_y. Convert both coordinates
  * into the continuous native world, including entry after M1. */
 a->transform.x=(int32_t)word(2704+w->camera_x-w->secondary_x_offset)*256;
 a->transform.y=(int32_t)word(383+w->camera_y-w->secondary_y_offset)*256;
 return 1;
}
static uint16_t rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){
 if(!w->interactions->rank_valid){enemy_fault(w);return 0;}
 return boss2_rank[n][interaction_rank(w,&q->actor,runtime_owner_boss2[n])][q->source_flags&3][(w->interactions->player_count_flag&2)>>1];
}
static struct EnemyActor*parent(struct WeaponWorld*w,struct NativeEntity*e){
 struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;
}
static struct EnemyActor*grandparent(struct WeaponWorld*w,struct NativeEntity*e){
 struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p){enemy_fault(w);return 0;}return parent(w,p);
}
static void follow(struct WeaponActor*a,const struct WeaponActor*p){
 transform_integer_offset(&a->transform,word((p->transform.x>>8)-(a->transform.x>>8)),word(transform_y_up(&p->transform)-transform_y_up(&a->transform)));
}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(!n)return 0;
 n->payload.enemy.boss2=(__typeof__(n->payload.enemy.boss2)){0};enemy_schedule(&n->payload.enemy,state);return n;
}
static int move(struct WeaponWorld*w,struct WeaponActor*a,enum WeaponMove mode){return interaction_move(w,a,mode)>=0;}
static void reset_velocity(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){return boss_animate(w,e,callback);}
static int outside(struct WeaponWorld*w,struct WeaponActor*a){return interaction_outside(w,a,boss_fx_bounds[8],1);}
static int arm_position(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=parent(w,e);if(!p)return 0;
 unsigned angle=(uint16_t)q->boss2.angle>>8;if(angle>=176)return enemy_fault(w);
 follow(&q->actor,&p->actor);transform_integer_offset(&q->actor.transform,-96+boss2_curve[angle][0],-boss2_curve[angle][1]);return 1;
}
static int hit_sound(struct WeaponWorld*w,struct WeaponActor*a){
 if(!(a->hit.flags&1)&&(a->hit.events&1)&&a->hit.last_kind==1)return interaction_sound(w,0x108d);return 1;
}
static int aim(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=parent(w,e);if(!p)return 0;
 unsigned mask=(enemy_player_live(w->players[0])?1:0)|(enemy_player_live(w->players[1])?2:0),slot=mask==2?1:0;
 if(mask==3){if(++q->boss2.target_ticks>=200){q->boss2.target_ticks=0;q->boss2.target_slot^=1;}slot=q->boss2.target_slot;}
 if(q->boss2.cooldown)q->boss2.cooldown--;
 struct PlayerTransform empty={.x=(w->camera_x-1)*256,.y=(513+w->camera_y)*256};
 const struct PlayerTransform*t=w->players[slot]?&w->players[slot]->actor.transform:&empty;
 unsigned angle=enemy_angle(word((t->x>>8)-(q->actor.transform.x>>8)),word(transform_y_up(t)-transform_y_up(&q->actor.transform)));
 for(unsigned i=0;i<7;i++){
  const int16_t*s=boss2_sectors[i];if((int)angle<s[0]||(int)angle>=s[1])continue;
  if(s[3]!=q->boss2.pose&&++q->boss2.pose_timer>=5){q->boss2.pose_timer=0;q->boss2.pose+=(s[3]>q->boss2.pose)?1:-1;}
  p->boss2.command=0;int d=s[2]-(int)angle;
  if(d< -3){p->boss2.command=1;return 0;}if(d>3){p->boss2.command=-1;return 0;}
  if(q->boss2.cooldown)return 0;q->boss2.angle=(int16_t)angle;return 1;
 }
 /* The source's final DBRA exit leaves carry from the failed upper-bound
  * comparison clear, so no sector also means no shot. */
 return 0;
}
static int shell_spawn(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*root=grandparent(w,e);if(!root)return 0;unsigned index=(uint16_t)q->boss2.pose;
 if(index>=7)return enemy_fault(w);const int16_t*offset=boss2_muzzles[index];
 if(!root->actor.phase){
  if(!interaction_sound(w,0x1065))return 0;struct NativeEntity*n=child(w,e,SHELL);if(!n)return 0;
  n->payload.enemy.boss2.angle=q->boss2.angle;transform_integer_offset(&n->payload.enemy.actor.transform,offset[0],offset[1]);
 }
 struct NativeEntity*n=child(w,e,SPARK);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,offset[0],offset[1]);return 1;
}
static int wreck(struct WeaponWorld*w,struct NativeEntity*e,unsigned index){
 static const int16_t xy[9][2]={{-96,-8},{-64,-12},{-40,-16},{-24,-16},{-8,-24},{16,-28},{32,-32},{48,-32},{72,-32}};
 struct PlayerTransform t=e->payload.enemy.actor.transform;transform_integer_offset(&t,xy[index][0],xy[index][1]);
 if(index==0||index==8){if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,BFX_SMOKE))return 0;}
 else if(!boss_fx_pattern(w,entity_ref(e),&t,BP_SMALL))return 0;
 if(index<2){if(!boss_fx_pattern(w,entity_ref(e),&t,BP_WRECK0))return 0;}
 else if(index<8){if(!boss_fx_pattern(w,entity_ref(e),&t,BP_WRECK1))return 0;}
 if(!boss_fx_pattern(w,entity_ref(e),&t,index==0||index==1||index==8?BP_WRECK2:BP_WRECK3))return 0;
 return index!=8||boss_fx_pattern(w,entity_ref(e),&t,BP_DEBRIS)!=0;
}
static int callback(struct WeaponWorld*w,struct NativeEntity*e,unsigned id){
 static const unsigned wr[]={BC_7CA60,BC_7CAB0,BC_7CAFA,BC_7CB44,BC_7CB8E,BC_7CBD8,BC_7CC22,BC_7CC6C,BC_7CCB6};
 static const unsigned bursts[]={BC_7CD06,BC_7CD24,BC_7CD42,BC_7CD60,BC_7CD7E,BC_7CD9C,BC_7CDBA,BC_7CDD8,BC_7CDF6};
 static const unsigned patches[]={BC_7CE92,BC_7CE9C,BC_7CEA6,BC_7CEB0,BC_7CEBA,BC_7CEC4,BC_7CECE,BC_7CED8};
 static const int16_t offsets[9][2]={{-128,-32},{-88,-40},{-72,-56},{-56,-56},{-40,-56},{-8,-56},{8,-56},{8,-56},{32,-56}};
 if(id==BC_7C918)return shell_spawn(w,e);
 for(unsigned i=0;i<9;i++){
  if(id==wr[i])return wreck(w,e,i);
  if(id==bursts[i]){
   struct NativeEntity*n=child(w,e,BURST);if(!n)return 0;n->payload.enemy.selector=i==0?0:i==8?2:1;
   transform_integer_offset(&n->payload.enemy.actor.transform,offsets[i][0],offsets[i][1]);return 1;
  }
 }
 for(unsigned i=0;i<8;i++)if(id==patches[i]){w->enemies->boss2_patch_mask|=(uint8_t)(1u<<i);const struct BossBackgroundPatch*p=&boss2_background[i];struct SceneTilePatch tile={.cells=p->tiles,.x=p->x,.y=p->y,.width=p->width,.height=p->height,.plane=SCENE_TILE_MAIN};return scene_video_patch(w->video,&tile)?1:enemy_fault(w);}
 return enemy_fault(w);
}
/* Returns 2 for source entries that install the following task and return. */
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;q->entry=0;
 switch(s){
 case ROOT:
  a->palette=0x34;if(!root_position(w,a))return 0;interaction_priority(w,a,0,28);
  a->phase=1;q->dead=0;reset_velocity(a);enemy_schedule(q,WAIT);return 2;
 case WAIT:if(!child(w,e,ARM))return 0;boss_start(q,BA_M2_WAIT);return 1;
 case INTRO:if(!interaction_sound(w,0x28))return 0;boss_start(q,BA_M2_INTRO);return 1;
 case WAIT_DEATH:return 1;
 case ARM:
  q->boss2=(__typeof__(q->boss2)){0};a->phase=q->dead=0;
  if(!child(w,e,BODY)||!child(w,e,TURRET))return 0;a->state=ARM_IDLE;return 1;
 case ARM_IDLE:q->boss2.pose=0;return 1;
 case SWING_LEFT:q->boss2.pose=1;return 1;
 case SWING_RIGHT:case SWING_FAR:q->boss2.pose=2;return 1;
 case BODY:a->palette=0x2a;a->hit_palette=0x78;interaction_priority(w,a,0,8);a->hit.hp=1;reset_velocity(a);q->dead=0;q->boss2.pose=-1;return 1;
 case TURRET:
  a->palette=0x2a;a->hit_palette=0x78;a->display_flags|=2;interaction_priority(w,a,0,20);
  a->hit.hp=(int16_t)rank(w,q,0);if(w->fault||!a->hit.hp)return enemy_fault(w);
  reset_velocity(a);q->boss2=(__typeof__(q->boss2)){.pose=3,.max_hp=a->hit.hp};a->phase=q->dead=0;
  if(!child(w,e,SMOKE))return 0;a->hit.flags|=8;a->state=TURRET_IDLE;return 1;
 case TURRET_IDLE:return 1;
 case RECOIL:
  if((unsigned)q->boss2.pose>=7)return enemy_fault(w);boss_start_entry(q,boss_m2_recoil[q->boss2.pose]);
  q->boss2.cooldown=rank(w,q,1);a->hit.flags&=247;return !w->fault;
 case SMOKE:q->boss2.smoke_left=0;q->boss2.smoke_right=5;return 1;
 case PUFF_LEFT:case PUFF_RIGHT:
  a->palette=13;transform_integer_offset(&a->transform,s==PUFF_LEFT?-32:32,s==PUFF_LEFT?16:0);
  a->vx=s==PUFF_LEFT?-614:819;if(a->transform.facing&1)a->vx=word(-a->vx);
  a->vy=-1020;a->ay=word(204+(random_word(w)&63)-31);a->ax=0;boss_start(q,BA_M2_PUFF);a->state=PUFF;return 1;
 case PUFF:return 1;
 case DEATH_SWING:
  for(unsigned i=0;i<2;i++){struct NativeEntity*n=human_corpse_entity(w,entity_ref(e),a,0);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,i?16:-16,16);}
  q->boss2.death_ticks=0;if(!interaction_sound(w,0x102d))return 0;q->boss2.pose=2;return 1;
 case DEATH_FALL:
  a->vx=a->transform.facing&1?-614:614;a->vy=250;a->ay=-25;a->ax=0;
  if(!interaction_sound(w,0x23)||!interaction_sound(w,0x1033))return 0;q->boss2.pose=2;return 1;
 case DESTROYED:{
  if(!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_DEATH_FIRE))return 0;
  struct NativeEntity*n=boss_fx_pattern(w,entity_ref(e),&a->transform,BP_DEATH_BLAST);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xd000;
  struct EnemyActor*p=parent(w,e);if(!p)return 0;p->actor.phase=255;a->display_flags&=253;w->enemies->scene_flags[1]=1;return 1;
 }
 case FREE_PARENT:{struct EnemyActor*p=parent(w,e);if(!p)return 0;q->dead=p->dead=255;entity_delete(w->entities,e);return 2;}
 case SHELL:
  a->palette=0x122;weapon_vector((uint8_t)q->boss2.angle,(int16_t)rank(w,q,2),&a->vx,&a->vy);
  a->hit.hp=1;interaction_priority(w,a,0xd000,16);a->hit.type_flags|=16;q->boss2.pose_timer=0;boss_start(q,BA_M2_SHELL);return !w->fault;
 case IMPACT:
  if(!interaction_sound(w,0x1021)||!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_IMPACT)||!child(w,e,TRAIL))return 0;
  for(unsigned i=0;i<4;i++){
   struct NativeEntity*n=child(w,e,DELAY_FIRE);if(!n)return 0;n->payload.enemy.actor.timer=(uint16_t)(4+4*i);n->payload.enemy.actor.transform.priority=0x2000;
   transform_integer_offset(&n->payload.enemy.actor.transform,0,32+16*i);
  }
  /* Fall through to the source shell's common smoke transform. */
  /* FALLTHROUGH */
 case SHELL_SMOKE:a->transform.priority=0x2000;return boss_fx_transform(w,e,BFX_SMOKE)?2:0;
 case SHELL_FIRE:return boss_fx_transform(w,e,BFX_FIRE)?2:0;
 case DELAY_FIRE:return 1;
 case TRAIL:boss_start(q,BA_M2_TRAIL);return 1;
 case FLASH:a->palette=0x122;interaction_priority(w,a,0x4000,16);boss_start(q,BA_M2_FLASH);return 1;
 case SPARK:a->palette=8;interaction_priority(w,a,0x4000,20);reset_velocity(a);boss_start(q,BA_M2_SPARK);return 1;
 case BURST:{
  static const unsigned bindings[]={BB_M2_BURST_A,BB_M2_BURST_B,BB_M2_BURST_C};if(q->selector>=3)return enemy_fault(w);
  boss_attack_binding(q,bindings[q->selector]);hit_begin_attack(&a->hit);a->palette=0x122;reset_velocity(a);interaction_priority(w,a,0xf000,16);
  boss_start(q,BA_M2_BURST);animate(w,e);if(!enemy_attack(w,a))return 0;entity_delete(w->entities,e);return 2;
 }
 case DELETE:entity_delete(w->entities,e);return 2;
 default:return enemy_fault(w);
 }
}
static void swing(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;int requested=state==SWING_LEFT?-1:state==SWING_RIGHT?1:2;
 int accelerate=q->boss2.command==requested;
 if(state!=SWING_FAR){if(accelerate)q->boss2.forced=0;else accelerate=q->boss2.forced!=0;}
 int16_t speed=word(q->boss2.speed+(accelerate?64:-160)),limit=state==SWING_FAR?320:384;
 if(accelerate){if(speed>=limit)speed=limit;}
 else if(speed<0){speed=0;enemy_schedule(q,ARM_IDLE);}q->boss2.speed=speed;
 int16_t angle=word(q->boss2.angle+(state==SWING_LEFT?-speed:speed));
 int wall=state==SWING_LEFT?angle<0:angle>0x7e00;
 if(wall){angle=state==SWING_LEFT?0:0x7e00;
  if(state!=SWING_FAR&&++q->boss2.wall_ticks>=20){q->boss2.speed=q->boss2.wall_ticks=0;q->boss2.forced=-1;enemy_schedule(q,state==SWING_LEFT?SWING_RIGHT:SWING_LEFT);}
 }else if(state!=SWING_FAR)q->boss2.wall_ticks=0;
 q->boss2.angle=angle;if(!arm_position(w,e))return;animate(w,e);if(q->actor.phase==255)enemy_schedule(q,DEATH_SWING);
}
static void turret(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy,*p=parent(w,e),*root=grandparent(w,e);if(!p||!root)return;
 struct WeaponActor*a=&q->actor;follow(a,&p->actor);
 if(state!=RECOIL){if((unsigned)q->boss2.pose>=7){enemy_fault(w);return;}boss_start_entry(q,boss_m2_turret[q->boss2.pose]);}
 int done=animate(w,e);if(state==RECOIL&&done)enemy_schedule(q,TURRET_IDLE);
 if(a->hit.events&128)p->actor.hit.events|=128;
 if(state==DESTROYED){if(p->dead==255)enemy_schedule(q,DELETE);return;}
 if(state==RECOIL){a->hit.flags&=247;if(root->actor.phase)a->hit.hp=q->boss2.max_hp;if(!hit_sound(w,a))return;if(p->dead==255)enemy_schedule(q,DELETE);return;}
 if(root->actor.phase){
  if(root->actor.phase==1)return;a->hit.flags&=247;a->hit.hp=q->boss2.max_hp;if(!hit_sound(w,a))return;
  if(root->actor.phase==2){p->boss2.command=2;return;}
  if(root->actor.phase!=3)return;p->boss2.command=0;
  if(!q->boss2.death_ticks){q->boss2.death_ticks=65535;if(!interaction_sound(w,0x1074))return;}
  if(++q->boss2.pose_timer>=5){q->boss2.pose_timer=0;if(++q->boss2.pose>=7){q->boss2.pose=6;enemy_schedule(q,RECOIL);}}return;
 }
 a->hit.flags&=247;if(aim(w,e))enemy_schedule(q,RECOIL);if(!hit_sound(w,a))return;
 if(a->hit.flags&1)enemy_schedule(q,DESTROYED);if(p->dead==255)enemy_schedule(q,DELETE);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry&&enter(w,e)!=1)return;unsigned s=a->state;struct EnemyActor*r;int done;
 switch(s){
 case WAIT:if(!move(w,a,WMOVE_SYNC))return;animate(w,e);if(boss2_scroll<=w->secondary_x_offset){w->enemies->intro_gate=0;enemy_schedule(q,INTRO);}return;
 case INTRO:if(!move(w,a,WMOVE_SYNC))return;if(animate(w,e))enemy_schedule(q,WAIT_DEATH);w->enemies->intro_gate=0;return;
 case WAIT_DEATH:
  if(!move(w,a,WMOVE_SYNC))return;if(q->dead){a->display_flags&=253;entity_delete(p,e);}else w->enemies->intro_gate=0;return;
 case ARM_IDLE:
  if(!arm_position(w,e))return;if(q->boss2.command==-1)enemy_schedule(q,SWING_LEFT);
  if(q->boss2.command==1)enemy_schedule(q,SWING_RIGHT);if(q->boss2.command==2)enemy_schedule(q,SWING_FAR);
  if(a->phase==255)enemy_schedule(q,DEATH_SWING);return;
 case SWING_LEFT:case SWING_RIGHT:case SWING_FAR:swing(w,e,s);return;
 case BODY:
  r=parent(w,e);if(!r)return;follow(a,&r->actor);
  if(q->boss2.pose!=r->boss2.pose){if((unsigned)r->boss2.pose>=3){enemy_fault(w);return;}boss_start_entry(q,boss_m2_body[r->boss2.pose]);}
  q->boss2.pose=r->boss2.pose;if(r->actor.hit.events&128)a->hit.events|=1;animate(w,e);
  if(r->actor.phase==255&&word((a->transform.x>>8)-w->camera_x)>320&&!q->dead){
   w->enemies->scene_flags[1]=1;q->dead=255;struct NativeEntity*n=boss_fx_pattern(w,entity_ref(e),&a->transform,BP_BODY);if(!n)return;n->payload.enemy.actor.transform.priority=0xd000;
   struct PlayerTransform t=a->transform;transform_integer_offset(&t,-32,0);t.priority++;
   for(unsigned i=0;i<16;i++)if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,BFX_METAL))return;
  }
  if(r->dead==255)enemy_schedule(q,DELETE);return;
 case TURRET_IDLE:case RECOIL:case DESTROYED:turret(w,e,s);return;
 case SMOKE:{
  r=parent(w,e);if(!r)return;follow(a,&r->actor);interaction_priority(w,a,0,12);
  struct EnemyActor*g=grandparent(w,e);if(!g)return;if(g->actor.phase==255)enemy_schedule(q,DELETE);
  if(r->boss2.max_hp<=0){enemy_fault(w);return;}unsigned index=(uint16_t)r->actor.hit.hp*10u/(uint16_t)r->boss2.max_hp;
  if(index>10){enemy_fault(w);return;}uint16_t period=boss2_smoke_periods[index];if(!period)return;
  if(++q->boss2.smoke_left>=period){q->boss2.smoke_left=0;if(!child(w,e,PUFF_LEFT))return;}
  if(++q->boss2.smoke_right>=period){q->boss2.smoke_right=0;child(w,e,PUFF_RIGHT);}return;
 }
 case PUFF:if(!move(w,a,WMOVE_BODY))return;if(animate(w,e)||outside(w,a))enemy_schedule(q,DELETE);return;
 case DEATH_SWING:{
  int16_t speed=word(q->boss2.speed+64);if(speed>=384)speed=384;q->boss2.speed=speed;
  uint16_t angle=(uint16_t)word(q->boss2.angle+speed);if(angle>0xaf00)angle=0xaf00;q->boss2.angle=(int16_t)angle;
  if(!arm_position(w,e))return;uint16_t count=++q->boss2.death_ticks;if((count==25||count==90)&&!interaction_sound(w,0x1032))return;
  if(angle>=0xaf00)enemy_schedule(q,DEATH_FALL);return;
 }
 case DEATH_FALL:if(!move(w,a,WMOVE_POINT))return;if(interaction_outside(w,a,boss_fx_bounds[7],0))enemy_schedule(q,FREE_PARENT);return;
 case SHELL:{
  int collision=interaction_move(w,a,WMOVE_FALL);if(collision<0)return;if(collision)enemy_schedule(q,IMPACT);
  animate(w,e);if(!enemy_attack(w,a))return;if(a->hit.flags&2)enemy_schedule(q,SHELL_SMOKE);
  if(++q->boss2.pose_timer>=15){q->boss2.pose_timer=0;if(!child(w,e,FLASH))return;}
  r=grandparent(w,e);if(!r)return;
  if(r->actor.phase==255){boss_defense(q,65535);enemy_schedule(q,SHELL_FIRE);}
  if(outside(w,a))enemy_schedule(q,DELETE);return;
 }
 case DELAY_FIRE:
  if(!--a->timer){transform_integer_offset(&a->transform,(random_word(w)&47)-31,0);boss_fx_schedule(w,e,BFX_FIRE);}return;
 case TRAIL:
  if(!move(w,a,WMOVE_SYNC))return;if(animate(w,e))enemy_schedule(q,DELETE);enemy_attack(w,a);return;
 case FLASH:if(!move(w,a,WMOVE_POINT))return;if(animate(w,e))enemy_schedule(q,DELETE);return;
 case SPARK:if(!move(w,a,WMOVE_SYNC))return;done=animate(w,e);if(done||outside(w,a))enemy_schedule(q,DELETE);return;
 default:enemy_fault(w);return;
 }
}
int boss2_spawn(struct WeaponWorld*w,struct EntityRef owner,enum EntityInsert order,const struct PlayerTransform*t,const uint8_t params[8]){
 struct NativeEntity*e=enemy_allocate(w,owner,order,t,tick);if(!e)return 0;e->payload.enemy.boss2=(__typeof__(e->payload.enemy.boss2)){0};
 if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];enemy_schedule(&e->payload.enemy,ROOT);return 1;
}
