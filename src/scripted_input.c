#include "tank_internal.h"
#include "player_air.h"
/* $079298 and its four child handlers. The child resets are observable by
 * later tasks; input selection must occur at the point of use, not once at
 * the beginning of a frame. siege_phase is the original shared $10E39C. */
enum State {DRIVER,ALIGN,RUN,HOLD,DEPART,STOP};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
struct PlayerSourceInput scripted_input_select(const struct EnemyWorld*w,unsigned slot,struct PlayerSourceInput physical){if(w&&w->scripted_input.active&&slot<2)return (struct PlayerSourceInput){w->scripted_input.player[slot].held,w->scripted_input.player[slot].actions};return physical;}
unsigned scripted_input_layout(const struct EnemyWorld*w,unsigned slot,unsigned physical){return w&&w->scripted_input.active&&slot<2?w->scripted_input.player[slot].layout:physical;}
static void clear(struct EnemyWorld*w){for(unsigned i=0;i<2;i++){w->scripted_input.player[i].previous=0;w->scripted_input.player[i].held=0;w->scripted_input.player[i].actions=0;w->scripted_input.player[i].latch=0;}}
static int stop(struct WeaponWorld*w,struct NativeEntity*e){w->enemies->scripted_input.active=0;w->enemies->siege_phase=0;entity_delete(w->entities,e);return 1;}
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned state){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(e)enemy_schedule(&e->payload.enemy,state);return e;}
static int run_probe(struct WeaponWorld*w,struct NativeEntity*e,unsigned slot){struct EnemyActor*q=&e->payload.enemy;struct PlayerBodyState*p=w->players[slot];if(q->scripted_input.triggered[slot]||!enemy_player_live(p))return 1;
 /* $02A8C0's player branch uses the exact foot probe; ordinary players
  * initialize type bit6. Its vehicle-contact branch has no player entry. */
 if(!(p->actor.hit.type_flags&64)||!w->interactions||!w->interactions->terrain)return enemy_fault(w);
 struct PlayerActionContext action={.body=p};struct PlayerAirContext air={.actions=&action,.terrain=w->interactions->terrain};int grounded=player_body_foot_contact(&air);if(grounded<0)return enemy_fault(w);if(!grounded)return 1;
 /* Source restores A6 to the script before $027DB2, so this second probe
  * uses the SCRIPT actor's terrain flags at the PLAYER's offset position. */
 struct TerrainMotion m={.transform=&q->actor.transform,.hit=&q->actor.hit,.collision_flags=q->lifecycle_flags,.platform_ids=q->actor.platform_ids};struct TerrainSample sample;
 if(!terrain_lookup(w->interactions->terrain,&m,tk_word((p->actor.transform.x>>8)+24),tk_word(transform_y_up(&p->actor.transform)-8),&sample))return enemy_fault(w);
 if(sample.raw!=64){q->scripted_input.triggered[slot]=1;w->enemies->scripted_input.player[slot].held|=1;w->enemies->scripted_input.player[slot].actions|=1;w->enemies->scripted_input.player[slot].latch|=1;}return 1;
}
static int driver(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyWorld*v=w->enemies;unsigned mode=v->siege_phase;
 if(!mode)v->scripted_input.active=0;
 else if(mode>=5)return enemy_fault(w);
 else if(mode!=v->scripted_input.active){v->scripted_input.active=mode;for(unsigned i=0;i<2;i++)v->scripted_input.player[i].layout=v->scripted_input.base_layout;clear(v);v->scripted_input.system_freeze=2;
  struct PlayerTransform t={0};if(!allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,mode))return 0;
 }
 if(v->siege_phase)for(unsigned i=0;i<2;i++)if(enemy_player_live(w->players[i]))w->players[i]->actor.hit.invulnerable=2;return !w->fault;
}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){(void)pool;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct EnemyWorld*v=w->enemies;
 if(q->entry){q->entry=0;switch(q->actor.state){
 case DRIVER:clear(v);v->siege_phase=v->scripted_input.active=0;break;
 case ALIGN:break;
 case RUN:q->scripted_input.triggered[0]=q->scripted_input.triggered[1]=0;q->scripted_input.timer=300;q->scripted_input.delay=20;break;
 case HOLD:clear(v);break;
 case DEPART:q->scripted_input.timer=20;clear(v);break;
 case STOP:stop(w,e);return;
 default:enemy_fault(w);return;
 }}
 unsigned s=q->actor.state;if(s==DRIVER){driver(w,e);return;}
 if(s==ALIGN)clear(v);
 if(v->scripted_input.active!=s||!v->siege_phase){stop(w,e);return;}
 if(s==HOLD)return;
 if(s!=ALIGN)clear(v);
 if(s==ALIGN||s==DEPART){for(unsigned i=0;i<2;i++)if(enemy_player_live(w->players[i])){int16_t x=tk_word((w->players[i]->actor.transform.x>>8)-w->camera_x);if(s==DEPART){if(x<=192)v->scripted_input.player[i].held|=8;}else if(x<(i?64:120))v->scripted_input.player[i].held|=8;else if(x>(i?80:136))v->scripted_input.player[i].held|=4;}
  if(s==DEPART){if(w->scroll_seed_latch){q->scripted_input.timer=tk_word(q->scripted_input.timer-1);if(q->scripted_input.timer<=0)enemy_schedule(q,STOP);}}return;
 }
 if(s==RUN){if(q->scripted_input.delay>0){q->scripted_input.delay=tk_word(q->scripted_input.delay-1);v->scripted_input.player[0].held|=4;v->scripted_input.player[1].held|=4;}else{v->scripted_input.player[0].held|=8;v->scripted_input.player[1].held|=8;if(!run_probe(w,e,0)||!run_probe(w,e,1))return;}return;}
 enemy_fault(w);
}
int scripted_input_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t){return allocate(w,p,o,t,DRIVER)!=0;}
