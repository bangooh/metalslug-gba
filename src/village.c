#include "village_internal.h"
#include "airship.h"
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void vg_schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
void vg_start_entry(struct EnemyActor*q,unsigned n){animation_begin(&q->actor.animation,(uint16_t)n);q->actor.movement_flags|=32;}
void vg_start(struct EnemyActor*q,unsigned n){vg_start_entry(q,village_entries[n]);}
void vg_reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
void vg_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:village_bindings[n].defense;q->actor.defense_count=n==65535?0:village_bindings[n].count;}
void vg_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){village_bindings[n].attack,village_bindings[n].count};q->actor.attack=&q->attack_set;}}
uint16_t vg_random(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
uint16_t vg_masked(struct WeaponWorld*w,uint16_t n){if(!w->rng){enemy_fault(w);return 0;}return source_random_mask(w->rng,n);}
uint16_t vg_bounded(struct WeaponWorld*w,uint16_t n){if(!w->rng){enemy_fault(w);return 0;}return source_random(w->rng,n);}
int vg_cycle(struct WeaponWorld*w,uint16_t slot,uint16_t first,uint16_t period){return w->video&&w->video->palette_cycle&&w->video->palette_cycle(w->video->palette_context,slot,scene_palette_high(first),period,1)?1:enemy_fault(w);}
int vg_patch(struct WeaponWorld*w,unsigned n){return n<VGV_COUNT&&scene_video_patch(w->video,&village_patches[n])?1:enemy_fault(w);}
int vg_cull(struct WeaponWorld*w,struct NativeEntity*e){if(vg_sx(w,&e->payload.enemy.actor)<vg_word(-e->payload.enemy.village.margin)){entity_delete(w->entities,e);return 1;}return 0;}
int vg_hurt(struct WeaponWorld*w,struct WeaponActor*a){unsigned n=(uint16_t)(a->hit.last_kind-1);if(!(a->hit.flags&1)&&(a->hit.events&1)&&n<5&&village_hurt_sounds[n]!=65535)return interaction_sound(w,village_hurt_sounds[n]);return 1;}
int vg_score(struct WeaponWorld*w,struct WeaponActor*a,uint32_t n){return w->items&&item_score_add(w->items,a->score_owner,n)?1:enemy_fault(w);}
struct EnemyActor*vg_parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;}
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;uint8_t*p=(uint8_t*)&e->payload.enemy.village;for(unsigned i=0;i<sizeof(e->payload.enemy.village);i++)p[i]=255;vg_schedule(e,s);return e;}
struct NativeEntity*vg_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,enum EntityInsert order){struct PlayerTransform t=e->payload.enemy.actor.transform;t.display_group=e->payload.enemy.source_flags;return allocate(w,entity_ref(e),order,&t,s);}
struct NativeEntity*vg_pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned id){if(id>=VGP_COUNT){enemy_fault(w);return 0;}struct NativeEntity*n=vg_child(w,e,VG_PATTERN,ENTITY_LAST_CHILD);if(n)n->payload.enemy.village.pattern=(uint16_t)id;return n;}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;if(f==VGF_PHASE&&width==1)return c->e->payload.enemy.actor.phase;enemy_fault(c->w);return 0;}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;uint32_t old=read(ctx,f,width);if(op==AN_FIELD_AND)v&=old;else if(op==AN_FIELD_OR)v|=old;else if(op==AN_FIELD_XOR)v^=old;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);c->e->payload.enemy.actor.phase=(uint8_t)v;return !c->w->fault;}
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(village_bindings)/sizeof(*village_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)vg_defense(&c->e->payload.enemy,n);else if(kind==AN_ATTACK){vg_attack(&c->e->payload.enemy,n);hit_begin_attack(&c->e->payload.enemy.actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(n==VGC_4FA40||n==VGC_4FA58)return vg_cycle(c->w,17,n==VGC_4FA40?0xc2:0xc1,65535);if(n==VGC_53D2C){if(!q->dead){q->dead=255;struct PlayerTransform t=q->actor.transform;transform_integer_offset(&t,-24,48);return item_score_popup(c->w,entity_ref(c->e),&t,q->actor.score_owner,0,4,0);}return 1;}return enemy_fault(c->w);}
int vg_animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=field,.bind=bind,.sound=sound,.call=call};int r=animation_step(&a->animation,&village_program,&o);if(a->animation.fault)enemy_fault(w);return r;}
static int watch(const struct NativeEntity*e){return e->kind==ENTITY_ENEMY?e->payload.enemy.village.watch:-1;}
static void setup(struct EnemyActor*q,unsigned palette,int margin,int hp,unsigned graph){q->actor.palette=(uint16_t)palette;q->village.margin=(int16_t)margin;q->actor.hit.hp=(int16_t)hp;q->scale_x=q->scale_y=255;q->actor.transform.facing=0;vg_start(q,graph);}
static int stamp_house(struct WeaponWorld*w,struct WeaponActor*a){const int16_t*t=village_house_stamp;return w->interactions&&terrain_stamp(w->interactions->terrain,&a->transform,t[0],t[1],t[2],t[3],village_house_cells)?1:enemy_fault(w);}
static int crouch_pair(struct WeaponWorld*w,struct NativeEntity*e){for(unsigned i=0;i<2;i++){struct PlayerTransform t=e->payload.enemy.actor.transform;transform_integer_offset(&t,i?48:7,32);uint8_t params[8]={1,255,255,255,255,255,255,255};if(!airship_spawn_template(w,entity_ref(e),ENTITY_FIRST_CHILD,323+i,&t,params))return 0;}return 1;}
static int gate_patches(struct WeaponWorld*w,struct WeaponActor*a,int destroyed){unsigned base=a->transform.priority?(destroyed?VGV_P2986C0:VGV_P298684):(destroyed?VGV_P298648:VGV_P29860C);for(unsigned i=0;i<3;i++)if(!vg_patch(w,base+i))return 0;return 1;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state,v=q->village.variant;struct NativeEntity*n;
 switch(s){
 case VG_GROUND_STATIC:a->transform.priority=0x8000;vg_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;/* fall through */
 case VG_STATIC:q->scale_x=q->scale_y=255;a->transform.facing=0;a->state=VG_STATIC;break;
 case VG_LAMP:a->palette=0x41;q->scale_x=q->scale_y=255;a->transform.facing=0;vg_start(q,VGA_LAMP);a->state=VG_LAMP_IDLE;break;
 case VG_LAMP_IDLE:break;
 case VG_LAMP_HIT:vg_start(q,VGA_LAMP_HIT);break;
 case VG_SENSOR:q->village.margin=144;a->state=VG_SENSOR_IDLE;break;
 case VG_SENSOR_IDLE:break;
 case VG_SENSOR_ACTIVE:vg_start(q,VGA_SENSOR);break;
 case VG_BREAK:q->village.margin=64;a->hit.hp=10;q->dead=0;vg_start(q,a->transform.priority==0||a->transform.priority==6?VGA_BREAK_A:VGA_BREAK_B);break;
 case VG_SIGN:setup(q,0x33,32,1,VGA_SIGN);break;
 case VG_SIGN_BREAK:if(!interaction_sound(w,0x1029))return 0;vg_start(q,VGA_SIGN_BREAK);break;
 case VG_SIGN_WRECK:a->hit.flags&=247;vg_start(q,VGA_SIGN_WRECK);break;
 case VG_WALL:setup(q,0x32,32,60,VGA_WALL);break;
 case VG_WALL2:a->hit.hp=60;vg_start(q,VGA_WALL2);break;
 case VG_LARGE:{setup(q,0x31,16,100,VGA_LARGE);q->village.watch=0;struct PlayerTransform t=a->transform;transform_integer_offset(&t,8,0);if(!climb_actor_spawn_watched(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,129,watch))return 0;break;}
 case VG_LARGE_WRECK:if(!vg_score(w,a,0x1000))return 0;vg_defense(q,65535);q->village.watch=255;vg_start(q,VGA_LARGE_WRECK);break;
 case VG_EXPLOSIVE:setup(q,0x30,16,20,VGA_EXPLOSIVE);break;
 case VG_EXPLOSIVE_WRECK:if(!vg_score(w,a,0x500))return 0;vg_attack(q,65535);hit_begin_attack(&a->hit);vg_defense(q,65535);vg_start(q,VGA_EXPLOSIVE_WRECK);break;
 case VG_HOUSE:if(v>=6)return enemy_fault(w);q->village.patch=VGV_P2983F0+v;if(v==3||v==5){if(!crouch_pair(w,e)||!stamp_house(w,a)||!vg_patch(w,q->village.patch))return 0;entity_delete(w->entities,e);return 0;}vg_start(q,VGA_HOUSE0+v/2);q->village.margin=64;a->hit.hp=40;q->dead=0;break;
 case VG_HUT:if(v>=10)return enemy_fault(w);q->dead=(uint8_t)(v/2);q->village.patch=VGV_P298468+v;vg_start(q,VGA_HUT0+v/2);q->village.margin=64;a->hit.hp=40;break;
 case VG_TOWER:a->palette=0x36;q->village.margin=64;a->hit.hp=100;vg_start(q,VGA_TOWER);if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;q->village.watch=0;if(!mission_event_spawn(w,entity_ref(e),0xea3fa,watch))return 0;break;
 case VG_TOWER2:if(!vg_patch(w,VGV_P298530))return 0;a->hit.hp=100;vg_start(q,VGA_TOWER2);break;
 case VG_TOWER_WRECK:if(!vg_score(w,a,0x2000))return 0;q->village.watch=255;if(!vg_patch(w,VGV_P298544))return 0;vg_defense(q,65535);vg_start(q,VGA_TOWER_WRECK);break;
 case VG_GATE:a->palette=0x36;q->village.margin=192;a->hit.hp=40;vg_start(q,VGA_GATE);break;
 case VG_GATE2:if(!gate_patches(w,a,0))return 0;a->hit.hp=40;vg_start(q,VGA_GATE2);break;
 case VG_GATE_WRECK:if(!vg_score(w,a,0x2000)||!gate_patches(w,a,1))return 0;vg_defense(q,65535);vg_start(q,VGA_GATE_WRECK);break;
 case VG_SWING:a->palette=0x1ae;vg_reset(a);a->transform.priority=0;vg_start(q,VGA_SWING);q->dead=0;n=vg_child(w,e,VG_SWING_CHILD,ENTITY_FIRST_CHILD);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,96,0);break;
 case VG_SWING_CHILD:a->palette=0x1af;vg_reset(a);a->transform.priority=0xf000;vg_start(q,VGA_SWING_CHILD);break;
 case VG_MARKER:if(v>=7||!w->background_view_valid)return enemy_fault(w);a->transform.x=(int32_t)vg_word(village_marker_xy[v][0]-w->background_x_offset+w->camera_x)*256;a->transform.y=(int32_t)vg_word(village_marker_xy[v][1]-w->background_y_offset+w->camera_y)*256;vg_defense(q,65535);a->hit.hp=60;break;
 case VG_BLINK_A:case VG_BLINK_B:if(!vg_cycle(w,25,s==VG_BLINK_A?12:13,s==VG_BLINK_A?2:1)||!vg_cycle(w,27,s==VG_BLINK_A?14:15,s==VG_BLINK_A?2:1))return 0;q->village.timer=s==VG_BLINK_A?180:120;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int impact_puff(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMALL);if(!n)return 0;struct PlayerTransform*t=&n->payload.enemy.actor.transform;transform_integer_offset(t,vg_word(a->hit.impact_x-(t->x>>8)),vg_word(a->hit.impact_y-transform_y_up(t)));return 1;}
static int frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;int done=0;
 if(s==VG_BLINK_A||s==VG_BLINK_B){if(!w->mission_valid)return enemy_fault(w);if(!--q->village.timer)vg_schedule(e,s==VG_BLINK_A?VG_BLINK_B:VG_BLINK_A);if((int16_t)w->mission_progress>=320)entity_delete(w->entities,e);return !w->fault;}
 if(s==VG_TOWER||s==VG_TOWER2)w->enemies->intro_gate=0;
 if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
 if(s==VG_SWING_CHILD){if(!w->scene_runtime)return enemy_fault(w);const struct SceneMapPlane*p=&w->scene_runtime->maps.planes[SCENE_TILE_AUX];transform_integer_offset(&a->transform,vg_word(-p->dx),p->dy);done=vg_animate(w,e);if(done){struct EnemyActor*p=vg_parent(w,e);if(!p)return 0;if(p->actor.phase!=a->phase)vg_start(q,p->actor.phase?VGA_SWING_CHILD_LEFT:VGA_SWING_CHILD_RIGHT);a->phase=p->actor.phase;}return !w->fault;}
 if(s==VG_MARKER){if(vg_sx(w,a)<0){entity_delete_children(w->entities,e);entity_delete(w->entities,e);}return 1;}
 if(s==VG_TOWER||s==VG_TOWER2)interaction_body_contact(w,a,&village_tower_contact);
 if(s!=VG_SENSOR_IDLE)done=vg_animate(w,e);
 if(s==VG_SENSOR_IDLE){int p=enemy_nearest_x(w,a,&village_sensor);if(p>=0&&(w->players[p]->actor.action_flags&4))vg_schedule(e,VG_SENSOR_ACTIVE);}
 else if(s==VG_SENSOR_ACTIVE){if(done)vg_schedule(e,VG_SENSOR_IDLE);}
 else if(s==VG_LAMP_IDLE){if(a->hit.flags&8){if(!interaction_sound(w,0x10a9))return 0;if(!(vg_random(w)&3)){n=vg_child(w,e,VG_LAMP_SPARK,ENTITY_LAST_CHILD);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,24);}vg_schedule(e,VG_LAMP_HIT);}}
 else if(s==VG_LAMP_HIT){if(done){a->hit.flags&=247;vg_schedule(e,VG_LAMP_IDLE);}}
 else if(s==VG_SIGN){if(a->hit.flags&8){a->hit.flags&=247;if(!vg_score(w,a,0x500))return 0;vg_schedule(e,VG_SIGN_BREAK);}}
 else if(s==VG_SIGN_BREAK){if(done)vg_schedule(e,VG_SIGN_WRECK);}
 else if(s==VG_SIGN_WRECK){if(a->hit.flags&8){if(!interaction_sound(w,0x1029))return 0;vg_start(q,VGA_SIGN_WRECK);}a->hit.flags&=247;}
 else if(s==VG_WALL||s==VG_WALL2){if((s==VG_WALL||!(a->hit.flags&1))&&(a->hit.flags&8)){a->hit.flags&=247;if(!interaction_sound(w,0x10bb))return 0;a->animation.counter=(a->animation.counter+1)&3;n=vg_child(w,e,VG_CHUNK,ENTITY_LAST_CHILD);if(!n)return 0;n->payload.enemy.village.variant=0;if(!vg_score(w,a,0x10))return 0;if(!(a->hit.flags&8))vg_start(q,s==VG_WALL?VGA_WALL_HIT:VGA_WALL2_HIT);}if(s==VG_WALL&&(a->hit.flags&1)){a->hit.flags&=254;vg_schedule(e,VG_WALL2);}}
 else if(s==VG_SWING){a->hit.hp=32767;if(a->hit.flags&8){int16_t vx,vy;if(!vg_attacker_velocity(w,a,&vx,&vy))return 0;if(vx&&((a->phase&&vx<0)||(!a->phase&&vx>0)))vg_start(q,a->phase?VGA_SWING_LEFT:VGA_SWING_RIGHT);a->hit.flags&=247;}}
 else if(s==VG_BREAK){int broken=a->hit.flags&1;if(a->hit.flags&8){if(!vg_hurt(w,a))return 0;a->hit.flags&=247;if(a->hit.last_kind==2){q->dead=1;broken=1;}}if(broken){a->hit.flags&=254;a->hit.hp=30;if(q->dead==1){if(!interaction_sound(w,0x1027))return 0;q->village.timer=vg_masked(w,3)+1;do{if(!vg_child(w,e,VG_DROP,ENTITY_FIRST_CHILD))return 0;}while(--q->village.timer);}if(!vg_pattern(w,e,a->transform.priority==0||a->transform.priority==6?VGP_P298074:VGP_P298062))return 0;if(a->transform.priority>=7||q->dead>=4)return enemy_fault(w);unsigned patch=village_break_patches[a->transform.priority][q->dead];if(patch!=65535&&!vg_patch(w,patch))return 0;if(++q->dead==2){entity_delete(w->entities,e);return !w->fault;}}}
 else if(s==VG_GATE&&vg_sx(w,a)>144){a->hit.flags&=246;return !w->fault;}
 else if((s==VG_GATE||s==VG_GATE2)&&vg_sx(w,a)<-32){}
 else if(s==VG_LARGE||s==VG_EXPLOSIVE||s==VG_HOUSE||s==VG_HUT||s==VG_TOWER||s==VG_TOWER2||s==VG_GATE||s==VG_GATE2){
  if(a->hit.flags&8){if(!vg_hurt(w,a))return 0;a->hit.flags&=247;if(s==VG_LARGE||s==VG_EXPLOSIVE){if(!impact_puff(w,e))return 0;if(s==VG_LARGE&&(w->frame_phase&1)){n=vg_child(w,e,VG_FLYING,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.village.variant=0;}}}
  if(a->hit.flags&1){
   if(s==VG_LARGE||s==VG_EXPLOSIVE){a->hit.flags&=254;if(!interaction_sound(w,s==VG_LARGE?0x1024:0x1076)||!vg_pattern(w,e,VGP_P297FE4))return 0;if(s==VG_EXPLOSIVE){vg_attack(q,VGB_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;}vg_schedule(e,s==VG_LARGE?VG_LARGE_WRECK:VG_EXPLOSIVE_WRECK);}
   else if(s==VG_HOUSE||s==VG_HUT){static const uint16_t sounds[]={0x102c,0x102c,0x1023,0x1035,0x1023};if(!interaction_sound(w,s==VG_HOUSE?0x1035:sounds[q->dead])||!vg_score(w,a,s==VG_HOUSE?0x300:0x500))return 0;vg_defense(q,65535);if(s==VG_HOUSE){if(!crouch_pair(w,e)||!stamp_house(w,a))return 0;transform_integer_offset(&a->transform,0,-16);if(!vg_pattern(w,e,VGP_P297FC0)||!vg_pattern(w,e,VGP_P297FD2)||!vg_pattern(w,e,VGP_P298086))return 0;}else{transform_integer_offset(&a->transform,16,0);if(!vg_pattern(w,e,VGP_P297FF6)||!vg_pattern(w,e,VGP_P298008))return 0;}if(!vg_patch(w,q->village.patch))return 0;entity_delete(w->entities,e);return !w->fault;}
   else{int gate=s==VG_GATE||s==VG_GATE2,second=s==VG_TOWER2||s==VG_GATE2;if(!second||gate)a->hit.flags&=254;if(!interaction_sound(w,second?0x1030:0x1023)||!vg_pattern(w,e,gate?VGP_P29803E:VGP_P29801A)||!vg_pattern(w,e,gate?VGP_P298050:VGP_P29802C))return 0;if(second&&!vg_pattern(w,e,gate?VGP_P2980EA:VGP_P2980D8))return 0;vg_schedule(e,gate?(second?VG_GATE_WRECK:VG_GATE2):(second?VG_TOWER_WRECK:VG_TOWER2));}
  }
 }
 if(vg_cull(w,e)&&s==VG_LARGE)q->village.watch=255;return !w->fault;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;if(q->actor.state==VG_DELETE){entity_delete(p,e);return;}if(q->entry){q->entry=0;if(!(q->actor.state>=VG_PATTERN?vg_effects_enter(w,e):enter(w,e)))return;}if(e->deleting||!e->active)return;unsigned s=q->actor.state;if(s>=VG_PATTERN)vg_effects_frame(w,e,s);else frame(w,e,s);}
int village_spawn_scene(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert order,const struct PlayerTransform*t,enum VillageEntry id,uint16_t config,uint8_t flags,uint16_t palette,uint16_t graph){
 if((unsigned)id>=VILLAGE_ENTRY_COUNT)return enemy_fault(w);unsigned state,variant=0;
 if(id>=VILLAGE_HOUSE0&&id<=VILLAGE_HOUSE5){state=VG_HOUSE;variant=id-VILLAGE_HOUSE0;}
 else if(id>=VILLAGE_HUT0&&id<=VILLAGE_HUT9){state=VG_HUT;variant=id-VILLAGE_HUT0;}
 else if(id>=VILLAGE_MARKER0&&id<=VILLAGE_MARKER6){state=VG_MARKER;variant=id-VILLAGE_MARKER0;}
 else{switch(id){case VILLAGE_STATIC:state=VG_STATIC;break;case VILLAGE_GROUND_STATIC:state=VG_GROUND_STATIC;break;case VILLAGE_LAMP:state=VG_LAMP;break;case VILLAGE_SENSOR:state=VG_SENSOR;break;case VILLAGE_BREAKABLE:state=VG_BREAK;break;case VILLAGE_SIGN:state=VG_SIGN;break;case VILLAGE_WALL:state=VG_WALL;break;case VILLAGE_LARGE:state=VG_LARGE;break;case VILLAGE_EXPLOSIVE:state=VG_EXPLOSIVE;break;case VILLAGE_TOWER:state=VG_TOWER;break;case VILLAGE_GATE:state=VG_GATE;break;case VILLAGE_SWING:state=VG_SWING;break;case VILLAGE_BLINK:state=VG_BLINK_A;break;default:return enemy_fault(w);}}
 struct NativeEntity*e=allocate(w,p,order,t,state);if(!e)return 0;struct EnemyActor*q=&e->payload.enemy;q->village.variant=variant;q->village.margin=(int16_t)config;q->actor.display_flags|=flags;
 if(palette!=65535)q->actor.palette=palette;if(graph!=65535)vg_start_entry(q,graph);return 1;
}
