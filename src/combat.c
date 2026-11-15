#include "combat.h"
#include "../generated/combat-assets.h"
#ifdef NATIVE_MISSION
#include "actors.h"
#endif
struct Combat combat;
void combat_sound(uint16_t command){
 if(combat.sounds<8)combat.sound[combat.sounds++]=command;else combat.fault=1;
}
#define sound combat_sound
static void animation_start(struct CombatAnim*a,unsigned sequence){
 *a=(struct CombatAnim){.sequence=(uint8_t)sequence};
 a->frame=combat_poses[combat_sequences[sequence].first].frame;
}
/* Compiled source $028D70 animation timings. Events happen on entry, not
 * every repeated drawing. done is published on the final held drawing. */
static unsigned animation_step(struct CombatAnim*a){
 const struct CombatSequence*s=&combat_sequences[a->sequence];
 const struct CombatPose*p=&combat_poses[s->first+a->pose];
 unsigned events=a->age==0&&!a->done?p->events:0;a->frame=p->frame;
 if(!a->done&&++a->age>=p->hold){
  if(a->pose+1<s->count){a->pose++;a->age=0;}
  else if(s->loop>=0){a->pose=(uint8_t)s->loop;a->age=0;}
  else a->done=1;
 }
 return events;
}
void combat_reset(void){combat=(struct Combat){.next_id=2};combat.player.id=1;combat.player.hp=1;}
void combat_clear_sounds(void){combat.sounds=0;}
void combat_begin_frame(const struct Player*p){
 combat.sounds=0;combat.player.flags&=(uint8_t)~8u;
 combat.player.x=(int16_t)(p->x>>8);combat.player.y=(int16_t)(512-(p->y>>8));combat.player.flip=p->facing;
 if(combat.player.invulnerable)combat.player.invulnerable--;
}
void combat_player_damage(void*user,const struct HitDamage*e){
 (void)user;if(e->target!=combat.player.id)return;
 combat.player_hits++;if(e->killed){combat.player_dead=60;combat.active=combat.pending=0;combat.player.invulnerable=255;}
}
void combat_drop(unsigned kind,unsigned quantity,unsigned lifetime,int x,int y){
 kind&=63;if(!kind)return;if(kind>=30)kind=10;
 if(combat.drop_count<COMBAT_CRATES)combat.drops[combat.drop_count++]=(struct CombatDrop){(uint16_t)kind,(uint16_t)quantity,(int16_t)x,(int16_t)y,(uint8_t)lifetime,0};
 else combat.fault=4;
}
int combat_spawn(void *user,const struct MissionSpawn*s){
 (void)user;
 if(s->template!=0x125)return 0;
 for(unsigned i=0;i<COMBAT_CRATES;i++)if(!combat.crates[i].active){
  struct CombatCrate*c=&combat.crates[i];
  *c=(struct CombatCrate){.x=s->x*256,.y=s->y*256,.active=1,.content=s->params[0],.quantity=s->params[1],.lifetime=s->params[2]};
  /* $09B3C6 and $2F772A: 16px half width, 24px height, filter $09A5FC.
   * HP is native-initialized to one; original ctor leaves the slot HP
   * untouched. This explicit initialization is a native safety adaptation. */
  c->hit=(struct HitActor){.id=combat.next_id++,.hp=1,.x=s->x,.y=(int16_t)(512-s->y)};
  animation_start(&c->anim,COMBAT_CRATE);return 1;
 }
 combat.fault=2;return 0;
}
static void damage(void*user,const struct HitDamage*event){
 (void)user;combat.hits++;if(event->killed)combat.kills++;
#ifdef NATIVE_MISSION
 actors_damage(0,event);
#endif
}
static unsigned attack(struct HitActor*a,const struct HitAttack*hit){
 unsigned result=0;
#ifdef NATIVE_MISSION
 struct HitActor*targets[COMBAT_CRATES+ACTOR_CAPACITY];
 const struct HitDefense*defenses[COMBAT_CRATES+ACTOR_CAPACITY];unsigned count=0;
 for(unsigned i=0;i<COMBAT_CRATES;i++){
  struct CombatCrate*c=&combat.crates[i];if(c->active&&!c->breaking&&!c->pending){targets[count]=&c->hit;defenses[count++]=&defense_crate;}
 }
 count+=actors_targets(targets+count,defenses+count,COMBAT_CRATES+ACTOR_CAPACITY-count);
 /* Stable spawn order across crates and NPCs. Pool type must not decide which
  * overlapping actor intercepts a bullet or receives a probe. */
 for(unsigned i=1;i<count;i++){struct HitActor*t=targets[i];const struct HitDefense*d=defenses[i];unsigned j=i;
  while(j&&targets[j-1]->id>t->id){targets[j]=targets[j-1];defenses[j]=defenses[j-1];j--;}
  targets[j]=t;defenses[j]=d;
 }
 for(unsigned i=0;i<count;i++){
  unsigned r=hit_pair(a,hit,targets[i],defenses[i],1,damage,0);if(r>result)result=r;if(a->flags&2)break;
 }
#else
 for(unsigned i=0;i<COMBAT_CRATES;i++){
  struct CombatCrate*c=&combat.crates[i];
  if(!c->active||c->breaking||c->pending)continue;
  unsigned r=hit_pair(a,hit,&c->hit,&defense_crate,1,damage,0);
  if(r>result)result=r;if(a->flags&2)break;
 }
#endif
 return result;
}
static void shoot(const struct Player*p){
 for(unsigned i=0;i<COMBAT_SHOTS;i++)if(!combat.shots[i].active){
  struct CombatShot*s=&combat.shots[i];int direction=p->facing?-1:1;
  combat.pistol_phase++;
  /* $03C710/$030AA4: muzzle is 28 source pixels above feet, odd shot
   * initially backs up 8px, then two 8px integrations per actor tick. */
  /* $03C7FC lowers the crouching muzzle by another 12 source pixels. */
  int up=combat.animation.sequence==COMBAT_UP_PISTOL;
  int muzzle=combat.animation.sequence==COMBAT_CROUCH_PISTOL?16:28;
  *s=(struct CombatShot){.x=p->x-((!up&&(combat.pistol_phase&1))?direction*8*256:0),.y=p->y-muzzle*256+((up&&(combat.pistol_phase&1))?8*256:0),
    .vx=(int16_t)(up?0:direction*0x800),.vy=(int16_t)(up?-0x800:0),.active=1};
  s->hit=(struct HitActor){.id=combat.next_id++};combat.shots_fired++;sound(0x10f7);return;
 }
 combat.fault=3;
}
void combat_step(const struct Player*p,unsigned keys,unsigned pressed){
#ifndef NATIVE_MISSION
 combat_begin_frame(p);
#endif
 for(unsigned i=0;i<COMBAT_CRATES;i++){
  struct CombatCrate*c=&combat.crates[i];if(!c->active)continue;
  if(c->pending){c->pending=0;c->breaking=1;animation_start(&c->anim,COMBAT_CRATE_BREAK);sound(0x102e);}
  /* Shared world collision adapter; the original $09B8C0 per-cell motion
   * routine is not yet translated. Stationary crates settle to the first
   * surface below their source spawn point. */
  if(!c->grounded&&!c->breaking){int floor=terrain_floor(c->x>>8,(c->y>>8)-1,(c->y>>8)+8);
   if(floor>=0){c->y=floor*256;c->grounded=1;}else c->y+=256;}
  c->hit.x=(int16_t)(c->x>>8);c->hit.y=(int16_t)(512-(c->y>>8));c->hit.flags&=(uint8_t)~8u;
  unsigned events=animation_step(&c->anim);
  if((events&(1u<<3))&&!c->drop_sent){
   c->drop_sent=1;
   /* $09B90A -> $09A7AA/$09A7CC: the two mirrored 16px X offsets
    * cancel; pickup starts four source pixels above the crate origin. */
   combat_drop(c->content,c->quantity,c->lifetime,c->hit.x,(c->y>>8)-4);
  }
  if(c->breaking&&c->anim.done)c->active=0;
 }
 if((keys&KEY_UP)&&p->grounded&&!combat.player_dead){
  if(!combat.aiming)animation_start(&combat.aim,COMBAT_AIM_UP_IDLE);
  combat.aiming=1;animation_step(&combat.aim);
 }else combat.aiming=0;
 if(combat.active&&combat.animation.done)combat.active=0;
 if(combat.pending){
  unsigned kind=combat.pending;combat.pending=0;combat.active=1;
  if(kind==2){combat.alternate^=1;combat.strikes++;animation_start(&combat.animation,combat.alternate?COMBAT_KNIFE_A:COMBAT_KNIFE_B);}
  else animation_start(&combat.animation,kind==4?COMBAT_UP_PISTOL:kind==3?COMBAT_CROUCH_PISTOL:COMBAT_PISTOL);
 }
 if(combat.active){
  unsigned events=animation_step(&combat.animation);
  if(events&(1u<<1)){hit_begin_attack(&combat.player);attack(&combat.player,&attack_knife);if(combat.player.events&4)sound(0x10fb);}
  if(events&(1u<<2))shoot(p);
 }
 /* $0330FA probe and pending next-tick action; fixture accepts button edges,
  * not the unported full player input/weapon cadence state machine. */
 if((pressed&KEY_A)&&!combat.active&&!combat.pending&&!combat.player_dead){
  hit_begin_attack(&combat.player);attack(&combat.player,&attack_probe);
  combat.pending=(keys&KEY_UP)?4:(combat.player.flags&2)?2:((keys&KEY_DOWN)&&p->grounded?3:1);
 }
 for(unsigned i=0;i<COMBAT_SHOTS;i++){
  struct CombatShot*s=&combat.shots[i];if(!s->active)continue;
  for(unsigned n=0;n<2;n++){
   s->x+=s->vx;s->y+=s->vy;
   if(terrain_solid(s->x>>8,s->y>>8,0)){s->active=0;break;}
  }
  if(!s->active)continue;
  s->hit.x=(int16_t)(s->x>>8);s->hit.y=(int16_t)(512-(s->y>>8));
  attack(&s->hit,&attack_pistol);
  if((s->hit.flags&2)||++s->age>=32)s->active=0;
 }
 for(unsigned i=0;i<COMBAT_CRATES;i++){
  struct CombatCrate*c=&combat.crates[i];if(c->active&&!c->breaking&&(c->hit.flags&1))c->pending=1;
 }
}
void combat_fixture_init(void){
 combat_reset();
 /* Declared test placements, not a purported M1 encounter or event route. */
 for(unsigned i=0;i<3;i++){
  /* Fixed fixture coordinates, checked against the imported terrain map. */
  int x=140+(int)i*140,y=328;
  struct MissionSpawn s={.template=0x125,.x=(int16_t)x,.y=(int16_t)y,.params={1,200,40,0,0,0,0,0}};
  combat_spawn(0,&s);
 }
}
