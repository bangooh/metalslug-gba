#include "tank_internal.h"
#include "m4_internal.h"
#include "items.h"
#include "../generated/banner-programs.h"
/* $046322..$046608. Most original 'Drop' labels are static drawing actors. */
enum State {NEW,STATIC,THROWN,GRENADE_BODY,GRENADE_FALL,CONTINUE_COUNT,DELETE,DELETE_TREE,WHITE_FLASH,WHITE_DONE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){enemy_schedule(&e->payload.enemy,s);}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static void face(struct EnemyActor*q){q->actor.transform.facing=q->params[1]&1;}
static void scale(struct EnemyActor*q){q->scale_x=q->scale_y=q->params[3]?q->params[3]:255;}
static void palette(struct EnemyActor*q){q->actor.palette=banner_table_palettes[(q->params[0]&3)*4+(q->params[2]&3)];}
static int smoke(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct PlayerTransform t=q->actor.transform;t.display_group=q->source_flags;struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,BFX_DAMAGE_SMOKE_BODY);if(!n)return 0;n->payload.enemy.actor.attached_offset=0;n->payload.enemy.params[0]=n->payload.enemy.params[1]=0;return 1;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(a->state){
 case NEW:
  if(q->family==230){unsigned n=q->params[0];if(!n){q->params[1]=random_word(w)&1;n=(random_word(w)&7)+1;}n=(uint8_t)(n-1)&15;q->params[0]=n;a->palette=banner_random_palettes[q->params[2]&3][n];animation_drawing(&a->animation,&banner_program,banner_random_drawings[n]);}
  else if(q->family>=231&&q->family<=242){unsigned n=q->family-231;animation_drawing(&a->animation,&banner_program,banner_fixed_drawings[n]);
   if(q->family==240||q->family==241){if(q->params[4]&&!smoke(w,e))return 0;scale(q);palette(q);}
   else {static const uint16_t pals[]={0xf2,0x1b,0x1b,0x6d,0x2c,0x2b,0x2b,0x2b,0x3a,0,0,0x4f};a->palette=pals[n];if(q->family==239&&q->params[0])a->palette=0x12f;}}
  else return enemy_fault(w);
  face(q);a->state=STATIC;break;
 case THROWN:a->vx=tk_word((int8_t)q->params[1]*16);if(q->params[1]&128)a->transform.facing=1;palette(q);scale(q);interaction_priority(w,a,0x8000,16);animation_begin(&a->animation,BANNER_THROWN);a->movement_flags|=32;break;
 case GRENADE_BODY:case GRENADE_FALL:w->grenade_body_collision=a->state==GRENADE_BODY?255:0;entity_delete(w->entities,e);return 0;
 case CONTINUE_COUNT:w->enemies->continue_count=q->params[0]==255?0:q->params[0];entity_delete(w->entities,e);return 0;
 case WHITE_FLASH:if(!scene_rgb_fade_spawn(w,entity_ref(e),0,31,31,16))return 0;w->enemies->palette_shutdown=255;a->timer=32;break;
 case WHITE_DONE:break;
 case DELETE_TREE:entity_delete_children(w->entities,e);/* fall through */
 case DELETE:entity_delete(w->entities,e);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(q->entry){q->entry=0;if(!enter(w,e))return;}if(e->deleting)return;
 if(a->state==WHITE_FLASH){a->timer--;if((int16_t)a->timer<=0){if(!scene_rgb_fade_spawn(w,entity_ref(e),31,31,31,16))return;a->timer=16;schedule(e,WHITE_DONE);}}
 else if(a->state==WHITE_DONE){a->timer--;if((int16_t)a->timer<=0){struct NativeEntity*n=entity_get(w->entities,e->parent);if(!n){enemy_fault(w);return;}if(n->kind==ENTITY_GROUP)n->payload.group.command=0;else if(n->kind==ENTITY_ENEMY)n->payload.enemy.dead=0;else{enemy_fault(w);return;}entity_delete(w->entities,e);}}
 else if(a->state==STATIC){if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(!animation_output_direct(&a->animation,AN_OUTPUT_PROJECTED)){enemy_fault(w);return;}if(interaction_outside(w,a,banner_bounds[0],0))schedule(e,DELETE);}
 else if(a->state==THROWN){if(interaction_move(w,a,WMOVE_POINT)<0)return;animation_step(&a->animation,&banner_program,0);if(a->animation.fault){enemy_fault(w);return;}if(interaction_outside(w,a,banner_bounds[1],0))schedule(e,DELETE_TREE);}
 else enemy_fault(w);
}
int banner_spawn_template(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){unsigned s;if(id>=230&&id<=242)s=NEW;else if(id==243)s=THROWN;else if(id==321)s=GRENADE_BODY;else if(id==322)s=GRENADE_FALL;else if(id==327)s=CONTINUE_COUNT;else return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(!e)return 0;struct EnemyActor*q=&e->payload.enemy;q->family=(uint8_t)id;if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];schedule(e,s);return 1;}
/* $0463C2 uses $09A7AA: its two opposite X offsets cancel, then adds Y+4. */
int banner_drop_item(struct WeaponWorld*w,const struct NativeEntity*e){if(!e||e->kind!=ENTITY_ENEMY)return enemy_fault(w);const struct EnemyActor*q=&e->payload.enemy;return item_respawn_below(w,&q->actor.transform,q->params[2],q->params[3],q->params[4]);}
/* Both $046518 and $0465EC compare depth with the NEXT task, not its parent. */
int banner_has_children(struct WeaponWorld*w,const struct NativeEntity*e){return m4_has_children(w,e);}

/*46608: source-only alias; the original outer cutscene caller is still pending. */
int banner_white_flash(struct WeaponWorld*w,struct EntityRef p){struct PlayerTransform t={.x=-256,.y=513*256,.priority=65535,.display_group=255};struct NativeEntity*e=enemy_allocate(w,p,ENTITY_FIRST_CHILD,&t,tick);if(!e)return 0;schedule(e,WHITE_FLASH);return 1;}
