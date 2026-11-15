#include "script151_internal.h"
static int target(struct WeaponWorld*w,struct WeaponActor*a,unsigned n){struct WeaponActor query={.transform=a->transform};if(!(a->hit.flags&64)){if(!w->interactions||!w->interactions->terrain){enemy_fault(w);return -1;}transform_integer_offset(&query.transform,w->interactions->terrain->delta_x,-w->interactions->terrain->delta_y);}return enemy_nearest_x(w,&query,&script151_target[n]);}
static int common(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p=tk_parent(w,e);if(!p||!s151_anchor(w,a,4064,160))return 0;
 if(q->script151.clock)q->script151.clock--;unsigned flash=p->script151.flags[S151_SIG_HIT]!=0;uint8_t inherited=p->lifecycle_flags;
 if(flash!=!!(a->hit.flags&8)){inherited=script151_palette_tail_lifecycle[flash];if(!w->video||!w->video->set_palette)return enemy_fault(w);for(unsigned i=0;i<9;i++){const uint16_t*t=script151_palettes[flash][i];if(!w->video->set_palette(w->video->palette_context,(uint8_t)t[0],scene_palette_high(t[1]),(uint8_t)(t[0]>>8)|1))return enemy_fault(w);}if(flash)a->hit.flags|=8;else a->hit.flags&=247;}
 if(flash)q->script151.render_flags|=129;else q->script151.render_flags&=126;/* $02B58 advances A0 to the palette-table terminator on transitions. */if(inherited&64)q->lifecycle_flags|=64;return 1;
}
static int tail(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;if(p->actor.hit.flags&1)s151_schedule(e,S151_COLLAPSE);return !w->fault;}
static int low(struct WeaponWorld*w,struct NativeEntity*e,unsigned resume){struct EnemyActor*q=&e->payload.enemy;struct EnemyActor*p=tk_parent(w,e);if(!p)return -1;if((int16_t)p->script151.threshold<p->actor.hit.hp)return 0;q->script151.resume=(uint8_t)resume;if(q->script151.low_ready)return 1;q->script151.clock=(uint16_t)s151_rank(w,q,S151R_LOW_WAIT);q->actor.state=S151_LOW_WAIT;return 2;}
static void next_burst(struct WeaponWorld*w,struct EnemyActor*q){unsigned r=S151R_WAIT;if(q->script151.burst){q->script151.burst--;r=S151R_BURST_WAIT;}q->script151.clock=(uint16_t)s151_rank(w,q,r);}
int s151_parts_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<8;budget++){unsigned s=a->state;int r;unsigned anim=0;
  switch(s){
  case S151_WALL_NEW:if(!s151_child(w,e,S151_FLAG))return 0;a->palette=q->script151.palettes[0]=0x54;q->script151.palettes[1]=0x1d4;q->script151.template_id=a->hit_palette=0x86;a->transform.priority=0x2000;q->script151.burst=q->script151.clock=q->script151.low_ready=0;s151_schedule(e,S151_WALL);return 0;
  case S151_WALL:anim=S151A_WALL;break;
  case S151_WALL_WAIT:a->palette=q->script151.palettes[0];return 1;
  case S151_WALL_OPEN:anim=S151A_WALL_OPEN;break;
  case S151_TOWER_A:case S151_TOWER_B:
   r=low(w,e,s);if(r<0)return 0;if(r==2){a->palette=q->script151.palettes[0];return !w->fault;}anim=s==S151_TOWER_A?(r?S151A_TOWER_A_LOW:S151A_TOWER_A):(r?S151A_TOWER_B_LOW:S151A_TOWER_B);break;
  case S151_AIM_A:case S151_AIM_B:
   a->palette=q->script151.palettes[0];r=low(w,e,s);if(r<0)return 0;if(r==2)return !w->fault;
   if(target(w,a,r?0:1)>=0){a->state=s==S151_AIM_A?S151_DOOR_B_OPEN:S151_DOOR_A_OPEN;continue;}
   q->script151.burst=(uint16_t)s151_rank(w,q,s==S151_AIM_A?S151R_BURST_A:S151R_BURST_B);a->state=s==S151_AIM_A?S151_FIRE_A:S151_FIRE_B;continue;
  case S151_FIRE_A:case S151_FIRE_B:anim=s==S151_FIRE_A?S151A_FIRE_A:S151A_FIRE_B;next_burst(w,q);s151_start(q,anim);return !w->fault;
  case S151_DOOR_A_OPEN:anim=S151A_DOOR_A_OPEN;break;
  case S151_DOOR_A_LOOP:anim=S151A_DOOR_A_LOOP;q->script151.burst=3;break;
  case S151_DOOR_A_CLOSE:anim=S151A_DOOR_A_CLOSE;break;
  case S151_DOOR_B_OPEN:anim=S151A_DOOR_B_OPEN;break;
  case S151_DOOR_B_LOOP:anim=S151A_DOOR_B_LOOP;q->script151.burst=3;break;
  case S151_DOOR_B_CLOSE:anim=S151A_DOOR_B_CLOSE;break;
  case S151_COLLAPSE:a->hit.flags|=1;anim=S151A_COLLAPSE;break;
  default:return enemy_fault(w);
  }a->palette=q->script151.palettes[0];s151_start(q,anim);return !w->fault;
 }return enemy_fault(w);
}
int s151_parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(!common(w,e))return 0;int done=s151_animate(w,e);
 switch(s){
 case S151_WALL:{struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;q->source_flags=p->source_flags;a->transform.display_group=p->actor.transform.display_group;if(p->script151.flags[S151_SIG_OPEN]){struct NativeEntity*parent=entity_get(w->entities,e->parent);if(!s151_child(w,parent,S151_GATE))return 0;q->script151.clock=60;s151_schedule(e,S151_WALL_WAIT);}return !w->fault;}
 case S151_WALL_WAIT:if(!q->script151.clock)s151_schedule(e,S151_WALL_OPEN);return !w->fault;
 case S151_WALL_OPEN:if(done)s151_schedule(e,S151_AIM_B);return !w->fault;
 case S151_LOW_WAIT:q->script151.clock--;if((int16_t)q->script151.clock<=0){q->script151.low_ready=1;s151_schedule(e,q->script151.resume);}break;
 case S151_TOWER_A:case S151_TOWER_B:if(done&&!q->script151.clock)s151_schedule(e,s==S151_TOWER_A?S151_AIM_A:S151_AIM_B);break;
 case S151_FIRE_A:case S151_FIRE_B:if(done){if(!q->script151.burst)s151_schedule(e,s==S151_FIRE_A?S151_TOWER_B:S151_TOWER_A);else if(!q->script151.clock)s151_schedule(e,s);}break;
 case S151_DOOR_A_OPEN:case S151_DOOR_B_OPEN:if(done)s151_schedule(e,s==S151_DOOR_A_OPEN?S151_DOOR_A_LOOP:S151_DOOR_B_LOOP);break;
 case S151_DOOR_A_LOOP:case S151_DOOR_B_LOOP:if(done){q->script151.burst--;if(q->script151.burst)s151_start(q,s==S151_DOOR_A_LOOP?S151A_DOOR_A_LOOP:S151A_DOOR_B_LOOP);else s151_schedule(e,s==S151_DOOR_A_LOOP?S151_DOOR_A_CLOSE:S151_DOOR_B_CLOSE);}break;
 case S151_DOOR_A_CLOSE:case S151_DOOR_B_CLOSE:if(done)s151_schedule(e,s==S151_DOOR_A_CLOSE?S151_TOWER_A:S151_TOWER_B);break;
 case S151_COLLAPSE:if(done){if(!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_SCRIPT151_10))return 0;return boss_fx_transform(w,e,BFX_FIRE);}return !w->fault;
 default:return enemy_fault(w);
 }return tail(w,e);
}
