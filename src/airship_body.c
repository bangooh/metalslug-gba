#include "airship_internal.h"
#include "scene_runtime.h"
static int platform(struct WeaponWorld*w,struct WeaponActor*a,int special){
 struct TerrainMotion m={.platform_ids=a->platform_ids};
 if(!terrain_platform_publish(w->interactions->terrain,&m,as_word((a->transform.x>>8)-160),as_word(transform_y_up(&a->transform)+46),320,special))return enemy_fault(w);a->platform_ids=m.platform_ids;return 1;
}
static void scroll_output(struct WeaponWorld*w){struct AirshipSceneState*s=&w->enemies->airship_scene;w->scroll_seed_latch=s->scroll_mode;if(w->scene_runtime){struct SceneCamera*c=&w->scene_runtime->camera;c->seed_latch=s->scroll_mode;c->seed_x=s->scroll_x_q16;c->seed_y=s->scroll_y_q16;}}
static void camera_lock(struct WeaponWorld*w,struct WeaponActor*a){w->enemies->airship_scene.camera_lock_x=as_word(as_sx(w,a)+112);w->enemies->airship_scene.camera_lock_valid=1;}
static void polar(unsigned angle,int16_t speed,int16_t*x,int16_t*y){*x=as_word(((int32_t)boss_trig[1][angle&255]*speed)>>8);*y=as_word(((int32_t)boss_trig[0][angle&255]*speed)>>8);}
static int follow(struct WeaponWorld*w,struct NativeEntity*e,int dy,int priority){struct EnemyActor*p=as_parent(w,e);if(!p)return 0;struct WeaponActor*a=&e->payload.enemy.actor;transform_integer_copy(&a->transform,&p->actor.transform);transform_integer_offset(&a->transform,0,(int16_t)dy);if(priority)a->transform.priority=p->actor.transform.priority;return 1;}
static int probe(struct WeaponWorld*w,struct WeaponActor*a,struct WeaponContact*h){return terrain_weapon_point_sample(w->interactions->terrain,a,as_word((a->transform.x>>8)+168),as_word(transform_y_up(&a->transform)+16),h)?1:enemy_fault(w);}
static int hover(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;q->airship.angle=(q->airship.angle-2)&255;int16_t vx,vy;polar(q->airship.angle,62,&vx,&vy);vx=as_word(vx*2);
 int16_t d=as_word(q->airship.target_x-as_sx(w,a));if(d)vx=as_word(vx+(d<0?-18:18));d=as_word(q->airship.target_y-as_sy(w,a));if(d)vy=as_word(vy+(d<0?-4:4));a->vx=vx;a->vy=vy;
 struct WeaponContact h;if(!probe(w,a,&h))return 0;
 if(h.solid&&(h.raw==0x25||h.raw==0x1e)){
  if(h.raw==0x1e)as_schedule(e,AS_BODY_DEPART);a->hit.flags&=191;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(a->vx>0)a->vx=0;
 }
 if(interaction_move(w,a,WMOVE_POINT)<0)return 0;
 if(q->airship.recoil){q->airship.recoil--;vx=a->vx;vy=a->vy;a->vy=w->frame_phase&1?512:-512;int r=interaction_move(w,a,WMOVE_POINT);a->vx=vx;a->vy=vy;if(r<0)return 0;}
 if(!probe(w,a,&h))return 0;int struck=h.solid&&h.raw==0x25&&!q->airship.ground_seen;q->airship.ground_seen=h.shape;
 if(struck){if(!interaction_sound(w,0x1081))return 0;q->airship.recoil=10;if(w->secondary_x_offset<2464&&!as_child(w,e,AS_IMPACT,ENTITY_FIRST_CHILD,184,80))return 0;}return !w->fault;
}
static int departure(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;int16_t vx=0,vy=0;
 if(q->airship.speed>=0){a->vx=0;a->vy=as_word((q->airship.speed>>5)*(w->frame_phase&1?-256:256));if(interaction_move(w,a,WMOVE_POINT)<0)return 0;q->airship.speed=as_word(q->airship.speed-3);polar(31,q->airship.speed,&vx,&vy);}
 a->vx=vx;a->vy=vy;return interaction_move(w,a,WMOVE_POINT)>=0;
}
static int gun_register(struct WeaponWorld*w,struct EnemyActor*q){
 if(!w->interactions->targets)return enemy_fault(w);int8_t owner=q->airship.owner;struct WeaponActor*a=&q->actor;
 target_ring_register(w->interactions->targets,as_sx(w,a),as_word(a->transform.x>>8),transform_y_up(&a->transform),q->airship.key,&owner);q->airship.owner=owner;return 1;
}
static int settle(struct WeaponWorld*w,struct WeaponActor*a){
 as_screen(w,a,as_sx(w,a),as_sy(w,a)&1023);
 for(unsigned budget=0;budget<2048;budget++){if(as_sy(w,a)<0){as_screen(w,a,160,384);return 0;}int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return -1;if(r)return 1;}enemy_fault(w);return -1;
}
int as_body_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;struct EnemyActor*p;unsigned s=a->state;
 switch(s){
 case AS_BODY:as_screen(w,a,w->scene_number==12?56:104,w->scene_number==12?272:184);break;
 case AS_BODY_INIT:{
  a->palette=0xcf;q->airship.margin=160;q->airship.recoil=0;q->airship.ground_seen=0;a->transform.facing=0;a->phase=0;a->transform.priority=0;as_reset(a);a->vy=160;as_start(q,ASA_BODY);
  if(!as_child(w,e,AS_BODY_CHILD,ENTITY_FIRST_CHILD,0,0))return 0;
  if(w->scene_number!=12){if(!as_child(w,e,AS_GROUND,ENTITY_FIRST_CHILD,0,0))return 0;for(unsigned i=0;i<2;i++){n=as_child(w,e,AS_PLAYER_TRACK,ENTITY_FIRST_CHILD,0,0);if(!n)return 0;n->payload.enemy.airship.variant=(uint8_t)i;}}
  struct PlayerTransform local={.y=(512-64)*256};if(!gun_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&local,1))return 0;
  if(!interaction_sound(w,0x1080)||!interaction_sound(w,0x107f))return 0;
  if(w->scene_number==12){as_schedule(e,AS_BODY_LANDED);return 2;}a->state=AS_BODY_ENTER;break;
 }
 case AS_BODY_LANDED:
  n=as_child(w,e,AS_CROUCH_PAIR,ENTITY_FIRST_CHILD,158,64);if(!n)return 0;n->payload.enemy.params[0]=1;n->payload.enemy.airship.variant=0;
  if(w->scene_number!=12){w->enemies->siege_phase=0;w->enemies->airship_scene.scroll_mode=65535;w->enemies->airship_scene.scroll_x_q16=0x8000;w->enemies->airship_scene.scroll_y_q16=0;w->enemies->airship_scene.scroll_valid=1;scroll_output(w);}
  as_reset(a);q->airship.angle=64;q->airship.speed=62;q->airship.target_x=104;q->airship.target_y=280;as_attack(q,ASB_BODY);hit_begin_attack(&a->hit);a->phase=255;a->state=AS_BODY_HOVER;break;
 case AS_BODY_LOWER:q->airship.target_y=256;break;
 case AS_BODY_DEPART:
  if(!interaction_sound(w,0x1026))return 0;if(w->scene_number!=12){w->enemies->airship_scene.scroll_mode=0;w->enemies->airship_scene.scroll_valid=1;scroll_output(w);}q->airship.speed=352;as_attack(q,65535);hit_begin_attack(&a->hit);break;
 case AS_BODY_CHILD:a->palette=0xcf;a->transform.facing=0;as_start(q,ASA_BODY_CHILD);break;
 case AS_GROUND:a->palette=0xd0;a->transform.facing=0;as_screen(w,a,as_sx(w,a),288);a->display_flags|=64;a->transform.priority=0xf000;as_start(q,ASA_GROUND);if(!as_child(w,e,AS_GROUND_CHILD,ENTITY_FIRST_CHILD,0,0))return 0;break;
 case AS_GROUND_CHILD:a->palette=0xd0;a->transform.facing=0;a->display_flags|=64;a->transform.priority=0xf000;as_start(q,ASA_GROUND_CHILD);break;
 case AS_PLAYER_TRACK:if(q->airship.variant>1)return enemy_fault(w);if(!enemy_player_live(w->players[q->airship.variant])){entity_delete(w->entities,e);return 0;}break;
 case AS_GUN:a->palette=0xd1;a->transform.facing=0;q->airship.angle=2;q->airship.direction=1;q->airship.clock=15;q->airship.owner=-1;if(!w->interactions->targets)return enemy_fault(w);q->airship.key=target_ring_new_key(w->interactions->targets);a->state=AS_GUN_IDLE;/* FALLTHROUGH */
 case AS_GUN_IDLE:as_start_entry(q,airship_gun_idle[q->airship.angle&7]);break;
 case AS_GUN_ACTIVE:break;
 case AS_GUN_COOLDOWN:q->airship.signal=255;q->airship.timer=10;break;
 case AS_OVERLAY:a->palette=q->dead==0?0x134:q->dead==1?0x152:0xe;a->transform.facing=0;p=as_parent(w,e);if(!p)return 0;as_start_entry(q,airship_overlay[p->airship.angle&7]);break;
 case AS_CASING:{a->palette=0xd2;a->transform.facing=0;a->transform.priority=0xc000;as_start(q,ASA_CASING);as_reset(a);uint16_t r=as_random(w)&255;a->vy=as_word(896+r);a->ay=-64;a->vx=as_word(64+r);break;}
 case AS_CASING_REST:case AS_CASING_FREEZE:break;
 case AS_CROUCH_PAIR:
  for(unsigned i=0;i<2;i++){n=as_child(w,e,AS_CROUCH,ENTITY_FIRST_CHILD,0,0);if(!n)return 0;n->payload.enemy.airship.variant=(uint8_t)i;n->payload.enemy.airship.direction=q->airship.variant;n->payload.enemy.params[0]=q->params[0];}entity_delete(w->entities,e);return 0;
 case AS_CROUCH:
  a->palette=q->airship.direction?1:2;a->transform.facing=(a->transform.facing&254)|(q->airship.direction&1);as_defense(q,ASB_CROUCH);as_start(q,ASA_CROUCH);as_reset(a);q->airship.signal=0;
  if(q->params[0]==1){int r=settle(w,a);if(r<0)return 0;if(!r){transform_integer_offset(&a->transform,0,3);a->vy=-1024;q->airship.signal=1;}}break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int wait_for_camera(struct WeaponWorld*w,struct NativeEntity*e){
 /* Source $0893E4 publishes the unchanged screen-space anchor while
  * the camera descends. Native world coordinates must follow that camera
  * even before movement/animation starts at source Y=640. */
 if(!scene_preserve_screen(w,&e->payload.enemy.actor)||!platform(w,&e->payload.enemy.actor,0))return 0;
 if(w->secondary_y_offset>=640){as_schedule(e,AS_BODY_INIT);return 2;}return 1;
}
int as_body_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;struct NativeEntity*n;
 if(s>=AS_BODY&&s<=AS_BODY_DEPART){
  if(!w->secondary_view_valid)return enemy_fault(w);
  if(s==AS_BODY)return wait_for_camera(w,e);
  if(s==AS_BODY_ENTER){w->enemies->intro_gate=0;if(!scene_preserve_screen(w,a))return 0;if(interaction_move(w,a,WMOVE_POINT)<0||!platform(w,a,0))return 0;camera_lock(w,a);as_animate(w,e);if(as_sy(w,a)>=280)as_schedule(e,AS_BODY_LANDED);return !w->fault;}
  if(s==AS_BODY_DEPART){if(interaction_move(w,a,WMOVE_SYNC)<0||!departure(w,q)||!platform(w,a,w->scene_number!=12))return 0;as_animate(w,e);if((uint16_t)q->airship.speed<=3&&!interaction_sound(w,0x23))return 0;if(as_cull(w,e)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);}return !w->fault;}
  if(!scene_preserve_screen(w,a))return 0;if(!hover(w,e)||!platform(w,a,0))return 0;camera_lock(w,a);as_animate(w,e);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;
  /* The late X threshold overwrites an earlier departure request, as source. */
  if(s==AS_BODY_HOVER&&w->secondary_x_offset>=1664)as_schedule(e,AS_BODY_LOWER);return !w->fault;
 }
 if(s==AS_BODY_CHILD||s==AS_GROUND_CHILD){if(!follow(w,e,0,1))return 0;if(as_sx(w,a)>=0)as_animate(w,e);return !w->fault;}
 if(s==AS_GROUND){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;p=as_parent(w,e);if(!p)return 0;transform_integer_offset(&a->transform,as_word((p->actor.transform.x>>8)-10-(a->transform.x>>8)),0);as_animate(w,e);return !w->fault;}
 if(s==AS_PLAYER_TRACK){
  if(!w->secondary_view_valid)return enemy_fault(w);if(w->secondary_x_offset>1656){entity_delete(w->entities,e);return 1;}struct PlayerBodyState*pl=w->players[q->airship.variant];if(!pl)return enemy_fault(w);
  if(w->secondary_y_offset<640||as_word(transform_y_up(&pl->actor.transform)+w->camera_y)>288)return 1;
  if(!interaction_sound(w,0x1053))return 0;
  for(unsigned i=0;i<3;i++){n=as_child(w,e,i<2?AS_SPLASH:AS_SPARK_WAIT,ENTITY_FIRST_CHILD,0,0);if(!n)return 0;transform_integer_copy(&n->payload.enemy.actor.transform,&pl->actor.transform);transform_integer_offset(&n->payload.enemy.actor.transform,i==0?-24:i==1?24:0,0);n->payload.enemy.actor.transform.priority=0xc000;n->payload.enemy.airship.timer=0;}entity_delete(w->entities,e);return !w->fault;
 }
 if(s==AS_GUN_IDLE||s==AS_GUN_ACTIVE||s==AS_GUN_COOLDOWN){
  if(!follow(w,e,64,0))return 0;
  if(s==AS_GUN_IDLE){q->airship.signal=0;as_animate(w,e);p=as_parent(w,e);if(!p)return 0;
   if(p->actor.phase&&as_sx(w,a)>=-16){if(!gun_register(w,q))return 0;if(q->airship.owner>=0){q->dead=(uint8_t)q->airship.owner;n=as_child(w,e,AS_OVERLAY,ENTITY_LAST_CHILD,0,0);if(!n)return 0;n->payload.enemy.dead=q->dead;as_schedule(e,AS_GUN_ACTIVE);}}return !w->fault;
  }
  if(s==AS_GUN_COOLDOWN){as_animate(w,e);if(!--q->airship.timer)as_schedule(e,AS_GUN_IDLE);return !w->fault;}
  q->airship.signal=1;if(!--q->airship.clock){q->airship.angle=as_word(q->airship.angle+q->airship.direction);if(q->airship.direction>=0&&q->airship.angle>3)q->airship.direction=-1;else if(q->airship.direction<0&&q->airship.angle<2)q->airship.direction=1;q->airship.angle&=7;q->airship.clock=15;q->airship.signal=0x88;as_start_entry(q,airship_gun_active[q->airship.angle]);}
  as_animate(w,e);struct PlayerBodyState*pl=w->players[q->dead?1:0];if(!pl)return enemy_fault(w);
  if(pl->actor.hit.flags&1){as_schedule(e,AS_GUN_COOLDOWN);return !w->fault;}
  if(!(w->frame_phase&7)){
   if(!(w->frame_phase&8)&&!as_child(w,e,AS_CASING,ENTITY_LAST_CHILD,8,32))return 0;
   if(!interaction_sound(w,0x108c))return 0;struct PlayerTransform t=a->transform;transform_integer_offset(&t,airship_muzzle[q->airship.angle][0],airship_muzzle[q->airship.angle][1]);if(!weapon_emit_gun_bullet_at(w,entity_ref(e),&t,(uint8_t)(q->airship.angle<<4)))return 0;
  }
  if(as_sx(w,a)>=-16){if(!gun_register(w,q))return 0;if(q->airship.owner>=0)return !w->fault;}
  q->airship.signal=255;as_schedule(e,AS_GUN_IDLE);return !w->fault;
 }
 if(s==AS_OVERLAY){if(!follow(w,e,0,0))return 0;p=as_parent(w,e);if(!p)return 0;if(p->airship.signal==255){entity_delete(w->entities,e);return 1;}if(p->airship.signal==0x88)as_start_entry(q,airship_overlay[p->airship.angle&7]);as_animate(w,e);return !w->fault;}
 if(s==AS_CASING||s==AS_CASING_REST||s==AS_CASING_FREEZE){
  int ground=interaction_move(w,a,s==AS_CASING_FREEZE?WMOVE_SYNC:WMOVE_POINT);if(ground<0)return 0;if(s==AS_CASING_FREEZE)a->animation.freeze=127;as_animate(w,e);
  if(s==AS_CASING&&a->vy<0&&ground){if(!(as_random(w)&15)){a->vx=as_word(-a->vx);a->transform.facing^=1;}a->vy=as_word(-a->vy)>>1;if(a->vy<=128){as_schedule(e,AS_CASING_REST);if(!(as_random(w)&127))as_schedule(e,AS_CASING_FREEZE);}}
  if(interaction_outside(w,a,airship_bounds[1],1))entity_delete(w->entities,e);return !w->fault;
 }
 if(s==AS_CROUCH){
  if(q->airship.signal){int r=interaction_move(w,a,WMOVE_BODY);if(r<0)return 0;if(r)q->airship.signal=0;}
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;as_animate(w,e);if(interaction_outside(w,a,airship_bounds[7],0)){entity_delete(w->entities,e);return 1;}
  struct PlayerBodyState*pl=w->players[q->airship.variant];if(pl&&!((pl->actor.transform.facing^a->transform.facing)&1)){
   int16_t dx=as_word((pl->actor.transform.x>>8)-(a->transform.x>>8)),dy=as_word(transform_y_up(&pl->actor.transform)-transform_y_up(&a->transform));if(a->transform.facing&1)dx=as_word(-dx);if(a->transform.facing&2)dy=as_word(-dy);
   if(dx>=airship_crouch_rect[0]&&dx<=airship_crouch_rect[1]&&dy>=airship_crouch_rect[2]&&dy<=airship_crouch_rect[3])pl->crouch_flags|=1;
  }return !w->fault;
 }
 return enemy_fault(w);
}
