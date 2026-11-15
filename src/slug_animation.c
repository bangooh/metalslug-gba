#include "slug_animation.h"
static int fail(struct SlugSharedState*s,unsigned code){if(!s->fault)s->fault=(uint8_t)code;return 0;}
static int noise(struct SlugAnimationContext*c,uint16_t command,int stop){
 struct WeaponWorld*w=c->weapons;
 return w&&w->sound&&w->sound(w->scene,command,stop)?1:fail(c->slug,5);
}
static void synchronize(struct WeaponActor*a){
 a->hit.x=(int16_t)(a->transform.x>>8);a->hit.y=transform_y_up(&a->transform);a->hit.flip=a->transform.facing;
}
static int begin(struct SlugSharedState*s,uint16_t entry,uint8_t frame){
 if(s->fault)return 0;s->frame_index=frame;
 if(entry==65535)return 1; /* source -1: keep the previous stream */
 if(entry>=slug_animation_program.command_count)return fail(s,4);
 animation_begin(&s->actor.animation,entry);s->actor.movement_flags|=32;return 1;
}
int slug_animation_select(struct SlugSharedState*s,enum SlugAnimationTableId table,unsigned choice,uint8_t frame){
 if((unsigned)table>=SANI_TABLE_COUNT||choice>=slug_animation_tables[table].count)return fail(s,4);
 return begin(s,slug_animation_tables[table].entries[choice],frame);
}
int slug_animation_turn(struct SlugSharedState*s,unsigned alternate,unsigned slope,unsigned phase,uint8_t frame){
 if(alternate>1||slope>=5||phase>=8)return fail(s,4);
 return begin(s,slug_turn_entries[alternate][slope][phase],frame);
}
int slug_set_defense(struct SlugSharedState*s,unsigned binding){
 if(binding>=SLUG_DEFENSE_BINDINGS)return fail(s,4);
 s->actor.defense=slug_defense_sets[binding].records;s->actor.defense_count=slug_defense_sets[binding].count;return 1;
}
int slug_set_defense_table(struct SlugSharedState*s,enum SlugDefenseTableId table,unsigned slope){
 if((unsigned)table>=SDEFT_COUNT||slope>=5)return fail(s,4);
 return slug_set_defense(s,slug_defense_tables[table][slope]);
}
int slug_set_attack(struct SlugSharedState*s,unsigned binding){
 if(binding>=SATT_COUNT)return fail(s,4);s->actor.attack=&slug_attack_sets[binding];return 1;
}
const struct SlugMount*slug_mount(struct SlugSharedState*s){
 if(!s->mount_valid||s->mount_index>=SLUG_MOUNT_COUNT){fail(s,4);return 0;}
 return &slug_mounts[s->mount_index];
}
static uint32_t read_field(void*user,unsigned field,unsigned width){
 struct SlugSharedState*s=((struct SlugAnimationContext*)user)->slug;
 if(width!=1){fail(s,4);return 0;}
 switch(field){case SAF_ACTION_FLAGS:return s->action_flags;case SAF_POSE:return s->actor.animation.pose;
  case SAF_ACCEL_TIMER:return s->accel_timer;default:fail(s,4);return 0;}
}
static int write_field(void*user,unsigned field,unsigned width,enum AnimationFieldOperation op,uint32_t value){
 struct SlugSharedState*s=((struct SlugAnimationContext*)user)->slug;
 uint32_t old=read_field(user,field,width);if(s->fault)return 0;
 switch(op){case AN_FIELD_SET:break;case AN_FIELD_AND:value&=old;break;case AN_FIELD_OR:value|=old;break;
  case AN_FIELD_XOR:value^=old;break;default:return fail(s,4);}
 switch(field){case SAF_ACTION_FLAGS:s->action_flags=(uint8_t)value;break;
  case SAF_POSE:s->actor.animation.pose=(uint8_t)value;break;
  case SAF_ACCEL_TIMER:s->accel_timer=(uint8_t)value;break;default:return fail(s,4);}return 1;
}
static int bind(void*user,unsigned kind,uint32_t value){
 struct SlugSharedState*s=((struct SlugAnimationContext*)user)->slug;
 if(kind==AN_DEFENSE){if(value==0xffffffffu){s->actor.defense=0;s->actor.defense_count=0;return 1;}
  return slug_set_defense(s,value);}
 if(kind==AN_PARAMETERS){
  if(value>=SLUG_MOUNT_COUNT)return fail(s,4);s->mount_index=(uint16_t)value;s->mount_valid=1;return 1;
 }
 return fail(s,4);
}
static void ephemeral_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 (void)world;e->payload.weapon.active=0;entity_delete(p,e); /* $031D6A/$031E38 -> $000518 */
}
static int bubble_sound(void*user,uint16_t command,int stop){
 struct WeaponWorld*w=user;return w->sound&&w->sound(w->scene,command,stop);
}
static void bubble_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct WeaponActor*a=&e->payload.weapon;if(w->fault)return;
 if(a->state==2){a->active=0;entity_delete(p,e);return;}
 struct WeaponActor*parent=entity_weapon_actor(entity_get(p,e->parent));
 if(!parent){w->fault=22;return;}
 if(!a->state){
  a->palette=0x1b0;a->attached_offset=56;a->transform.priority=65535;a->display_flags|=64;
  animation_begin(&a->animation,weapon_animation_entries[WANI_SLUG_BUBBLE]);a->movement_flags|=32;a->state=1;
 }
 /* $05E4CA reads the parent even while its deletion waits for this child.
  * Integer follow leaves fractions intact; animation can change $5C. */
 transform_integer_offset(&a->transform,
  (int16_t)((parent->transform.x>>8)-(a->transform.x>>8)),
  (int16_t)(transform_y_up(&parent->transform)+a->attached_offset-transform_y_up(&a->transform)));
 a->transform.priority=parent->transform.priority;
 struct WeaponAnimationContext c={.actor=a,.world=w,.sound=bubble_sound};
 if(weapon_animation_step(&c))a->state=2; /* $0320CC schedules $0321BC for next tick. */
 if(c.fault||a->animation.fault)w->fault=22;
}
static int effect(struct SlugAnimationContext*c,int bubble,int dx,int dy,int copy){
 struct WeaponWorld*w=c->weapons;
 if(!w||!w->entities||w->fault)return fail(c->slug,6);
 struct NativeEntity*e=entity_spawn(w->entities,c->entity,ENTITY_EFFECT,ENTITY_FIRST_CHILD,bubble?bubble_tick:ephemeral_tick);
 if(!e)return fail(c->slug,6);
 struct WeaponActor*a=&e->payload.weapon;
 *a=(struct WeaponActor){.active=1,.owner=255,.damage_owner=255,.score_owner=255,.platform_ids=255,.ground_slope=-1,.ground_tag=-1,
  .phase=255,.ttl=255,.raw_surface=255,.origin_y_up=-1,.trail_drawing=65535,
  .hit={.id=e->index,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
  .animation={.drawing=65535,.cursor=65535,.last_draw_cursor=65535}};
 if(copy){a->transform=c->slug->actor.transform;transform_integer_copy(&a->transform,&c->slug->actor.transform);
  transform_integer_offset(&a->transform,dx,dy);if(copy==2)a->transform.facing^=1;}
 return 1;
}
static int callback(void*user,unsigned kind,struct Animation*animation){
 (void)animation;struct SlugAnimationContext*c=user;struct SlugSharedState*s=c->slug;
 switch(kind){
  case SAC_DUST_FLAT:return effect(c,0,-32,0,1);
  case SAC_DUST_DOWN:return effect(c,0,-32,-8,1);
  case SAC_DUST_UP:return effect(c,0,-32,8,1);
  case SAC_DUST_STEEP_DOWN:return effect(c,0,-19,-12,1);
  case SAC_DUST_STEEP_UP:return effect(c,0,-19,12,1);
  case SAC_SPARK:case SAC_SPARK_LEFT:case SAC_SPARK_RIGHT:
   if(!(s->action_flags&2))return 1;s->action_flags&=(uint8_t)~2u;
   return effect(c,0,kind==SAC_SPARK_LEFT?-1:1,-3,kind!=SAC_SPARK);
  case SAC_SOUND_1092:case SAC_SOUND_108F:
   if(!s->sound_state)return 1;if(!noise(c,kind==SAC_SOUND_1092?0x1092:0x108f,0))return 0;
   s->sound_state=0;return 1;
  case SAC_TERRAIN_SOUND_A:case SAC_TERRAIN_SOUND_B:{
   if(!c->terrain)return fail(s,3);
   struct WeaponActor*a=&s->actor;struct TerrainMotion m={.transform=&a->transform,.hit=&a->hit,
    .collision_flags=s->lifecycle_flags,.platform_ids=a->platform_ids};
   /* $027DB2 masks D0 to six shape bits; the raw material byte is D7.
    * $027EBA preserves that D0 (or substitutes 17 for a platform).
    * These callbacks AND D0 with $C0, so their special-material branches
    * are unreachable. Still run the probe for event/platform side effects. */
   int contact=terrain_foot_contact(c->terrain,&m);a->platform_ids=m.platform_ids;
   if(contact<0)return fail(s,3);
   return noise(c,kind==SAC_TERRAIN_SOUND_A?0x1090:0x1091,0);
  }
  case SAC_DESTROYED_BUBBLE:
   if(!(s->actor.hit.flags&1))return 1;
   return noise(c,0x10e9,0)&&effect(c,1,0,0,0);
  default:return fail(s,4);
 }
}
int slug_animation_acceleration_dust(struct SlugAnimationContext*c,int left){
 return effect(c,0,left?32:-32,0,left?2:1);
}
static int sound(void*user,uint16_t command,int stop){return noise(user,command,stop);}
int slug_animation_step(struct SlugAnimationContext*c){
 struct SlugSharedState*s=c->slug;if(s->fault)return 0;synchronize(&s->actor);
 const struct AnimationOwner owner={.context=c,.read=read_field,.write=write_field,.bind=bind,.call=callback,.sound=sound};
 int complete=animation_step(&s->actor.animation,&slug_animation_program,&owner);
 if(s->actor.animation.fault)fail(s,4);return complete;
}
