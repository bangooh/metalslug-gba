#include "opening_internal.h"
/* Source4D8F2..4E512: independent HP, watched events and parent destruction bits. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
/*5DD22 uses440E4;2783A additionally changes depth priority. */
int op_copy_sync(struct WeaponWorld*w,struct WeaponActor*a){uint16_t priority=a->transform.priority;int r=op_sync(w,a);a->transform.priority=priority;return r;}
void op_schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
void op_start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void op_start(struct EnemyActor*q,unsigned n){op_start_entry(q,opening_entries[n]);}
void op_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:opening_bindings[n].defense;q->actor.defense_count=n==65535?0:opening_bindings[n].count;}
void op_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){opening_bindings[n].attack,opening_bindings[n].count};q->actor.attack=&q->attack_set;}}
int op_patch(struct WeaponWorld*w,unsigned n){return n<OPV_COUNT&&scene_video_patch(w->video,&opening_patches[n])?1:enemy_fault(w);}
int op_stamp(struct WeaponWorld*w,struct WeaponActor*a,unsigned n){if(n>=OPS_COUNT||!w->interactions)return enemy_fault(w);const struct OpeningStamp*s=&opening_stamps[n];return terrain_stamp(w->interactions->terrain,&a->transform,s->dx,s->dy,s->width,s->height,s->cells)?1:enemy_fault(w);}
int op_outside(struct WeaponWorld*w,struct NativeEntity*e){return op_sx(w,&e->payload.enemy.actor)<op_word(-e->payload.enemy.opening.margin);}
int op_cull(struct WeaponWorld*w,struct NativeEntity*e){if(op_outside(w,e)){entity_delete(w->entities,e);return 1;}return 0;}
int op_hit(struct WeaponWorld*w,struct WeaponActor*a){if(a->hit.flags&8){if(!vg_hurt(w,a))return 0;a->hit.flags&=247;}return !w->fault;}
struct EnemyActor*op_parent(struct WeaponWorld*w,struct NativeEntity*e){return vg_parent(w,e);}
static int watch(const struct NativeEntity*e){return e->kind==ENTITY_ENEMY?e->payload.enemy.opening.watch:-1;}
int op_watch(struct WeaponWorld*w,struct NativeEntity*e,uint32_t stream){e->payload.enemy.opening.watch=0;return mission_event_spawn(w,entity_ref(e),stream,watch)!=0;}
int op_before_sync_x(struct WeaponWorld*w,struct WeaponActor*a,int16_t*out){if(!w->scene_runtime)return enemy_fault(w);*out=op_word(op_sx(w,a)+((a->hit.flags&64)?0:w->scene_runtime->maps.planes[SCENE_TILE_MAIN].dx));return 1;}
int op_map_position(struct WeaponWorld*w,struct WeaponActor*a,int16_t x,int16_t y_down){if(!w->background_view_valid)return enemy_fault(w);int16_t nx=op_word(x-w->background_x_offset+w->camera_x),ny=op_word(y_down-w->background_y_offset+w->camera_y);transform_integer_offset(&a->transform,op_word(nx-(a->transform.x>>8)),op_word((a->transform.y>>8)-ny));return 1;}
struct NativeEntity*op_allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert order,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,p,order,t,tick);if(!e)return 0;uint8_t*b=(uint8_t*)&e->payload.enemy.opening;for(unsigned i=0;i<sizeof(e->payload.enemy.opening);i++)b[i]=255;op_schedule(e,s);return e;}
struct NativeEntity*op_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,int dx,int dy,int synchronize){struct EnemyActor*q=&e->payload.enemy;if(synchronize&&!op_copy_sync(w,&q->actor))return 0;struct PlayerTransform t=q->actor.transform;t.display_group=q->source_flags;transform_integer_offset(&t,dx,dy);return op_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,s);}
struct NativeEntity*op_pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned id,int dx,int dy){if(id>=OPP_COUNT){enemy_fault(w);return 0;}struct PlayerTransform t=e->payload.enemy.actor.transform;t.display_group=e->payload.enemy.source_flags;transform_integer_offset(&t,dx,dy);struct NativeEntity*n=op_allocate(w,entity_ref(e),ENTITY_LAST_CHILD,&t,OP_PATTERN);if(n)n->payload.enemy.opening.pattern=id;return n;}
int op_burst(struct WeaponWorld*w,struct NativeEntity*e,unsigned a,unsigned b,unsigned c,int dx,int dy){return op_pattern(w,e,a,0,0)&&op_pattern(w,e,b,0,0)&&op_pattern(w,e,c,dx,dy);}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(opening_bindings)/sizeof(*opening_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)op_defense(&c->e->payload.enemy,n);else if(kind==AN_ATTACK){op_attack(&c->e->payload.enemy,n);hit_begin_attack(&c->e->payload.enemy.actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;if(n==OPC_4FB74)return op_patch(c->w,OPV_P29575E)&&op_patch(c->w,OPV_P295772);if(n==OPC_5F93A){struct EnemyActor*p=op_parent(c->w,c->e);if(!p)return 0;p->dead=255;c->e->payload.enemy.opening.signal=255;return 1;}return enemy_fault(c->w);}
int op_animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.bind=bind,.sound=sound,.call=call};int r=animation_step(&a->animation,&opening_program,&o);if(a->animation.fault)enemy_fault(w);return r;}
static int template_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned id,int dx,int dy){struct EnemyActor*q=&e->payload.enemy;if(!op_copy_sync(w,&q->actor))return 0;struct PlayerTransform t=q->actor.transform;t.display_group=q->source_flags;transform_integer_offset(&t,dx,dy);return id==30?captor_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,id,&t,0):scene_prop_spawn_template(w,entity_ref(e),ENTITY_FIRST_CHILD,id,&t,0);}
static int tower_patch(struct WeaponWorld*w,uint8_t signal){switch(signal){case 16:return op_patch(w,OPV_P29557E);case 17:return op_patch(w,OPV_P295592);case 18:return op_patch(w,OPV_P2955A6);case 1:return op_patch(w,OPV_P2955BA);case 2:return op_patch(w,OPV_P2955CE);default:return 1;}}
static int bunker_patch(struct WeaponWorld*w,struct EnemyActor*q){/*4FAF8 never writes the compared source74 byte. */if(q->opening.patch_seen==q->dead)return 1;switch(q->dead){case 16:return op_patch(w,OPV_P295646);case 1:return op_patch(w,OPV_P29565A);case 17:return op_patch(w,OPV_P29566E);default:return 1;}}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;
 switch(a->state){
 case OP_HUT:a->palette=0x3f;q->scale_x=q->scale_y=255;a->transform.facing=0;q->dead=0;if(!template_child(w,e,30,40,1)||!op_child(w,e,OP_ROOF,40,1,1))return 0;op_start(q,OPA_HUT);break;
 case OP_HUT_WRECK:if(!op_sync(w,a)||!op_stamp(w,a,OPS_S295D38))return 0;op_start(q,OPA_HUT_WRECK);break;
 case OP_ROOF:a->palette=0x3d;q->scale_x=q->scale_y=255;a->transform.facing=0;a->hit.hp=20;op_start(q,OPA_ROOF);break;
 case OP_ROOF_DIE:if(!vg_score(w,a,0x1000))return 0;op_attack(q,OPB_ROOF_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;op_start(q,OPA_ROOF_DIE);break;
 case OP_TOWER:if(!template_child(w,e,259,92,34)||!op_child(w,e,OP_TOP,0,0,1)||!op_child(w,e,OP_BASE,128,48,1))return 0;q->dead=a->phase=0;break;
 case OP_TOP:if(!op_watch(w,e,0xe92b2))return 0;q->opening.margin=80;a->hit.hp=60;a->phase=0;op_start(q,OPA_TOP);break;
 case OP_TOP_WRECK:if(!vg_score(w,a,0x1000))return 0;op_defense(q,65535);q->opening.watch=255;if(!interaction_sound(w,0x1030)||!op_patch(w,OPV_P29556A)||!op_patch(w,OPV_P2955E2))return 0;a->state=OP_DRAIN_SYNC;break;
 case OP_BASE:n=op_child(w,e,OP_BLOCK,41,9,1);if(!n)return 0;n->payload.enemy.actor.phase=0x88;q->opening.margin=80;a->hit.hp=60;q->dead=a->phase=0;op_start(q,OPA_BASE);break;
 case OP_BASE_WRECK:if(!vg_score(w,a,0x1000))return 0;op_defense(q,65535);if(!interaction_sound(w,0x1028)||!op_patch(w,OPV_P29560A)||!op_pattern(w,e,OPP_P295CB0,-32,48))return 0;a->state=OP_DRAIN_SYNC;break;
 case OP_FLAG:a->palette=0x40;q->scale_x=q->scale_y=255;a->transform.facing=0;op_start(q,OPA_FLAG);break;
 case OP_BUNKER:if(!op_watch(w,e,0xe92ea))return 0;q->opening.margin=112;a->hit.hp=60;a->phase=q->dead=0;op_start(q,OPA_BUNKER);break;
 case OP_BUNKER_WRECK:{if(!vg_score(w,a,0x1000))return 0;op_defense(q,65535);int16_t x=0;if(!op_before_sync_x(w,a,&x)||!interaction_sound(w,0x1030)||!op_patch(w,x>=0?OPV_P29561E:OPV_P295632))return 0;a->state=OP_BUNKER_END;break;}
 case OP_BUNKER_END:break;
 case OP_BRIDGE:if(!op_child(w,e,OP_PILLAR,0,0,1))return 0;n=op_child(w,e,OP_PILLAR,208,0,1);if(!n)return 0;n->payload.enemy.actor.transform.facing|=1;q->opening.margin=96;q->dead=0;if(!op_watch(w,e,0xe9310))return 0;break;
 case OP_BRIDGE_ACTIVE:a->hit.hp=60;op_start(q,OPA_BRIDGE);break;
 case OP_PILLAR:if(!op_child(w,e,OP_BLOCK,a->transform.facing&1?-20:43,-23,1))return 0;q->dead=0;a->hit.hp=60;q->opening.margin=112;op_start(q,OPA_PILLAR);break;
 case OP_NEST:{a->palette=0x40;q->opening.margin=112;a->hit.hp=300;op_start(q,OPA_NEST);if(!op_copy_sync(w,a))return 0;struct PlayerTransform t=a->transform;t.display_group=q->source_flags;transform_integer_offset(&t,152,56);if(!nest185_attached(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,0))return 0;a->state=OP_NEST_WAIT;break;}
 case OP_NEST_WAIT:case OP_NEST_ACTIVE:break;
 case OP_NEST_WRECK:if(!vg_score(w,a,0x3000))return 0;op_defense(q,65535);if(!op_sync(w,a)||!op_stamp(w,a,OPS_S295D78)||!interaction_sound(w,0x1030))return 0;op_start(q,OPA_NEST_WRECK);break;
 case OP_SHED:if(!template_child(w,e,259,64,73))return 0;q->opening.margin=112;a->hit.hp=30;op_start(q,OPA_SHED);break;
 case OP_SHED2:if(!interaction_sound(w,0x1028)||!op_patch(w,OPV_P295786))return 0;a->hit.hp=30;break;
 case OP_DRAIN_SYNC:case OP_DRAIN:break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;
 if(s==OP_TOWER){if(q->dead==255){entity_delete(w->entities,e);return 1;}if(a->phase!=q->dead&&!tower_patch(w,q->dead))return 0;if(q->dead==18)entity_delete(w->entities,e);a->phase=q->dead;return !w->fault;}
 if(s==OP_NEST_WAIT){int16_t x=0;if(!op_before_sync_x(w,a,&x))return 0;if(x<272){a->state=OP_NEST_ACTIVE;s=OP_NEST_ACTIVE;}else{if(!op_sync(w,a))return 0;op_animate(w,e);return !w->fault;}}
 if(!op_sync(w,a))return 0;
 if(s==OP_DRAIN_SYNC){op_schedule(e,OP_DRAIN);return 1;}
 if(s==OP_DRAIN){uint16_t next=e->next;if(next==ENTITY_NONE||w->entities->nodes[next].depth<=e->depth)entity_delete(w->entities,e);return 1;}
 if(s==OP_BUNKER_END){if(op_outside(w,e))op_schedule(e,OP_DRAIN);return bunker_patch(w,q);}
 if(s==OP_BRIDGE){if(q->dead==17){if(!interaction_sound(w,0x1030)||!op_burst(w,e,OPP_P295BEA,OPP_P295BFC,OPP_P295CC2,0,0)||!op_patch(w,OPV_P2956AA)||!op_patch(w,OPV_P2956E6))return 0;op_schedule(e,OP_BRIDGE_ACTIVE);}op_cull(w,e);return !w->fault;}
 int done=op_animate(w,e);
 if(s==OP_HUT){if(q->dead==255)op_schedule(e,OP_HUT_WRECK);}
 else if(s==OP_ROOF_DIE){if(done){op_attack(q,65535);hit_begin_attack(&a->hit);if(!interaction_sound(w,0x1028))return 0;p=op_parent(w,e);if(!p)return 0;p->dead=255;if(!op_pattern(w,e,OPP_P295BD8,0,0))return 0;op_schedule(e,OP_DRAIN);}return !w->fault;}
 else if(s==OP_FLAG){p=op_parent(w,e);if(!p)return 0;if(p->actor.phase==255)entity_delete(w->entities,e);return !w->fault;}
 else if(s==OP_ROOF||s==OP_TOP||s==OP_BASE||s==OP_BUNKER||s==OP_PILLAR||s==OP_BRIDGE_ACTIVE||s==OP_NEST_ACTIVE||s==OP_SHED||s==OP_SHED2){
  if(s==OP_BASE&&(int8_t)a->phase<1&&a->hit.hp<=30){if(!interaction_sound(w,0x1028)||!op_pattern(w,e,OPP_P295BA2,0,0)||!op_patch(w,OPV_P2955F6))return 0;q->dead=255;p=op_parent(w,e);if(!p)return 0;p->dead++;a->phase=1;}
  if(!op_hit(w,a))return 0;
  if(a->hit.flags&1){switch(s){
   case OP_ROOF:op_schedule(e,OP_ROOF_DIE);break;
   case OP_TOP:case OP_BUNKER:transform_integer_offset(&a->transform,16,0);if(!op_burst(w,e,OPP_P295B5A,OPP_P295B6C,OPP_P295CB0,32,s==OP_TOP?48:32))return 0;if(s==OP_TOP){p=op_parent(w,e);if(!p)return 0;p->dead+=16;}else{q->opening.watch=255;q->dead+=16;}a->phase=255;op_schedule(e,s==OP_TOP?OP_TOP_WRECK:OP_BUNKER_WRECK);break;
   case OP_BASE:if(!op_pattern(w,e,OPP_P295BA2,0,0)||!op_pattern(w,e,OPP_P295BB4,0,0))return 0;p=op_parent(w,e);if(!p)return 0;p->dead++;op_schedule(e,OP_BASE_WRECK);break;
   case OP_PILLAR:if(!interaction_sound(w,0x102f))return 0;op_defense(q,65535);if(!op_pattern(w,e,OPP_P295B7E,0,-32)||!op_pattern(w,e,OPP_P295B90,0,-32)||!op_pattern(w,e,OPP_P295CB0,a->transform.facing&1?-32:32,0)||!op_patch(w,a->transform.facing&1?OPV_P2956D2:OPV_P295696)||!op_patch(w,a->transform.facing&1?OPV_P295736:OPV_P29570E))return 0;p=op_parent(w,e);if(!p)return 0;p->dead|=a->transform.facing&1?1:16;q->dead=255;entity_delete(w->entities,e);return !w->fault;
   case OP_BRIDGE_ACTIVE:op_defense(q,65535);if(!vg_score(w,a,0x5000)||!interaction_sound(w,0x1028)||!op_burst(w,e,OPP_P295C0E,OPP_P295C20,OPP_P295CC2,0,0)||!op_patch(w,OPV_P2956BE)||!op_patch(w,OPV_P2956FA)||!op_patch(w,OPV_P295722)||!op_patch(w,OPV_P29574A))return 0;q->opening.watch=255;entity_delete(w->entities,e);return !w->fault;
   case OP_NEST_ACTIVE:if(!interaction_sound(w,0x102f)||!op_burst(w,e,OPP_P295C56,OPP_P295C68,OPP_P295CE6,0,0))return 0;op_schedule(e,OP_NEST_WRECK);break;
   case OP_SHED:case OP_SHED2:if(s==OP_SHED)a->hit.flags&=254;else{if(!vg_score(w,a,0x1000)||!interaction_sound(w,0x1030))return 0;op_defense(q,65535);}if(!op_pattern(w,e,OPP_P295BA2,32,48)||!op_pattern(w,e,OPP_P295CB0,32,48))return 0;if(s==OP_SHED)op_schedule(e,OP_SHED2);else{if(!op_patch(w,OPV_P29579A))return 0;entity_delete(w->entities,e);}break;
   default:return enemy_fault(w);
  }}
 }
 if(op_outside(w,e)){if(s==OP_TOP||s==OP_BASE){p=op_parent(w,e);if(!p)return 0;p->dead|=s==OP_TOP?240:15;if(s==OP_TOP)a->phase=255;op_schedule(e,OP_DRAIN_SYNC);}else if(s==OP_BUNKER){a->phase=255;op_schedule(e,OP_BUNKER_END);}else{if(s==OP_BRIDGE_ACTIVE)q->opening.watch=255;entity_delete(w->entities,e);}}
 return s==OP_BUNKER?bunker_patch(w,q):!w->fault;
}
static int pattern(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy,*p=op_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;q->scale_x=p->scale_x;q->scale_y=p->scale_y;const struct BossPatternData*t=&opening_patterns[q->opening.pattern];int emit=q->opening.delay==255;if(!emit&&!--q->opening.delay){q->opening.delay=t->period;emit=1;}
 if(emit){if(t->follow){int16_t dx=p->actor.transform.facing&1?op_word(-t->dx):t->dx,dy=p->actor.transform.facing&1?op_word(-t->dy):t->dy;transform_integer_offset(&a->transform,op_word((p->actor.transform.x>>8)+dx-(a->transform.x>>8)),op_word(transform_y_up(&p->actor.transform)+dy-transform_y_up(&a->transform)));}
  for(unsigned budget=0;budget<ENTITY_CAPACITY;budget++){unsigned kind=t->random?opening_pattern_choices[q->opening.pattern][vg_bounded(w,16)]:t->kind;struct PlayerTransform tr=a->transform;tr.display_group=q->source_flags;struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&tr,(enum BossFxKind)kind);if(!n)return 0;int16_t dx=vg_bounded(w,t->spread_x),dy=vg_bounded(w,t->spread_y);if(a->transform.facing&1)dx=op_word(-dx);if(a->transform.facing&2)dy=op_word(-dy);transform_integer_offset(&n->payload.enemy.actor.transform,dx,dy);n->payload.enemy.actor.display_flags|=a->display_flags&64;n->payload.enemy.scale_x=q->scale_x;n->payload.enemy.scale_y=q->scale_y;if(q->opening.delay!=255)break;if(!--q->opening.remaining){entity_delete(w->entities,e);return !w->fault;}if(budget==ENTITY_CAPACITY-1)return enemy_fault(w);}
 }if(!--q->opening.remaining)entity_delete(w->entities,e);return !w->fault;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;unsigned s=q->actor.state;if(s==OP_PATTERN){if(q->entry){q->entry=0;if(q->opening.pattern>=OPP_COUNT){enemy_fault(w);return;}q->opening.remaining=opening_patterns[q->opening.pattern].life;q->opening.delay=opening_patterns[q->opening.pattern].period;}pattern(w,e);return;}
 if(q->entry){q->entry=0;if(!(s>=OP_BLOCK?op_crew_enter(w,e):s>=OP_BARRIER?op_fort_enter(w,e):enter(w,e)))return;}if(e->deleting||!e->active)return;s=q->actor.state;if(s>=OP_BLOCK)op_crew_frame(w,e,s);else if(s>=OP_BARRIER)op_fort_frame(w,e,s);else frame(w,e,s);
}
int opening_spawn_scene(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,enum OpeningEntry id,uint16_t config,uint8_t flags){static const uint8_t states[]={OP_HUT,OP_TOWER,OP_BUNKER,OP_BRIDGE,OP_NEST,OP_SHED,OP_BARRIER};if((unsigned)id>=OPENING_ENTRY_COUNT)return enemy_fault(w);struct NativeEntity*e=op_allocate(w,p,o,t,states[id]);if(!e)return 0;e->payload.enemy.opening.margin=(int16_t)config;e->payload.enemy.actor.display_flags|=flags;return 1;}
int opening_clear_ordnance(struct WeaponWorld*w,struct EntityRef p,int damage){struct PlayerTransform t={.x=-256,.y=513*256,.priority=65535,.display_group=255};struct NativeEntity*e=op_allocate(w,p,ENTITY_FIRST_CHILD,&t,OP_NUKE);if(!e)return 0;e->payload.enemy.actor.variant=damage!=0;return 1;}
struct NativeEntity*opening_flag(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t){return op_allocate(w,p,o,t,OP_FLAG);}
struct NativeEntity*opening_side(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t){return op_allocate(w,p,o,t,OP_SIDE);}
