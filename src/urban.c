#include "urban_internal.h"
#include "emitter59_internal.h"
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void ur_schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
void ur_start_entry(struct EnemyActor*q,uint16_t n){if(n!=65535){animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}}
void ur_start(struct EnemyActor*q,unsigned n){ur_start_entry(q,urban_entries[n]);}
void ur_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:urban_bindings[n].defense;q->actor.defense_count=n==65535?0:urban_bindings[n].count;}
static int patch(struct WeaponWorld*w,unsigned n){return n<URV_COUNT&&scene_video_patch(w->video,&urban_patches[n])?1:enemy_fault(w);}
static int stamp(struct WeaponWorld*w,struct WeaponActor*a,unsigned n){if(n>=URS_COUNT||!w->interactions)return enemy_fault(w);const struct UrbanStamp*s=&urban_stamps[n];return terrain_stamp(w->interactions->terrain,&a->transform,s->dx,s->dy,s->width,s->height,s->cells)?1:enemy_fault(w);}
static int cull(struct WeaponWorld*w,struct NativeEntity*e){if(op_sx(w,&e->payload.enemy.actor)<op_word(-e->payload.enemy.urban.margin)){entity_delete(w->entities,e);return 1;}return 0;}
static int children(struct WeaponWorld*w,struct NativeEntity*e){return e->next!=ENTITY_NONE&&w->entities->nodes[e->next].depth>e->depth;}
struct NativeEntity*ur_allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(!e)return 0;uint8_t*b=(uint8_t*)&e->payload.enemy.urban;for(unsigned i=0;i<sizeof(e->payload.enemy.urban);i++)b[i]=255;ur_schedule(e,s);return e;}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;if(!op_copy_sync(w,&q->actor))return 0;struct PlayerTransform t=q->actor.transform;t.display_group=q->source_flags;return ur_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,s);}
struct NativeEntity*ur_pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned id){if(id>=URP_COUNT){enemy_fault(w);return 0;}struct PlayerTransform t=e->payload.enemy.actor.transform;t.display_group=e->payload.enemy.source_flags;struct NativeEntity*n=ur_allocate(w,entity_ref(e),ENTITY_LAST_CHILD,&t,UR_PATTERN);if(n)n->payload.enemy.urban.pattern=id;return n;}
static int burst(struct WeaponWorld*w,struct NativeEntity*e,unsigned a,unsigned b,unsigned c){return ur_pattern(w,e,a)&&ur_pattern(w,e,b)&&ur_pattern(w,e,c);}
struct Context{struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;if(f==URF_72_2&&width==2)return (uint16_t)c->e->payload.enemy.urban.stage;if(f==URF_20_1&&width==1)return c->e->payload.enemy.actor.phase;enemy_fault(c->w);return 0;}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;uint32_t old=read(ctx,f,width);if(op==AN_FIELD_AND)v&=old;else if(op==AN_FIELD_OR)v|=old;else if(op==AN_FIELD_XOR)v^=old;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(f==URF_72_2)c->e->payload.enemy.urban.stage=(int16_t)v;else c->e->payload.enemy.actor.phase=(uint8_t)v;return !c->w->fault;}
static int bind(void*ctx,unsigned k,uint32_t v){struct Context*c=ctx;unsigned n=v==0xffffffffu?65535:v;if(k!=AN_DEFENSE||(n!=65535&&n>=sizeof(urban_bindings)/sizeof(*urban_bindings)))return enemy_fault(c->w);ur_defense(&c->e->payload.enemy,n);return 1;}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;if(n>=sizeof(urban_callback_patches)/sizeof(*urban_callback_patches))return enemy_fault(c->w);return patch(c->w,urban_callback_patches[n]);}
int ur_animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=field,.bind=bind,.sound=sound,.call=call};int done=animation_step(&a->animation,&urban_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
static void setup(struct EnemyActor*q,uint16_t pal,int16_t margin,int16_t hp,unsigned graph){q->actor.palette=pal;q->urban.margin=margin;q->actor.hit.hp=hp;q->scale_x=q->scale_y=255;q->actor.transform.facing=0;ur_start(q,graph);}
static int rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>1)return enemy_fault(w);q->actor.hit.hp=urban_rank[n][(interaction_rank(w,&q->actor,runtime_owner_urban[n]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];return 1;}
static void depth(struct WeaponWorld*w,struct WeaponActor*a){uint16_t v=a->transform.priority;if((uint16_t)(v-0x4000)<0x7fff)a->transform.priority=(v&0xe01f)|((255-((transform_y_up(&a->transform)+w->camera_y)&255))<<5);}
static int fixed_depth(struct WeaponWorld*w,struct WeaponActor*a,int y){if(!w->secondary_view_valid)return enemy_fault(w);/*2813C uses map Y through440D0, not actorY or extra priority bits. */unsigned row=(uint16_t)(512-y+w->secondary_y_offset)&255;a->transform.priority=0x8000|((255-row)<<5);return 1;}
static int palette_mode(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){static const uint16_t first[4][3]={{0x4b,0x4e,0x8e},{0x4a,0x4d,0x8d},{0xbd,0xbe,0x123},{0x4c,0x4f,0x8f}};struct SceneVideoWorld*v=w->video;uint16_t period=n<2?65535:1;uint8_t flags=n==2?2:1;if(!v||!v->palette_cycle||!v->actor_palette_cycle)return enemy_fault(w);return v->palette_cycle(v->palette_context,0x1a,scene_palette_high(first[n][0]),period,flags)&&v->palette_cycle(v->palette_context,0x1d,scene_palette_high(first[n][1]),period,flags)&&v->actor_palette_cycle(v->palette_context,e->index,e->generation,first[n][2],period,flags)?1:enemy_fault(w);}
static int neon(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy,*p=op_parent(w,e);if(!p)return 0;if(q->urban.latch)return 1;if(w->enemies->palette_shutdown){if(!palette_mode(w,e,2))return 0;q->urban.latch=w->enemies->palette_shutdown;return 1;}if(p->captor_done==0x7f)return 1;if(p->captor_done==255){if(!palette_mode(w,e,2))return 0;p->captor_done=0x7f;return 1;}if(p->dead==255)return 1;q->urban.clock++;if(p->dead==1)return palette_mode(w,e,3);if(p->dead==2)return palette_mode(w,e,0);if(!(q->urban.clock&3))return palette_mode(w,e,q->urban.clock&4?0:1);return 1;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;struct EnemyActor*p;
 switch(s){
 case UR_BUILDING:setup(q,0x8b,144,120,URA_BUILDING);break;
 case UR_BUILDING2:if(!op_sync(w,a)||!stamp(w,a,URS_S29B29A))return 0;ur_start(q,URA_BUILDING2);break;
 case UR_BUILDING_WRECK:if(!vg_score(w,a,0x2000)||!op_sync(w,a)||!stamp(w,a,URS_S29B2B2))return 0;ur_defense(q,65535);ur_start(q,URA_BUILDING_WRECK);break;
 case UR_COLUMN:a->palette=0x8c;q->urban.margin=64;a->hit.hp=60;q->scale_x=q->scale_y=255;a->transform.facing=0;a->transform.priority=0x8000;vg_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;ur_start(q,URA_COLUMN);break;
 case UR_WALL_CHILD:case UR_WALL:a->phase=s==UR_WALL;a->hit.hp=s==UR_WALL?640:280;ur_start(q,s==UR_WALL?URA_WALL:URA_WALL_CHILD);if(!op_sync(w,a)||!stamp(w,a,s==UR_WALL?URS_S29B2F2:URS_S29B2CA))return 0;q->urban.margin=96;a->state=UR_WALL;break;
 case UR_WALL2:ur_start(q,a->phase?URA_WALL2:URA_WALL_CHILD2);break;
 case UR_WALL_WRECK:if(!op_sync(w,a))return 0;if(!a->phase){if(!vg_score(w,a,0x3000))return 0;p=op_parent(w,e);if(!p)return 0;p->actor.phase=255;ur_start(q,URA_WALL_CHILD_WRECK);if(!stamp(w,a,URS_S29B2DE))return 0;}else{if(!vg_score(w,a,0x1000))return 0;transform_integer_offset(&a->transform,48,0);ur_start(q,URA_WALL_WRECK);if(!stamp(w,a,URS_S29B306))return 0;}break;
 case UR_COMPOUND:q->urban.margin=96;if(!child(w,e,UR_WALL_CHILD)||!child(w,e,UR_NEON_WAIT)||!child(w,e,UR_CAPTOR_TRIGGER))return 0;a->phase=q->captor_done=0;/*fall through*/
 case UR_COMPOUND_IDLE:q->dead=0;a->state=UR_COMPOUND_IDLE;break;
 case UR_COMPOUND_WAIT:case UR_COMPOUND_DRAIN:q->dead=255;break;
 case UR_CAPTOR_TRIGGER:case UR_NEON_WAIT:break;
 case UR_NEON:setup(q,0x8d,64,60,URA_NEON);transform_integer_offset(&a->transform,160,96);if(!fixed_depth(w,a,176))return 0;q->urban.clock=0;q->urban.latch=0;break;
 case UR_NEON_WRECK:ur_start(q,URA_NEON_WRECK);break;
 case UR_STALL:setup(q,0x91,160,60,URA_STALL);if(!fixed_depth(w,a,160))return 0;break;
 case UR_STALL_WRECK:if(!op_sync(w,a))return 0;ur_start(q,URA_STALL_WRECK);if(!w->secondary_view_valid)return enemy_fault(w);if((int16_t)w->secondary_x_offset<0x390&&(!patch(w,URV_P29A5F2)||!stamp(w,a,URS_S29B3B2)))return 0;break;
 case UR_FRAGILE:setup(q,0x92,128,1,URA_FRAGILE);break;
 case UR_FRAGILE_BREAK:if(!interaction_sound(w,0x1054))return 0;ur_start(q,URA_FRAGILE_BREAK);break;
 case UR_FRAGILE_WRECK:if(!op_sync(w,a)||!vg_score(w,a,0x1000)||!stamp(w,a,URS_S29B380))return 0;ur_start(q,URA_FRAGILE_WRECK);break;
 case UR_BREAKABLE:setup(q,0x93,80,1,URA_BREAKABLE);break;
 case UR_HIT_STAGE:setup(q,0x7a,64,70,URA_HIT_STAGE);a->animation.freeze=6;break;
 case UR_SMALL:setup(q,0x94,80,40,URA_SMALL);break;
 case UR_SMALL_WRECK:ur_start(q,URA_SMALL_WRECK);break;
 case UR_RANKED:if(!op_sync(w,a)||!stamp(w,a,URS_S29B31A)||!rank(w,q,0))return 0;q->urban.margin=96;ur_start(q,URA_RANKED);break;
 case UR_MULTI:{unsigned v=q->urban.variant;if(v>4)return enemy_fault(w);q->urban.stage=v;ur_start(q,v==4?URA_MULTISTAGE4:URA_MULTISTAGE);a->palette=0x95;if(v!=4){if(!op_copy_sync(w,a))return 0;struct PlayerTransform t=a->transform;t.display_group=q->source_flags;transform_integer_offset(&t,64,0);if(!emitter59_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,0))return 0;/*Source writes variant to62 (unused contact-pointer byte), not98 parameter0. Keep rank params at allocatorFF. */}q->urban.margin=128;q->scale_x=q->scale_y=255;a->transform.facing=0;if(!rank(w,q,1))return 0;a->hit.hp=op_word(a->hit.hp+320);a->phase=0;if(v==4)ur_defense(q,65535);break;}
 case UR_MULTI2:case UR_MULTI3:case UR_MULTI_WRECK:if(q->urban.stage<0||q->urban.stage>=4)return enemy_fault(w);if(s==UR_MULTI_WRECK&&!vg_score(w,a,0x4000))return 0;ur_start_entry(q,urban_stages[s==UR_MULTI2?0:s==UR_MULTI3?1:2][q->urban.stage]);break;
 case UR_BLOCKER:a->palette=14;a->display_flags|=64;a->transform.priority=0xf000;q->urban.margin=96;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;
 if(s==UR_COMPOUND_DRAIN){if(!children(w,e))entity_delete(w->entities,e);return 1;}
 if(s==UR_BLOCKER)interaction_body_contact(w,a,&urban_blocker_contact);
 if(!op_sync(w,a))return 0;
 if(s==UR_COMPOUND_IDLE||s==UR_COMPOUND_WAIT){if(q->dead==(s==UR_COMPOUND_IDLE?1:2))ur_schedule(e,s==UR_COMPOUND_IDLE?UR_COMPOUND_WAIT:UR_COMPOUND_IDLE);if(q->captor_done==255)ur_schedule(e,UR_COMPOUND_DRAIN);if(!children(w,e))cull(w,e);return !w->fault;}
 if(s==UR_CAPTOR_TRIGGER){if(op_sx(w,a)<=224){struct PlayerTransform t=a->transform;t.display_group=q->source_flags;transform_integer_offset(&t,176,64);if(!captor_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,127,&t,0))return 0;entity_delete(w->entities,e);}return !w->fault;}
 if(s==UR_NEON_WAIT){if(op_sx(w,a)<=288){a->state=UR_NEON;if(!enter(w,e))return 0;return frame(w,e,UR_NEON);}return !w->fault;}
 if(s==UR_BUILDING||s==UR_BUILDING2||s==UR_WALL||s==UR_WALL2||s==UR_RANKED)w->enemies->intro_gate=0;
 if(s==UR_COLUMN||s==UR_NEON||s==UR_NEON_WRECK||s==UR_STALL||s==UR_STALL_WRECK)depth(w,a);
 if((s==UR_NEON||s==UR_NEON_WRECK)&&!neon(w,e))return 0;
 int done=s==UR_BLOCKER?0:ur_animate(w,e);
 if(s==UR_STALL_WRECK||s==UR_WALL_WRECK){entity_delete(w->entities,e);return !w->fault;}
 if(s==UR_FRAGILE_BREAK){if(done)ur_schedule(e,UR_FRAGILE_WRECK);}
 else if(s==UR_BUILDING){if(a->hit.last_kind!=5&&!op_hit(w,a))return 0;if(a->hit.last_kind==5||a->hit.hp<=60){if(!interaction_sound(w,0x1023))return 0;a->hit.flags&=247;if(!ur_pattern(w,e,URP_P29A278)||!ur_pattern(w,e,URP_P29A28A))return 0;ur_schedule(e,UR_BUILDING2);}}
 else if(s==UR_WALL||s==UR_MULTI||s==UR_MULTI2){if(s==UR_MULTI&&q->urban.stage==4){}else{if(!op_hit(w,a))return 0;int threshold=s==UR_WALL?160:s==UR_MULTI?212:106;if(a->hit.hp<=threshold){if(!interaction_sound(w,s==UR_WALL?0x102a:0x1027))return 0;if(s==UR_WALL){n=ur_pattern(w,e,URP_P29A2D2);if(!n)return 0;if(a->phase)transform_integer_offset(&n->payload.enemy.actor.transform,-48,0);}ur_schedule(e,s==UR_WALL?UR_WALL2:s==UR_MULTI?UR_MULTI2:UR_MULTI3);}}}
 else if(s==UR_BUILDING2||s==UR_COLUMN||s==UR_WALL2||s==UR_NEON||s==UR_STALL||s==UR_FRAGILE||s==UR_BREAKABLE||s==UR_HIT_STAGE||s==UR_SMALL||s==UR_RANKED||s==UR_MULTI3){
  if(s==UR_HIT_STAGE&&done)ur_start(q,URA_HIT_STAGE);
  if(s==UR_BREAKABLE)a->hit.flags&=247;else{int hit=a->hit.flags&8;if(!op_hit(w,a))return 0;if(s==UR_HIT_STAGE&&hit){if((uint16_t)q->urban.stage>=8)return enemy_fault(w);ur_start_entry(q,urban_hit[q->urban.stage]);}}
  if(a->hit.flags&1){if(s!=UR_RANKED)a->hit.flags&=254;switch(s){
   case UR_BUILDING2:if(!interaction_sound(w,0x1030)||!ur_pattern(w,e,URP_P29A278)||!ur_pattern(w,e,URP_P29A28A))return 0;n=ur_pattern(w,e,URP_P29A3CE);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xf000;ur_schedule(e,UR_BUILDING_WRECK);break;
   case UR_COLUMN:if(!vg_score(w,a,0x3000)||!interaction_sound(w,0x1023)||!stamp(w,a,URS_S29B368)||!burst(w,e,URP_P29A29C,URP_P29A2AE,URP_P29A3E0)||!patch(w,URV_P29A566))return 0;entity_delete(w->entities,e);return !w->fault;
   case UR_WALL2:if(!interaction_sound(w,0x102b))return 0;if(a->phase)transform_integer_offset(&a->transform,-48,0);n=ur_pattern(w,e,URP_P29A2C0);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xc000;n=ur_pattern(w,e,URP_P29A3F2);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xc000;ur_schedule(e,UR_WALL_WRECK);break;
   case UR_NEON:case UR_STALL:if(!interaction_sound(w,s==UR_NEON?0x102a:0x102c)||!burst(w,e,s==UR_NEON?URP_P29A2E4:URP_P29A32C,s==UR_NEON?URP_P29A2F6:URP_P29A33E,s==UR_NEON?URP_P29A404:URP_P29A428))return 0;ur_schedule(e,s==UR_NEON?UR_NEON_WRECK:UR_STALL_WRECK);break;
   case UR_FRAGILE:if(!ur_pattern(w,e,URP_P29A350)||!ur_pattern(w,e,URP_P29A43A))return 0;ur_schedule(e,UR_FRAGILE_BREAK);break;
   case UR_BREAKABLE:if(!interaction_sound(w,0x102e)||!stamp(w,a,URS_S29B358)||!burst(w,e,URP_P29A362,URP_P29A374,URP_P29A44C))return 0;entity_delete(w->entities,e);return !w->fault;
   case UR_HIT_STAGE:if(!vg_score(w,a,0x300)||!interaction_sound(w,0x1023)||!burst(w,e,URP_P29A386,URP_P29A398,URP_P29A45E))return 0;entity_delete(w->entities,e);return !w->fault;
   case UR_SMALL:if(!interaction_sound(w,0x102e)||!burst(w,e,URP_P29A3AA,URP_P29A3BC,URP_P29A470))return 0;ur_schedule(e,UR_SMALL_WRECK);break;
   case UR_RANKED:if(!vg_score(w,a,0x1000)||!interaction_sound(w,0x102b)||!stamp(w,a,URS_S29B32E)||!patch(w,URV_P29A606)||!burst(w,e,URP_P29A308,URP_P29A308,URP_P29A416))return 0;entity_delete(w->entities,e);return !w->fault;
   case UR_MULTI3:if(!interaction_sound(w,0x1035))return 0;ur_schedule(e,UR_MULTI_WRECK);break;
   default:return enemy_fault(w);
  }}
 }
 cull(w,e);return !w->fault;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;unsigned s=q->actor.state;if(q->entry){q->entry=0;if(!(s>=UR_BLINK_A?ur_fx_enter(w,e):enter(w,e)))return;}if(e->active&&!e->deleting){s=q->actor.state;if(s>=UR_BLINK_A)ur_fx_frame(w,e,s);else frame(w,e,s);}}
int urban_spawn_scene(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,enum UrbanEntry id,uint16_t config,uint8_t flags){static const uint8_t states[]={UR_BUILDING,UR_COLUMN,UR_COMPOUND,UR_WALL,UR_FRAGILE,UR_STALL,UR_HIT_STAGE,UR_BREAKABLE,UR_RANKED,UR_SMALL,UR_BLOCKER};if((unsigned)id>=URBAN_ENTRY_COUNT)return enemy_fault(w);unsigned s=id>=URBAN_MULTISTAGE0?UR_MULTI:states[id];struct NativeEntity*e=ur_allocate(w,p,o,t,s);if(!e)return 0;e->payload.enemy.urban.variant=id>=URBAN_MULTISTAGE0?id-URBAN_MULTISTAGE0:0;e->payload.enemy.urban.margin=config;e->payload.enemy.actor.display_flags|=flags;return 1;}
int urban_blink(struct WeaponWorld*w,struct EntityRef p){struct PlayerTransform t={.x=-256,.y=513*256,.priority=65535,.display_group=255};return ur_allocate(w,p,ENTITY_FIRST_CHILD,&t,UR_BLINK_A)!=0;}
struct NativeEntity*urban_debris_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned v){if(v>6){enemy_fault(w);return 0;}struct NativeEntity*e=ur_allocate(w,p,o,t,UR_DEBRIS);if(e)e->payload.enemy.urban.variant=v;return e;}
