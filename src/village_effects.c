#include "village_internal.h"
int vg_attacker_velocity(struct WeaponWorld*w,const struct WeaponActor*a,int16_t*vx,int16_t*vy){unsigned i=a->hit.last_attacker;if(i>=ENTITY_CAPACITY)return enemy_fault(w);struct NativeEntity*n=&w->entities->nodes[i];if(!n->active)return enemy_fault(w);if(n->kind==ENTITY_PLAYER){*vx=n->payload.player.vx;*vy=n->payload.player.vy;return 1;}if(n->kind==ENTITY_PLAYER_ARM){*vx=*vy=0;return 1;}struct WeaponActor*b=entity_weapon_actor(n);if(!b)return enemy_fault(w);*vx=b->vx;*vy=b->vy;return 1;}
static struct NativeEntity*factory(struct WeaponWorld*w,struct NativeEntity*e,unsigned kind){
 static const enum BossFxKind fx[]={BFX_SMALL,BFX_FIRE,BFX_BLAST,BFX_FIRE_BLUE,BFX_SMOKE_BLUE};
 if(kind<=VFX_BFX_SMOKE_BLUE)return boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&e->payload.enemy.actor.transform,fx[kind]);
 if(kind==VFX_ONE_SHOT)return vg_child(w,e,VG_ONE_SHOT,ENTITY_LAST_CHILD);
 if(kind>=VFX_WOOD_A&&kind<=VFX_WOOD_D){struct NativeEntity*n=vg_child(w,e,VG_DEBRIS,ENTITY_LAST_CHILD);if(n)n->payload.enemy.village.variant=kind-VFX_WOOD_A;return n;}
 if(kind>=VFX_BFX_DEBRIS0&&kind<=VFX_BFX_DEBRIS7)return boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&e->payload.enemy.actor.transform,(enum BossFxKind)(BFX_DEBRIS0+kind-VFX_BFX_DEBRIS0));enemy_fault(w);return 0;
}
static int pattern(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy,*p=vg_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;q->scale_x=p->scale_x;q->scale_y=p->scale_y;const struct BossPatternData*t=&village_patterns[q->village.pattern];int emit=q->village.delay==255;
 if(!emit&&!--q->village.delay){q->village.delay=t->period;emit=1;}
 if(emit){if(t->follow){int16_t dx=p->actor.transform.facing&1?vg_word(-t->dx):t->dx,dy=p->actor.transform.facing&1?vg_word(-t->dy):t->dy;transform_integer_offset(&a->transform,vg_word((p->actor.transform.x>>8)+dx-(a->transform.x>>8)),vg_word(transform_y_up(&p->actor.transform)+dy-transform_y_up(&a->transform)));}
  for(unsigned budget=0;budget<ENTITY_CAPACITY;budget++){unsigned kind=t->random?village_pattern_choices[q->village.pattern][vg_bounded(w,16)]:t->kind;struct NativeEntity*n=factory(w,e,kind);if(!n)return 0;int16_t dx=vg_bounded(w,t->spread_x),dy=vg_bounded(w,t->spread_y);if(a->transform.facing&1)dx=vg_word(-dx);if(a->transform.facing&2)dy=vg_word(-dy);struct EnemyActor*v=&n->payload.enemy;transform_integer_offset(&v->actor.transform,dx,dy);v->actor.display_flags|=a->display_flags&64;v->scale_x=q->scale_x;v->scale_y=q->scale_y;if(q->village.delay!=255)break;if(!--q->village.remaining){entity_delete(w->entities,e);return !w->fault;}if(budget==ENTITY_CAPACITY-1)return enemy_fault(w);}
 }if(!--q->village.remaining)entity_delete(w->entities,e);return !w->fault;
}
static void impact(struct WeaponActor*a,const struct WeaponActor*p){transform_integer_offset(&a->transform,vg_word(p->hit.impact_x-(a->transform.x>>8)),vg_word(p->hit.impact_y-transform_y_up(&a->transform)));}
int vg_effects_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;unsigned s=a->state;
 switch(s){
 case VG_PATTERN:if(q->village.pattern>=VGP_COUNT)return enemy_fault(w);q->village.delay=village_patterns[q->village.pattern].period;q->village.remaining=village_patterns[q->village.pattern].life;break;
 case VG_ONE_SHOT:a->palette=0x6d;q->scale_x=q->scale_y=255;a->transform.facing=0;vg_start(q,VGA_ONE_SHOT);break;
 case VG_LAMP_SPARK:a->palette=0x41;a->display_flags|=64;q->scale_x=q->scale_y=255;a->transform.facing=0;vg_reset(a);q->village.timer=240+(vg_random(w)&31);a->vx=vg_random(w)&31;if(a->vx&1)a->vx=vg_word(-a->vx);a->vy=vg_random(w)&63;if(a->vy&1)a->vy=vg_word(-a->vy);vg_start(q,VGA_LAMP_SPARK);break;
 case VG_CHUNK:
  a->palette=q->village.variant?11:0x29;
  if(!q->village.variant){p=vg_parent(w,e);if(!p)return 0;impact(a,&p->actor);}vg_reset(a);a->ay=-16;
  if(q->village.variant==2)a->vy=vg_masked(w,511);a->vx=vg_masked(w,q->village.variant==2?255:15);if(a->vx&1)a->vx=vg_word(-a->vx);q->village.timer=40;vg_start(q,VGA_CHUNK);break;
 case VG_FLYING:
  a->palette=q->village.variant?0xad:0x9e;vg_reset(a);p=vg_parent(w,e);if(!p)return 0;impact(a,&p->actor);a->vy=3072;a->vx=w->frame_phase&1?-256:256;
  if(p->actor.hit.last_kind!=4){int16_t vx,vy;if(!vg_attacker_velocity(w,&p->actor,&vx,&vy))return 0;a->vx=vg_word(vx*2);a->vy=vg_word(vy*2);}a->ay=vg_word(-((vg_masked(w,511)|256)&511));vg_start_entry(q,village_flying[vg_masked(w,3)]);break;
 case VG_DROP:{
  a->palette=10;vg_start_entry(q,village_flying[vg_masked(w,3)]);p=vg_parent(w,e);if(!p)return 0;impact(a,&p->actor);q->scale=vg_masked(w,1023)+1;interaction_priority(w,a,0x8000,20);
  uint16_t r=vg_random(w);unsigned angle=(r&62)+32;int16_t y=vg_word(boss_trig[0][angle]*((r&7)+1));y>>=2;a->vy=vg_word(((int32_t)y*q->scale)>>8);a->vx=vg_word(((int32_t)boss_trig[1][angle]*q->scale)>>8);
  if((int8_t)vg_random(w)<=0){a->vx=vg_word(a->vx*((vg_random(w)&3)+2))>>1;a->vy=vg_word(a->vy*((vg_random(w)&3)+2))>>1;}q->village.launch_y=a->vy;a->ay=-48;q->village.blink=255;q->village.remaining=1;break;
 }
 case VG_DEBRIS:{
  if(q->village.variant>=4)return enemy_fault(w);vg_start_entry(q,village_debris[q->village.variant]);a->palette=10;q->scale_x=q->scale_y=255;a->transform.facing|=vg_random(w)&1;a->vy=2040;a->ay=-68;a->ax=0;a->vx=vg_bounded(w,1023);if(a->vx&1)a->vx=vg_word(-a->vx);uint16_t r=vg_bounded(w,2047);a->vy=vg_word(a->vy-r);a->transform.facing|=r&1;if(interaction_outside(w,a,boss_fx_bounds[q->village.variant&1?4:5],1)){entity_delete(w->entities,e);return 0;}vg_defense(q,65535);break;
 }
 default:return enemy_fault(w);
 }return !w->fault;
}
int vg_effects_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int done=0,r;
 switch(s){
 case VG_PATTERN:return pattern(w,e);
 case VG_ONE_SHOT:if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(vg_animate(w,e))entity_delete(w->entities,e);return !w->fault;
 case VG_LAMP_SPARK:if(interaction_move(w,a,WMOVE_POINT)<0)return 0;if(q->village.timer<=20&&(q->village.timer&1))a->animation.delay--;else vg_animate(w,e);q->village.timer=vg_word(q->village.timer-1);if(q->village.timer<=0)entity_delete(w->entities,e);if(interaction_outside(w,a,village_lamp_bounds,0))entity_delete(w->entities,e);return !w->fault;
 case VG_CHUNK:if(interaction_move(w,a,WMOVE_POINT)<0)return 0;if(q->village.timer==10)a->damage_timer=20;vg_animate(w,e);if(interaction_outside(w,a,village_chunk_bounds,1)||!--q->village.timer)entity_delete(w->entities,e);return !w->fault;
 case VG_FLYING:r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r){a->vy=vg_word(-a->vy)>>1;if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMALL))return 0;if(a->vy<256){entity_delete(w->entities,e);return 1;}}vg_animate(w,e);if(interaction_outside(w,a,boss_fx_bounds[8],1))entity_delete(w->entities,e);return !w->fault;
 case VG_DROP:r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r){a->ay=-48;a->vx>>=2;q->village.launch_y>>=1;a->vy=q->village.launch_y;if((int16_t)--q->village.remaining<=0)vg_schedule(e,VG_DELETE);}if((q->village.remaining==1||(++q->village.blink&1))&&boss_sprite_admitted(w))vg_animate(w,e);if(interaction_outside(w,a,boss_fx_bounds[6],1))vg_schedule(e,VG_DELETE);return !w->fault;
 case VG_DEBRIS:if(a->vy<=-1280)a->vy=-1279;if(interaction_move(w,a,WMOVE_POINT)<0)return 0;if(boss_sprite_admitted(w))done=vg_animate(w,e);if(done||interaction_outside(w,a,boss_fx_bounds[q->village.variant&1?4:5],1))entity_delete(w->entities,e);return !w->fault;
 default:return enemy_fault(w);
 }
}
