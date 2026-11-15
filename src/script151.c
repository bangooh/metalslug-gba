#include "script151_internal.h"
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void s151_schedule(struct NativeEntity*e,unsigned s){enemy_schedule(&e->payload.enemy,s);}
struct NativeEntity*s151_allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(e){e->cosmetic=s==S151_TRAIL;uint8_t*b=(uint8_t*)&e->payload.enemy.script151;for(unsigned i=0;i<sizeof(e->payload.enemy.script151);i++)b[i]=255;e->payload.enemy.script151.render_flags=0;e->payload.enemy.actor.palette=0;s151_schedule(e,s);}return e;}
struct NativeEntity*s151_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct PlayerTransform t={.display_group=255};return s151_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,s);}
void s151_start(struct EnemyActor*q,unsigned n){animation_begin(&q->actor.animation,script151_entries[n]);q->actor.movement_flags|=32;}
void s151_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){script151_bindings[n].attack,script151_bindings[n].count};q->actor.attack=&q->attack_set;}}
static void defense(struct EnemyActor*q,unsigned n){q->actor.defense=script151_bindings[n].defense;q->actor.defense_count=script151_bindings[n].count;}
int16_t s151_rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=S151R_COUNT){enemy_fault(w);return 0;}return (int16_t)script151_ranks[n].values[(interaction_rank(w,&q->actor,runtime_owner_script151[n]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];}
int s151_anchor(struct WeaponWorld*w,struct WeaponActor*a,int16_t x,int16_t y){if(!w->secondary_view_valid)return enemy_fault(w);x=tk_word(x-w->secondary_x_offset);y=tk_word(512-y+w->secondary_y_offset);if((uint16_t)x>=384)x=384;transform_integer_offset(&a->transform,tk_word(x-tk_sx(w,a)),tk_word(y-tk_sy(w,a)));return 1;}
int s151_patch(struct WeaponWorld*w,unsigned n){return n<sizeof(script151_patches)/sizeof(*script151_patches)&&scene_video_patch(w->video,&script151_patches[n])?1:enemy_fault(w);}
struct Context{struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(f==S151F_7A&&width==2)return q->script151.palette;if(f==S151F_74&&width==4)return q->script151.overlay;if(f==S151F_78&&width==2)return (uint16_t)q->script151.offset_y;if(f==S151F_20&&width==1)return q->actor.phase;enemy_fault(c->w);return 0;}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;uint32_t old=read(ctx,f,width);if(op==AN_FIELD_AND)v&=old;else if(op==AN_FIELD_OR)v|=old;else if(op==AN_FIELD_XOR)v^=old;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(f==S151F_7A)q->script151.palette=v;else if(f==S151F_74)q->script151.overlay=v;else if(f==S151F_78)q->script151.offset_y=(int16_t)v;else if(f==S151F_20)q->actor.phase=(uint8_t)v;return !c->w->fault;}
static int bind(void*ctx,unsigned kind,uint32_t n){struct Context*c=ctx;if(kind!=AN_ATTACK)return enemy_fault(c->w);if(n==0xffffffffu)n=65535;else if(n>=sizeof(script151_bindings)/sizeof(*script151_bindings))return enemy_fault(c->w);s151_attack(&c->e->payload.enemy,n);hit_begin_attack(&c->e->payload.enemy.actor.hit);return 1;}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;struct WeaponActor*v=&c->e->payload.enemy.actor;if(n==S151C_283CA){hit_begin_attack(&v->hit);return 1;}if(n==S151C_283D8)return enemy_attack(c->w,v);if(n==S151C_434CE)return 1;return s151_effect_callback(c->w,c->e,n);}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
int s151_animate(struct WeaponWorld*w,struct NativeEntity*e){struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=field,.bind=bind,.call=call,.sound=sound};int r=animation_step(&e->payload.enemy.actor.animation,&script151_program,&o);if(e->payload.enemy.actor.animation.fault)enemy_fault(w);return r;}
static int watch(const struct NativeEntity*e){return e->kind==ENTITY_ENEMY?e->payload.enemy.script151.flags[S151_SIG_WATCH]:-1;}
static int script(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<256;budget++){
  if(q->script151.cursor>=sizeof(script151_commands)/sizeof(*script151_commands))return enemy_fault(w);const struct Script151Command*c=&script151_commands[q->script151.cursor];unsigned n=c->value;
  switch(c->op){
  case S151OP_END:return 1;
  case S151OP_TIMER:q->script151.wait=(uint16_t)n;break;
  case S151OP_WAIT:if(q->script151.wait){q->script151.wait--;return 1;}break;
  case S151OP_SOUND:if(!tk_sound(w,n))return 0;break;
  case S151OP_FLAG:if((n&255)>=9)return enemy_fault(w);q->script151.flags[n&255]=(n&256)?255:0;break;
  case S151OP_HP_WAIT:if(q->script151.ratio>n)return 1;break;
  case S151OP_HP_ZERO:if(a->hit.hp)return 1;break;
  case S151OP_EVENTS:if(!mission_event_spawn(w,entity_ref(e),n?0xe9686:0xe9638,watch))return 0;break;
  case S151OP_PATCH:if(!s151_patch(w,n))return 0;break;
  case S151OP_CLEAR:{if(n>=2)return enemy_fault(w);const struct Script151Clear*r=&script151_clear[n];if(!scene_video_clear(w->video,r->plane,r->x,r->y,r->width,r->height))return enemy_fault(w);break;}
  case S151OP_NUKE:if(!s151_child(w,e,S151_NUKE))return 0;break;
  case S151OP_FX:if(n>10||!boss_fx_pattern(w,entity_ref(e),&a->transform,(enum BossPattern)(BP_SCRIPT151_0+n)))return 0;break;
  case S151OP_STAMP:{const struct Script151Stamp*t=&script151_stamp;if(!w->interactions||!w->interactions->terrain)return enemy_fault(w);
   /* $044022 uses background plane's pixel scroll, independent of camera. */
   if(!w->background_view_valid)return enemy_fault(w);transform_integer_offset(&a->transform,tk_word(-w->background_x_offset-tk_sx(w,a)),tk_word(512+w->background_y_offset-tk_sy(w,a)));
   if(!terrain_stamp(w->interactions->terrain,&a->transform,t->dx,t->dy,t->width,t->height,t->cells))return enemy_fault(w);break;}
  case S151OP_UNLOCK:w->enemies->scripted_player_lock=0;break;
  case S151OP_NOP:break;
  default:return enemy_fault(w);
  }q->script151.cursor++;if(c->pause)return 1;if(w->fault)return 0;
 }return enemy_fault(w);
}
static int root_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(a->state!=S151_ROOT)return 1;
 a->hit.hp=s151_rank(w,q,S151R_HP);q->script151.full_hp=(uint16_t)a->hit.hp;q->script151.threshold=q->script151.full_hp>>2;for(unsigned i=0;i<9;i++)q->script151.flags[i]=0;q->script151.flags[S151_SIG_HIT]=255;
 static const uint8_t children[]={S151_WINDOW,S151_WALL_NEW,S151_ROOF_B,S151_ROOF_A,S151_BASE};for(unsigned i=0;i<5;i++)if(!s151_child(w,e,children[i]))return 0;
 a->transform.priority=0xc000;q->script151.clock=150;q->script151.cursor=SCRIPT151_END;q->script151.wait=0;defense(q,S151B_ROOT_IDLE);s151_start(q,S151A_ROOT);a->state=S151_WAIT;return 0;
}
static void root_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==S151_WAIT){if(!w->secondary_view_valid){enemy_fault(w);return;}int trigger=a->hit.flags&8;if(!trigger&&w->secondary_x_offset>=3752){q->script151.clock=(uint8_t)(q->script151.clock-1);trigger=!q->script151.clock;}if(trigger){defense(q,S151B_ROOT);if(!tk_sound(w,0x28))return;q->script151.cursor=q->script151.wait=0;a->state=S151_ACTIVE;}}
 if(!s151_anchor(w,a,4032,224))return;if(s==S151_DEAD){script(w,e);return;}
 interaction_body_contact(w,a,&script151_contact);if((uint16_t)tk_sx(w,a)>=300)a->hit.invulnerable=2;if(!q->script151.full_hp){enemy_fault(w);return;}q->script151.ratio=(uint16_t)(((uint32_t)(uint16_t)a->hit.hp<<8)/q->script151.full_hp);
 if(!script(w,e))return;q->script151.flags[S151_SIG_HIT]=a->hit.flags&8?255:0;if(a->hit.flags&8){a->hit.flags&=247;struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&a->transform,BFX_SMALL);if(!n)return;n->payload.enemy.actor.transform.x=(int32_t)a->hit.impact_x*256;n->payload.enemy.actor.transform.y=(512-(int32_t)a->hit.impact_y)*256;}
 s151_animate(w,e);if(a->hit.flags&1){w->enemies->damage_enabled=0;q->script151.flags[S151_SIG_HIT]=0;s151_schedule(e,S151_DEAD);}
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;if(q->entry){q->entry=0;unsigned s=q->actor.state;int r=s<=S151_DEAD?root_enter(w,e):s<=S151_COLLAPSE?s151_parts_enter(w,e):s151_effects_enter(w,e);if(!r||w->fault)return;}unsigned s=q->actor.state;if(s<=S151_DEAD)root_frame(w,e,s);else if(s<=S151_COLLAPSE)s151_parts_frame(w,e,s);else s151_effects_frame(w,e,s);}
int script151_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,const uint8_t params[8]){struct NativeEntity*e=s151_allocate(w,p,o,t,S151_ROOT);if(!e)return 0;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;}
