#include "fort_internal.h"
static struct NativeEntity*factory(struct WeaponWorld*w,struct NativeEntity*e,unsigned kind){
 if(kind<=FTFX_BLAST_LOW){struct NativeEntity*n=ft_child(w,e,FT_BLAST,ENTITY_LAST_CHILD);if(n)n->payload.enemy.fort.variant=(uint8_t)kind;return n;}
 if(kind>=FTFX_DEBRIS72_A&&kind<=FTFX_DEBRIS88_E){struct NativeEntity*n=ft_child(w,e,FT_DEBRIS,ENTITY_LAST_CHILD);if(n)n->payload.enemy.fort.variant=(uint8_t)(kind-FTFX_DEBRIS72_A);return n;}
 if(kind==FTFX_CHUNK)return ft_child(w,e,FT_CHUNK,ENTITY_LAST_CHILD);
 static const enum BossFxKind fx[]={BFX_SMALL,BFX_FIRE,BFX_BLAST,BFX_SMOKE,BFX_DEBRIS_AD_A,BFX_DEBRIS_AD_B,BFX_DEBRIS77_A,BFX_DEBRIS77_B};
 if(kind>=FTFX_SMALL&&kind<=FTFX_DEBRIS77_B)return boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&e->payload.enemy.actor.transform,fx[kind-FTFX_SMALL]);
 if(kind>=FTFX_DEBRIS0&&kind<=FTFX_DEBRIS7)return boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&e->payload.enemy.actor.transform,(enum BossFxKind)(BFX_DEBRIS0+kind-FTFX_DEBRIS0));
 enemy_fault(w);return 0;
}
static int pattern(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p=ft_parent(w,e);if(!p)return 0;
 q->scale_x=p->scale_x;q->scale_y=p->scale_y;const struct BossPatternData*t=&fort_patterns[q->fort.pattern];
 int emit=q->fort.delay==255;if(!emit&&!--q->fort.delay){q->fort.delay=t->period;emit=1;}
 if(emit){
  if(t->follow){int16_t dx=p->actor.transform.facing&1?ft_word(-t->dx):t->dx,dy=p->actor.transform.facing&1?ft_word(-t->dy):t->dy;
   transform_integer_offset(&a->transform,ft_word((p->actor.transform.x>>8)+dx-(a->transform.x>>8)),ft_word(transform_y_up(&p->actor.transform)+dy-transform_y_up(&a->transform)));
  }
  for(unsigned budget=0;budget<ENTITY_CAPACITY;budget++){
   unsigned kind=t->random?fort_pattern_choices[q->fort.pattern][ft_bounded(w,16)]:t->kind;struct NativeEntity*n=factory(w,e,kind);if(!n)return 0;
   int16_t dx=ft_bounded(w,t->spread_x),dy=ft_bounded(w,t->spread_y);if(a->transform.facing&1)dx=ft_word(-dx);if(a->transform.facing&2)dy=ft_word(-dy);
   struct EnemyActor*v=&n->payload.enemy;transform_integer_offset(&v->actor.transform,dx,dy);v->actor.display_flags|=a->display_flags&64;v->scale_x=q->scale_x;v->scale_y=q->scale_y;
   if(q->fort.delay!=255)break;if(!--q->fort.remaining){entity_delete(w->entities,e);return 1;}if(budget==ENTITY_CAPACITY-1)return enemy_fault(w);
  }
 }if(!--q->fort.remaining)entity_delete(w->entities,e);return !w->fault;
}
int ft_effects_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;
 if(s==FT_PATTERN){if(q->fort.pattern>=FTP_COUNT)return enemy_fault(w);q->fort.delay=fort_patterns[q->fort.pattern].period;q->fort.remaining=fort_patterns[q->fort.pattern].life;return 1;}
 if(s==FT_CHUNK){
  a->palette=0xf3;ft_reset(a);a->vx=(int16_t)ft_masked(w,127);if(a->vx&1)a->vx=ft_word(-a->vx);
  /* The source masks the signed result; it does not take abs(vx). */
  a->vy=ft_word(a->vy-((uint16_t)a->vx&127));ft_start(q,FTA_CHUNK);return !w->fault;
 }
 if(s==FT_BLAST){
  q->scale_x=q->scale_y=255;
  if(q->fort.variant>1)return enemy_fault(w);a->palette=0x6c;ft_start(q,q->fort.variant?FTA_BLAST_LOW:FTA_BLAST_HIGH);
  a->transform.facing|=ft_random(w)&1;if(interaction_outside(w,a,fort_blast_bounds[q->fort.variant],1)){entity_delete(w->entities,e);return 0;}ft_defense(q,65535);return !w->fault;
 }
 if(s==FT_DEBRIS){
  q->scale_x=q->scale_y=255;
  static const uint8_t roots[7]={FTA_DEBRIS_A,FTA_DEBRIS_B,FTA_DEBRIS_B,FTA_DEBRIS_C,FTA_DEBRIS_D,FTA_DEBRIS_E,FTA_DEBRIS_E};
  if(q->fort.variant>=7)return enemy_fault(w);ft_start(q,(enum FortAnimation)roots[q->fort.variant]);a->palette=q->fort.variant==2||q->fort.variant==6?0x88:0x72;
  a->transform.facing|=ft_random(w)&1;a->vy=2040;a->ay=-68;a->ax=0;a->vx=(int16_t)ft_bounded(w,1023);if(a->vx&1)a->vx=ft_word(-a->vx);
  uint16_t r=ft_bounded(w,2047);a->vy=ft_word(a->vy-r);a->transform.facing|=r&1;
  if(interaction_outside(w,a,boss_fx_bounds[q->fort.variant==3?5:4],1)){entity_delete(w->entities,e);return 0;}ft_defense(q,65535);return !w->fault;
 }
 return enemy_fault(w);
}
int ft_effects_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int done=0;
 if(s==FT_PATTERN)return pattern(w,e);
 if(s==FT_DEBRIS&&a->vy<=-1280)a->vy=-1279;
 if(s!=FT_BLAST&&interaction_move(w,a,WMOVE_POINT)<0)return 0;
 if(s!=FT_DEBRIS||boss_sprite_admitted(w))done=ft_animate(w,e);
 const int16_t*b=s==FT_CHUNK?fort_chunk_bounds:s==FT_BLAST?fort_blast_bounds[q->fort.variant]:boss_fx_bounds[q->fort.variant==3?5:4];
 if(done||interaction_outside(w,a,b,1))entity_delete(w->entities,e);return !w->fault;
}
