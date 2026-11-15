#include "actors.h"
#include "../generated/actor-assets.h"
#include "terrain_edits.h"
struct Actors actors;
static int abs_i(int x){return x<0?-x:x;}
/* $03EF5C: unsigned WORD comparisons, exclusive upper edges. dy is UP. */
int actor_pickup_overlap(int16_t dx,int16_t dy,uint8_t flags,unsigned ox,unsigned sx,unsigned oy,unsigned sy){
 return !(flags&9)&&(uint16_t)(dx+ox)<sx&&(uint16_t)(dy+oy)<sy;
}
static void start(struct Actor*a,unsigned seq){
 a->animation=(struct ActorAnim){.frame=actor_poses[actor_sequences[seq].first].frame,
   .sequence=(uint8_t)seq,.defense=a->animation.defense};
}
static unsigned animate(struct Actor*a){
 struct ActorAnim*v=&a->animation;const struct ActorSequence*s=&actor_sequences[v->sequence];
 const struct ActorPose*p=&actor_poses[s->first+v->pose];unsigned events=0;
 v->frame=p->frame;
 if(v->age==0&&!v->done){
  events=p->events;if(p->defense!=254)v->defense=p->defense;
  if(events&2)a->hit.flip^=1;
  if(p->sound)combat_sound(p->sound);
 }
 if(!v->done&&++v->age>=p->hold){
  if(v->pose+1<s->count){v->pose++;v->age=0;}
  else if(s->loop>=0){v->pose=(uint8_t)s->loop;v->age=0;}
  else v->done=1;
 }
 return events;
}
static void sync(struct Actor*a){a->hit.x=(int16_t)(a->x>>8);a->hit.y=(int16_t)(512-(a->y>>8));}
void actors_reset(void){actors=(struct Actors){0};}
unsigned actors_enemies(void){unsigned n=0;for(unsigned i=0;i<ACTOR_CAPACITY;i++){
 const struct Actor*a=&actors.slots[i];if(a->active&&a->counted&&a->state!=SOLDIER_DEAD)n++;
 }return n;}
int actors_spawn(const struct MissionSpawn*s,struct SourceRng*r,int camera_x){
 if(s->template!=1&&s->template!=4&&s->template!=5&&s->template!=0xb6&&s->template!=0x105&&s->template!=0x106&&s->template!=0xf9&&s->template!=0x12d&&s->template!=0x136&&s->template!=0xc0&&s->template!=0xac)return 0;
 // Grenade variant only. Unported ranged weapons must not become rifle clones.
 if(s->template==0xf9&&(s->params[4]!=0||(s->params[5]!=1&&s->params[5]!=2)))return 0;
 struct Actor*a=0;unsigned live=0;
 for(unsigned i=0;i<ACTOR_CAPACITY;i++){if(actors.slots[i].active)live++;else if(!a)a=&actors.slots[i];}
 if(!a){actors.fault=1;return 0;}if(live+1>actors.peak)actors.peak=(uint16_t)(live+1);
 *a=(struct Actor){.x=s->x*256,.y=s->y*256,.source=s->source,.active=1,
   .counted=s->counted,.subtype=s->subtype,.template=s->template};
 for(unsigned i=0;i<8;i++)a->params[i]=s->params[i];
 a->hit=(struct HitActor){.id=combat.next_id++,.hp=1}; // explicit clean-slot HP adaptation
 if(s->template==0xc0||s->template==0xac){
  a->kind=ACTOR_POW;a->hit.flip=!!s->params[2];
  start(a,s->template==0xc0?ACTOR_POW_BOUND:ACTOR_POW_IDLE);
  a->animation.defense=actor_poses[actor_sequences[a->animation.sequence].first].defense;
 }else if(s->template==0x12d||s->template==0x136){
  a->kind=ACTOR_FOOD;a->hit.flip=s->params[2]!=255;a->hit.invulnerable=8;
  a->timer=s->params[1]?(uint16_t)(s->params[1]<<4):32767;
  start(a,s->template==0x12d?ACTOR_FOOD_301:ACTOR_FOOD_310);a->animation.defense=255;
 }else if(s->template==0x105||s->template==0x106){
  unsigned n=s->template==0x106;a->kind=ACTOR_SIGN;start(a,n?ACTOR_SIGN_B:ACTOR_SIGN_A);a->animation.defense=255;
  const struct TerrainStamp*stamp=&sign_stamps[n];
  if(!terrain_stamp(s->x+stamp->dx,s->y+stamp->dy,stamp->width,stamp->height,stamp->cells)){actors.fault=3;return 0;}
 }else if(s->template==0xb6){
  a->kind=ACTOR_CRITTER;a->hit.flip=s->params[0]&1;a->vx=(int16_t)((s->params[1]<<5)*(a->hit.flip?1:-1));
  a->timer=(uint16_t)(6+(source_random_word(r)&7));start(a,ACTOR_BIRD);a->animation.defense=255;
 }else{
  a->kind=s->template==0xf9?ACTOR_CHARGER:ACTOR_SOLDIER;
  a->hit.flip=(s->template==5||((s->template==1||s->template==0xf9)&&s->x-camera_x<160));
  int floor=terrain_floor(s->x,s->y-1,s->y+8);
  if(floor>=0){a->y=floor*256;a->grounded=1;}
  a->vx=(int16_t)(soldier_walk_speeds[(source_random_word(r)&14)>>1]*(a->hit.flip?1:-1));
  a->state=SOLDIER_WALK;start(a,ACTOR_WALK);a->animation.defense=0;
  if(a->kind==ACTOR_CHARGER){
   // $04290C/$042A44 initial patrol. Native rank fixed to source first row.
   a->home_x=s->x;a->timer=(uint16_t)((s->params[2]*0x14c)>>8);
   a->vx=a->hit.flip?0x100:-0x100;start(a,s->params[5]==1?ACTOR_CHARGER_RUN:ACTOR_CHARGER_WALK);
   if(!s->params[0]){a->state=SOLDIER_IDLE;a->vx=0;start(a,ACTOR_IDLE);}
  }
 }
 sync(a);actors.spawned++;return 1;
}
/* Native world-terrain adapter, shared immutable source collision map.
 * This is deliberately not advertised as the original $056ACC physics. */
static void physics(struct Actor*a){
 int old_y=a->y,old_x=a->x;a->x+=a->vx;
 if(terrain_solid(a->x>>8,(a->y>>8)-20,0)){a->x=old_x;a->vx=0;}
 if(a->grounded){int floor=terrain_floor(a->x>>8,(a->y>>8)-8,(a->y>>8)+8);
  if(floor>=0)a->y=floor*256;else{a->grounded=0;a->vy=0;}}
 if(!a->grounded){a->vy=source_clamp_velocity((int16_t)(a->vy-0x80),0x600);a->y-=a->vy;
  if(a->vy<=0){int floor=terrain_floor(a->x>>8,(old_y>>8)-1,a->y>>8);
   if(floor>=0){a->y=floor*256;a->grounded=1;a->vy=0;}}
 }
 sync(a);
}
void actors_damage(void*user,const struct HitDamage*e){
 (void)user;if(!e->killed)return;
 for(unsigned i=0;i<ACTOR_CAPACITY;i++){
  struct Actor*a=&actors.slots[i];if(a->active&&a->hit.id==e->target&&a->kind==ACTOR_POW){
   if(!a->state){a->state=1;a->animation.defense=255;start(a,a->template==0xc0?ACTOR_POW_FREE:ACTOR_POW_UNTIE);}
   return;
  }
  if(a->active&&a->hit.id==e->target&&a->state!=SOLDIER_DEAD){
   a->state=SOLDIER_DEAD;a->vx=0;a->animation.defense=255;start(a,ACTOR_DEATH);actors.killed++;return;
  }
 }
}
unsigned actors_targets(struct HitActor**hit,const struct HitDefense**def,unsigned cap){
 unsigned n=0;for(unsigned i=0;i<ACTOR_CAPACITY;i++){
  struct Actor*a=&actors.slots[i];if(!a->active||a->animation.defense==255)continue;
  if(a->kind==ACTOR_POW){if(a->state)continue;}
  else if((a->kind!=ACTOR_SOLDIER&&a->kind!=ACTOR_CHARGER)||a->state==SOLDIER_DEAD)continue;
  if(n>=cap){actors.fault=2;break;}hit[n]=&a->hit;def[n]=&actor_defenses[a->animation.defense];n++;
 }return n;
}
static void grenade(struct Actor*owner,const struct Player*p,int camera_y){
 struct Actor*a=0;unsigned live=0;
 for(unsigned i=0;i<ACTOR_CAPACITY;i++){if(actors.slots[i].active)live++;else if(!a)a=&actors.slots[i];}
 if(!a){actors.fault=4;return;}if(live+1>actors.peak)actors.peak=(uint16_t)(live+1);
 /* $057044/$05707A chooses a source trajectory from target relative Y.
  * $055BCE creates it exactly 32 source pixels above the thrower. */
 int dy=(owner->y-p->y)>>8,screen_up=512-(owner->y>>8)+camera_y;
 unsigned base=owner->hit.flip?17:4,far=owner->hit.flip?19:16;
 unsigned choice=screen_up>=384?far:dy>96?base+1:dy< -64?far:base;
 const int16_t*t=grenade_trajectories[choice];
 *a=(struct Actor){.x=owner->x,.y=owner->y-32*256,.active=1,.kind=ACTOR_PROJECTILE,
  .vx=owner->hit.flip?(int16_t)-t[0]:t[0],.vy=t[2],.gravity=t[1],.spawn_y=(int16_t)((owner->y>>8)-32),.source=0x55bce};
 a->hit=(struct HitActor){.id=combat.next_id++,.flip=owner->hit.flip};start(a,ACTOR_GRENADE);a->animation.defense=255;sync(a);actors.grenades++;
}
static void projectile(struct Actor*a){
 if(a->state){animate(a);if(a->animation.done)a->active=0;return;}
 int old_y=a->y;a->x+=a->vx;a->y-=a->vy;a->vy=(int16_t)(a->vy+a->gravity);
 // $055ECA damping once below the spawn height. Terrain integration remains
 // the native immutable-world adapter, not the unported $027BC8 routine.
 if((a->y>>8)>a->spawn_y){a->vy=(int16_t)(a->vy-(a->vy>>5));
  if(a->vx){int delta=a->vx>>5;a->vx=(int16_t)(a->vx-(delta?delta:1));}}
 sync(a);unsigned events=animate(a);if(events&8)hit_begin_attack(&a->hit);
 hit_pair(&a->hit,&soldier_grenade,&combat.player,&actor_defenses[PLAYER_DEFENSE],1,combat_player_damage,0);
 int floor=a->vy<=0?terrain_floor(a->x>>8,old_y>>8,a->y>>8):-1;
 if((a->hit.flags&2)||floor>=0||terrain_solid(a->x>>8,a->y>>8,0)){
  if(floor>=0)a->y=floor*256;a->state=1;a->vx=a->vy=0;start(a,ACTOR_BURST);combat_sound(0x1027);
 }
}
static void charger(struct Actor*a,const struct Player*p,int camera_y,unsigned events){
 if(events&4)grenade(a,p,camera_y);
 if(a->state==SOLDIER_WALK){
  if(abs_i((a->x>>8)-a->home_x)>=a->params[0]*8){a->state=SOLDIER_IDLE;a->vx=0;start(a,ACTOR_IDLE);}
 }else if(a->state==SOLDIER_IDLE){
  a->hit.flip=p->x>=a->x;
  if(a->timer)a->timer--;
  else if(a->params[1]){a->state=a->params[5]==1?SOLDIER_WINDUP:SOLDIER_THROW;start(a,a->params[5]==1?ACTOR_STAND:ACTOR_THROW);}
  else {a->state=SOLDIER_LEAVE;a->hit.flip=p->x<a->x;a->vx=a->hit.flip?0x300:-0x300;start(a,ACTOR_WALK);}
 }else if(a->state==SOLDIER_WINDUP&&a->animation.done){a->state=SOLDIER_THROW;a->params[5]=2;start(a,ACTOR_THROW);}
 else if(a->state==SOLDIER_THROW&&a->animation.done){
  a->params[1]--;unsigned delay=(a->params[3]*0x14c)>>8;a->timer=(uint16_t)(delay>255?255:delay);
  a->state=SOLDIER_IDLE;start(a,ACTOR_IDLE);
 }else if(a->state==SOLDIER_MELEE&&a->animation.done){a->state=SOLDIER_IDLE;start(a,ACTOR_IDLE);}
 if(a->state==SOLDIER_WALK||a->state==SOLDIER_IDLE){
  struct HitActor probe=a->hit;hit_begin_attack(&probe);
  hit_pair(&probe,&soldier_probe,&combat.player,&actor_defenses[PLAYER_DEFENSE],1,0,0);
  if(probe.flags&2){a->state=SOLDIER_MELEE;a->vx=0;hit_begin_attack(&a->hit);start(a,ACTOR_MELEE);}
 }
}
static void food(struct Actor*a,const struct Player*p){
 physics(a);
 if(!a->state){animate(a);if(!--a->timer){a->state=1;a->timer=30;}}
 else {if(a->timer&1)animate(a);if(!a->timer--){a->active=0;return;}}
 if(a->hit.invulnerable){a->hit.invulnerable--;return;}
 // $03EF5C + box $09A6D6: unsigned WORD ranges, upper edges exclusive.
 int16_t dx=(int16_t)((p->x>>8)-(a->x>>8)),dy=(int16_t)((a->y>>8)-(p->y>>8));
 if(!combat.player_dead&&actor_pickup_overlap(dx,dy,combat.player.flags,16,32,16,40)){
  unsigned tier=a->params[0];if(tier>14)tier=14;
  actors.score+=food_scores[tier];actors.pickups++;combat_sound(0x1121);a->active=0;
 }
}
static void pow_step(struct Actor*a,const struct Player*p,int camera_x){
 physics(a);unsigned events=animate(a);
 if(events&16)combat_drop(a->params[3],a->params[4],a->params[5],(a->x>>8)+(a->hit.flip?16:-16),(a->y>>8)-4);
 if(a->state==1&&a->animation.done){ // $03FC6A/$03FE9C -> $03FF24
  a->state=2;a->hit.flags=0;a->hit.hp=1;a->timer=15;
  if(a->params[0])a->hit.flip=a->params[0]!=1;
  a->vx=a->hit.flip?0x180:-0x180;start(a,ACTOR_POW_WALK);
 }else if(a->state==2){
  // Native patrol turnaround on static terrain; original ledge hopping is pending.
  if(!a->timer--){a->timer=30;a->hit.flip^=1;a->vx=(int16_t)-a->vx;}
  int16_t dx=(int16_t)((p->x>>8)-(a->x>>8)),dy=(int16_t)((a->y>>8)-(p->y>>8));
  if(!combat.player_dead&&actor_pickup_overlap(dx,dy,combat.player.flags,32,56,32,56)){
   actors.rescued++;combat_sound(0x113a);a->state=3;a->timer=4;
   a->hit.flip=(a->x>>8)-camera_x<=192;a->vx=a->hit.flip?0x700:-0x700;
  }
 }else if(a->state==3&&! --a->timer){
  a->hit.flip^=1;a->vx=0;a->state=a->params[3]?4:5;start(a,a->params[3]?ACTOR_POW_BOW:ACTOR_POW_WAVE);
 }else if(a->state==4&&a->animation.done){a->state=5;start(a,ACTOR_POW_WAVE);}
 else if(a->state==5&&a->animation.done){a->state=6;a->vx=-0x400;a->hit.flip=0;start(a,ACTOR_POW_EXIT);}
}
void actors_step(const struct Player*p,struct SourceRng*r,int camera_x,int camera_y){
 for(unsigned i=0;i<ACTOR_CAPACITY;i++){
  struct Actor*a=&actors.slots[i];if(!a->active)continue;
  if((a->x>>8)<camera_x-80||(a->y>>8)>448){a->active=0;actors.despawned++;continue;}
  a->hit.flags&=(uint8_t)~8u;
  if(a->kind==ACTOR_CRITTER){
   // $08E4FE/$08E566: world storage replaces the source scroll adjustment.
   a->vy=(int16_t)(a->vy+(a->state?-4:4));a->x+=a->vx;a->y-=a->vy;
   // Both conditional branches target the immediately following timer block.
   if(a->timer&&!--a->timer){a->state^=1;a->timer=(uint16_t)(6+(source_random_word(r)&7));}
   sync(a);animate(a);continue;
  }
  if(a->kind==ACTOR_SIGN){animate(a);continue;}
  if(a->kind==ACTOR_PROJECTILE){projectile(a);continue;}
  if(a->kind==ACTOR_FOOD){food(a,p);continue;}
  if(a->kind==ACTOR_POW){pow_step(a,p,camera_x);continue;}
  physics(a);
  if(a->state==SOLDIER_DEAD){animate(a);if(a->animation.done)a->active=0;continue;}
  /* Native reduced decision policy. Source speeds, action frames, hit shapes,
   * and melee callback timing are retained; crowd/rank/surrender/grab AI is
   * NOT yet a matching translation of $056B92/$057582. */
  unsigned events=animate(a);
  if(events&1){
   hit_pair(&a->hit,&soldier_melee,&combat.player,&actor_defenses[PLAYER_DEFENSE],1,combat_player_damage,0);
   if(a->hit.events&2)combat_sound(0x10fa);
  }
  if(a->kind==ACTOR_CHARGER){charger(a,p,camera_y,events);continue;}
  if(a->state==SOLDIER_MELEE){if(a->animation.done){a->state=SOLDIER_IDLE;start(a,ACTOR_IDLE);}continue;}
  if(a->state==SOLDIER_TURN){if(!a->animation.done)continue;a->state=SOLDIER_WALK;start(a,ACTOR_WALK);}
  if((a->x>>8)-camera_x<0||(a->x>>8)-camera_x>320)continue;
  if(!combat.player_dead){
   struct HitActor probe=a->hit;hit_begin_attack(&probe);
   hit_pair(&probe,&soldier_probe,&combat.player,&actor_defenses[PLAYER_DEFENSE],1,0,0);
   if(probe.flags&2){a->state=SOLDIER_MELEE;a->vx=0;hit_begin_attack(&a->hit);start(a,ACTOR_MELEE);continue;}
  }
  int dx=(p->x-a->x)>>8,dy=(p->y-a->y)>>8;
  if(abs_i(dx)<144&&abs_i(dy)<48){
   unsigned face=dx>=0;
   if(face!=a->hit.flip){a->state=SOLDIER_TURN;a->vx=0;start(a,ACTOR_TURN);continue;}
   if(abs_i(dx)<40){if(a->state!=SOLDIER_IDLE){a->state=SOLDIER_IDLE;a->vx=0;start(a,ACTOR_IDLE);}}
   else if(a->state!=SOLDIER_WALK){a->state=SOLDIER_WALK;start(a,ACTOR_WALK);}
  }
  if(a->state==SOLDIER_WALK)a->vx=(int16_t)(soldier_walk_speeds[0]*(a->hit.flip?1:-1));
 }
}
