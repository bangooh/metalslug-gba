#include "script151_internal.h"
/* Children of $075100. Source integer coordinates remain separate from the
 * subpixel accumulators, including before the first SYNC of each projectile. */
static void position(struct WeaponWorld*w,struct WeaponActor*a,int16_t x,int16_t y){transform_integer_offset(&a->transform,tk_word(x-tk_sx(w,a)),tk_word(y-tk_sy(w,a)));}
static int follow(struct WeaponWorld*w,struct NativeEntity*e,int dx,int dy){struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;position(w,&e->payload.enemy.actor,tk_word(tk_sx(w,&p->actor)+dx),tk_word(tk_sy(w,&p->actor)+dy));return 1;}
static void palettes(struct EnemyActor*q){for(unsigned i=0;i<3;i++)q->script151.palettes[i]=0x1e1+i;q->actor.palette=q->script151.palettes[0];}
static void draw(struct WeaponActor*a,uint16_t n){animation_drawing(&a->animation,&script151_program,n);a->movement_flags|=32;}
static int delete_tree(struct WeaponWorld*w,struct NativeEntity*e){entity_delete_children(w->entities,e);entity_delete(w->entities,e);return !w->fault;}
int s151_effect_callback(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(n==S151C_7650E||n==S151C_76536){struct NativeEntity*c=s151_child(w,e,n==S151C_7650E?S151_FLAME:S151_SMOKE);if(!c)return 0;if(n==S151C_76536)position(w,&c->payload.enemy.actor,tk_sx(w,a),tk_word(tk_sy(w,a)-64));s151_attack(q,n==S151C_7650E?S151B_FLAME_PULSE:S151B_SMOKE_PULSE);hit_begin_attack(&a->hit);int r=enemy_attack(w,a);s151_attack(q,65535);return r;}
 if(n==S151C_75EE8){
  /* The original changes A6 to $100800 before reading its parent. This
   * context must be supplied by that scene owner, never by the wall HP. */
  if(!w->enemies->script151_projectile_context.valid)return enemy_fault(w);
  struct PlayerTransform t={.display_group=255};struct NativeEntity*c=s151_allocate(w,w->projectile_group,ENTITY_FIRST_CHILD,&t,S151_FRAGMENT);if(!c)return 0;
  c->payload.enemy.script151.resume=w->enemies->script151_projectile_context.cutoff>=w->enemies->script151_projectile_context.hp?1:0;
  position(w,&c->payload.enemy.actor,tk_word(tk_sx(w,a)-8),tk_word(tk_sy(w,a)+q->script151.offset_y));return 1;
 }
 if(n==S151C_76DD4){
  /* Preserve the projectile colour cycle and attack trajectory. Only the
   * separate, non-colliding visual trail child is emitted one in three. */
  unsigned i=(q->script151.variant+1)&3;if(i>=3)i=0;
  if(!i){struct NativeEntity*c=s151_allocate(w,entity_ref(e),ENTITY_LAST_CHILD,&a->transform,S151_TRAIL);if(!c)return 0;c->payload.enemy.actor.transform.priority--;}
  q->script151.variant=i;a->palette=q->script151.palettes[i];return 1;
 }
 return enemy_fault(w);
}
static int fragment(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(!tk_sound(w,0x10ea)||!w->rng)return enemy_fault(w);a->hit.type_flags|=16;palettes(q);
 if(q->script151.resume>1)return enemy_fault(w);int n=enemy_nearest_x(w,a,&script151_target[q->script151.resume]);int16_t dx,dy;
 if(n<0){dx=source_random(w->rng,256);dy=source_random(w->rng,16)+16;}
 else{dx=tk_word((w->players[n]->actor.transform.x>>8)-(a->transform.x>>8));dy=tk_word(transform_y_up(&w->players[n]->actor.transform)+16-transform_y_up(&a->transform));if(tk_random(w)&3){dx=tk_word(dx+(tk_random(w)&127)-63);dy=tk_word(dy+(tk_random(w)&15));}if(dy<0)dy=0;}
 uint16_t time=(uint16_t)s151_rank(w,q,S151R_FRAGMENT_TIME);if(time<2)time=2;uint16_t reciprocal=65536u/time;
 a->vx=tk_word(((int32_t)dx*(int16_t)reciprocal)>>8);
 uint16_t scaled=(512u*reciprocal)>>8;a->ay=tk_word(-((scaled*(uint32_t)reciprocal)>>16));
 a->vy=tk_word(((uint16_t)(dy+256)*(uint32_t)reciprocal)>>8);
 interaction_priority(w,a,0xd000,20);s151_start(q,S151A_FRAGMENT);return !w->fault;
}
int s151_effects_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(a->state){
 case S151_FLAG:return 1;
 case S151_FLAME:a->palette=0x156;a->transform.priority=0;a->phase=0;s151_start(q,S151A_FLAME);s151_attack(q,S151B_FLAME);hit_begin_attack(&a->hit);break;
 case S151_SMOKE:s151_start(q,S151A_SMOKE);a->palette=0x55;a->display_flags|=64;s151_attack(q,S151B_SMOKE);hit_begin_attack(&a->hit);a->vx=tk_word(-s151_rank(w,q,S151R_SMOKE_SPEED));a->vy=0;break;
 case S151_WINDOW:a->transform.priority=0xd000;a->palette=0x51;s151_start(q,S151A_WINDOW);return 0;
 case S151_WINDOW_FALL:s151_start(q,S151A_WINDOW_FALL);a->phase=0;a->vx=a->vy=0;break;
 case S151_ROOF_A:case S151_ROOF_B:a->palette=0x50;s151_start(q,S151A_ROOF);a->hit.hp=160;break;
 case S151_ROOF_A_WRECK:s151_start(q,S151A_ROOF_WRECK);if(!s151_patch(w,0))return 0;break;
 case S151_ROOF_B_WRECK:s151_start(q,S151A_ROOF_WRECK);for(unsigned i=1;i<=4;i++)if(!s151_patch(w,i))return 0;break;
 case S151_ROOF_B_DRAIN:break;
 case S151_GATE:q->script151.palettes[0]=0x52;q->script151.palettes[1]=0x167;a->palette=0x52;a->transform.priority=0;draw(a,script151_raw_draw[0]);return 0;
 case S151_BASE:q->script151.palettes[0]=0xa0;q->script151.palettes[1]=0x168;a->palette=0xa0;a->transform.priority=0x2000;draw(a,script151_raw_draw[1]);if(!s151_child(w,e,S151_CREW))return 0;break;
 case S151_CREW:a->palette=0xe;a->transform.priority=0;q->script151.variant=4;s151_start(q,S151A_CREW);break;
 case S151_NUKE:s151_attack(q,S151B_NUKE);hit_begin_attack(&a->hit);position(w,a,0,256);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;w->enemies->clear_ordnance=1;s151_schedule(e,S151_NUKE_END);return 0;
 case S151_NUKE_END:w->enemies->clear_ordnance=0;s151_attack(q,65535);hit_begin_attack(&a->hit);entity_delete(w->entities,e);return 0;
 case S151_FRAGMENT:return fragment(w,e);
 case S151_TRAIL:palettes(q);s151_start(q,S151A_TRAIL);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
int s151_effects_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p=0;
 if(s!=S151_SMOKE&&s!=S151_WINDOW_FALL&&s!=S151_FRAGMENT){p=tk_parent(w,e);if(!p)return 0;}
 switch(s){
 case S151_FLAG:if(p->actor.hit.flags&1)return delete_tree(w,e);if(!follow(w,e,0,p->script151.offset_y))return 0;a->transform.priority=p->actor.transform.priority;draw(a,p->script151.overlay);if(q->script151.palette!=p->script151.palette)q->script151.palette=a->palette=p->script151.palette;if(!animation_output_direct(&a->animation,AN_OUTPUT_DIRECT))return enemy_fault(w);break;
 case S151_FLAME:if(!follow(w,e,-32,-24))return 0;if(p->actor.hit.flags&1){entity_delete(w->entities,e);break;}if(s151_animate(w,e)){entity_delete(w->entities,e);break;}if(a->phase){a->phase=0;s151_start(p,S151A_FLAME_RETURN);}break;
 case S151_SMOKE:if(interaction_move(w,a,WMOVE_POINT)<0||!enemy_attack(w,a))return 0;if((uint16_t)tk_word(tk_sx(w,a)+48)>=416){entity_delete(w->entities,e);break;}s151_animate(w,e);break;
 case S151_WINDOW:if(p->script151.flags[S151_SIG_OPEN])s151_schedule(e,S151_WINDOW_FALL);if(!s151_anchor(w,a,4064,160))return 0;s151_animate(w,e);break;
 case S151_WINDOW_FALL:if(a->phase)a->vx=tk_word(a->vx+64);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;s151_animate(w,e);if((uint16_t)tk_sx(w,a)>=384)entity_delete(w->entities,e);break;
 case S151_ROOF_A:case S151_ROOF_B:{int left=s==S151_ROOF_A;if(!s151_anchor(w,a,left?3872:4040,48))return 0;s151_animate(w,e);int trigger=p->script151.flags[left?S151_SIG_ROOF_A:S151_SIG_ROOF_B];if(!trigger){if(a->hit.flags&8){if(left&&!(a->hit.flags&1)&&a->hit.last_kind==1&&!tk_sound(w,0x108d))return 0;a->hit.flags&=247;}trigger=a->hit.flags&1;}if(trigger){if(!boss_fx_pattern(w,entity_ref(e),&a->transform,left?BP_SCRIPT151_8:BP_SCRIPT151_9))return 0;s151_schedule(e,left?S151_ROOF_A_WRECK:S151_ROOF_B_WRECK);}break;}
 case S151_ROOF_A_WRECK:if(!s151_anchor(w,a,3872,48))return 0;s151_animate(w,e);if(p->script151.flags[S151_SIG_ROOF_A_END])return delete_tree(w,e);break;
 case S151_ROOF_B_WRECK:if(p->script151.flags[S151_SIG_ROOF_B_END]){entity_delete_children(w->entities,e);a->state=S151_ROOF_B_DRAIN;} /* fall through */
 case S151_ROOF_B_DRAIN:if(!s151_anchor(w,a,4040,48))return 0;s151_animate(w,e);break;
 case S151_BASE:if(p->script151.flags[S151_SIG_BASE_END]){entity_delete_children(w->entities,e);return boss_fx_schedule(w,e,BFX_SMOKE);} /* fall through */
 case S151_GATE:if(!s151_anchor(w,a,s==S151_GATE?4064:4136,160))return 0;a->palette=q->script151.palettes[!!p->script151.flags[S151_SIG_HIT]];draw(a,script151_raw_draw[s==S151_GATE?0:1]);if(!animation_output_direct(&a->animation,AN_OUTPUT_DIRECT))return enemy_fault(w);break;
 case S151_CREW:if(!s151_anchor(w,a,4136,160))return 0;s151_animate(w,e);break;
 case S151_FRAGMENT:if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;a->vy=tk_word(a->vy+a->ay);weapon_integrate_free(a);s151_animate(w,e);if(!enemy_attack(w,a))return 0;if((uint16_t)tk_sx(w,a)>=320||(uint16_t)tk_word(tk_sy(w,a)-256)>=320)entity_delete(w->entities,e);break;
 case S151_TRAIL:if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;a->palette=p->actor.palette;if(s151_animate(w,e))entity_delete(w->entities,e);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
