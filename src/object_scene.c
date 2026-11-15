#include "object_scene.h"
#include "source_visit_inline.h"
#include "play_world.h"
#include "scene_runtime.h"
#include "session.h"
#include "display_scale.h"
#include "../m1_native/runtime.h"
#include "../generated/player-programs.h"
#include "../generated/approved-player-data.h"
#include <stddef.h>
_Static_assert(sizeof(struct NativeObjectScene)<53248,"captured OBJ frame RAM budget");
_Static_assert(offsetof(struct NativeEntity,payload)==0,"owner lookup layout");
static int fail(struct NativeObjectScene*s,unsigned fault){
 if(s&&!s->fault)s->fault=fault;return 0;
}
static struct NativeEntity*owner(struct NativeObjectScene*s,struct Animation*a){
 struct NativePlayWorld*w=s->world;
 if(a==&w->slug.actor.animation){
  unsigned i=w->slug.actor.hit.id;if(i>=ENTITY_CAPACITY)return 0;
  struct NativeEntity*e=&w->entities.nodes[i];
  return e->active&&e->kind==ENTITY_SLUG&&e->payload.slug==&w->slug?e:0;
 }
 uintptr_t base=(uintptr_t)w->entities.nodes,at=(uintptr_t)a;
 if((at&3)||at<base||at-base>sizeof(w->entities.nodes)-sizeof(struct NativeEntity))return 0;
 /* Bounds first; never read the trailing index outside the pool. Validate
  * the slot identity before treating an arbitrary Animation as an entity. */
 struct NativeEntity*e=(struct NativeEntity*)at;
 unsigned index=e->index;if(index>=ENTITY_CAPACITY||&w->entities.nodes[index]!=e)return 0;
 if(!e->active||e->deleting)return 0;
 if(e->kind==ENTITY_PLAYER&&a==&e->payload.player.actor.animation)return e;
 if(e->kind==ENTITY_PLAYER_ARM&&a==&e->payload.arm.animation)return e;
 struct WeaponActor*q=entity_weapon_actor(e);return q&&a==&q->animation?e:0;
}
struct Count {struct NativeObjectScene*s;unsigned forced,admitted,reflection,part;};
static int count_part(void*ctx,unsigned map,const struct NativeObjectPlacement*p){
 struct Count*c=ctx;struct NativeSourceStripBudget*q=&c->s->strips;
 unsigned before=q->records,keys=q->keys;
 int n=native_source_strips_map(q,map,p,c->forced);if(n<0)return 0;c->admitted+=n;
 if(!c->reflection||(c->part&1)){
  if(c->s->part_count<NATIVE_PART_CACHE_LIMIT){
   unsigned strips=before<380&&keys<210&&q->records<380?q->records-before:65535;
   c->s->parts[c->s->part_count++]=(struct NativeCapturedPart){map,p->x,p->y,(~p->order)>>16,strips,p->flip,0};
  }
 }
 c->part++;return 1;
}
/* Omitted sparse decorations still consume exact source strips, because
 * gameplay spawn admission reads that budget. Do not build discarded parts. */
static int count_only(void*ctx,unsigned map,const struct NativeObjectPlacement*p){
 struct Count*c=ctx;
 int n=native_source_strips_map(&c->s->strips,map,p,c->forced);
 if(n<0)return 0;c->admitted+=n;return 1;
}
static int reset(void*ctx,const struct SceneSpritePools*p){
 struct NativeObjectScene*s=ctx;if(!s||s->fault||!p||!p->valid)return 0;
 native_source_strips_begin(&s->strips);s->part_count=s->count=s->submitted_maps=0;s->budget_ready=0;s->latched_valid=0;
 /*5A88A clears previous free counts, not to the new pool capacity. */
 s->world->weapons.sprite_budget=(struct WeaponSpriteBudget){0,p->lower_end,p->upper_begin,1};
 return 1;
}
static int begin(void*ctx){
 struct NativeObjectScene*s=ctx;
 if(!s||s->fault||s->collecting)return 0;
 s->part_count=s->count=s->submitted_maps=0;s->collecting=1;s->resolved=0;
 native_source_strips_begin(&s->strips);return 1;
}
static inline __attribute__((always_inline)) int draw_body(void*ctx,struct Animation*a,enum AnimationDrawMode mode){
 struct NativeObjectScene*s=ctx;
 if(!s||s->fault||!s->collecting)return fail(s,NOS_STATE);
 if((unsigned)mode>AN_OUTPUT_PROJECTED_FORCED)return fail(s,NOS_DRAWING);
 struct NativeEntity*e=owner(s,a);if(!e)return fail(s,NOS_OWNER);
 struct NativePlayWorld*w=s->world;struct PlayerTransform*t;
 uint8_t *movement,*display,damage,hit,collision=0,hit_palette,flags=0;
 unsigned fixed=65535;uint16_t source;struct NativeSourceDrawPose pose={.zoom_x=255,.zoom_y=255,.direct=mode!=AN_OUTPUT_SCRIPT};
 if(e->kind==ENTITY_PLAYER||e->kind==ENTITY_PLAYER_ARM){
  struct PlayerActionLayer*q=e->kind==ENTITY_PLAYER?&e->payload.player.actor:&e->payload.arm;
  t=&q->transform;source=q->palette;movement=&q->movement_flags;display=&q->display_flags;
  damage=q->damage_timer;hit=q->hit.type_flags;collision=q->hit.flags;hit_palette=q->hit_palette;
 }else{
  struct WeaponActor*q=entity_weapon_actor(e);if(!q)return fail(s,NOS_OWNER);
  t=&q->transform;source=q->palette;movement=&q->movement_flags;display=&q->display_flags;
  damage=q->damage_timer;hit=q->hit.type_flags;collision=q->hit.flags;hit_palette=q->hit_palette;
  if(q->fixed_palette)fixed=q->fixed_palette;
  if(e->kind==ENTITY_ENEMY){pose.zoom_x=e->payload.enemy.scale_x;pose.zoom_y=e->payload.enemy.scale_y;flags=e->payload.enemy.private_palette_flags;}
  else if(e->kind==ENTITY_ITEM){pose.zoom_x=e->payload.item.scale_x;pose.zoom_y=e->payload.item.scale_y;}
  else if(e->kind==ENTITY_INTERACTION){pose.zoom_x=e->payload.interaction.scale_x;pose.zoom_y=e->payload.interaction.scale_y;}
  else if(e->kind==ENTITY_SLUG){pose.zoom_x=w->slug.scale_x_y>>8;pose.zoom_y=w->slug.scale_x_y;}
 }
 /* An unallocated source14 word is FFFF; 5AA4E LSL.W #8 selects
  * physical paletteFF. Sensor4F46A deliberately never allocates a palette. */
 if(source==65535&&fixed==65535)fixed=255;
 int ordinary=mode==AN_OUTPUT_SCRIPT&&!(a->drawing_flags&1);
 if(ordinary&&!native_source_blink(a,damage,hit,w->weapons.frame_phase,movement))return 1;
 if(ordinary){
  if((*movement&129)&&!(collision&1)&&hit_palette){fixed=hit_palette;*movement|=128;}
  /*2F84A reads the Slug status8D.bit2, independently of collision13.*/
  if(e->kind==ENTITY_SLUG&&(w->weapons.frame_phase&16)&&(hit&1)&&(w->slug.status_flags&4)){
   if(!hit_palette)return fail(s,NOS_PALETTE);fixed=hit_palette;
  }
  /* DRAW clears the one-shot character selector. Character-specific parts
   * were resolved by the offline player graph compiler. */
  *display&=(uint8_t)~32;
 }
 native_source_anchor(&pose,t,w->weapons.camera_x,w->weapons.camera_y,pose.direct);
 pose.priority=t->priority;pose.flip=t->facing;pose.display_flags=*display;pose.sequence=s->submitted_maps;
 if((mode==AN_OUTPUT_PROJECTED||mode==AN_OUTPUT_PROJECTED_FORCED)&&(*display&4)){
  if(!s->config.projection_valid)return fail(s,NOS_CAMERA);
  native_source_project(&pose,s->config.pivot_x,s->config.pivot_y);
 }
 if(w->weapons.scene_number==0){
  if(!w->weapons.secondary_view_valid)return fail(s,NOS_CAMERA);
  if(w->weapons.secondary_x_offset<=0x780){
   uint8_t raw;
   if(!w->terrain.cell||!w->terrain.cell(w->terrain.map,(int16_t)(t->x>>8),
    (int16_t)(512-transform_y_up(t)-8),&raw))return fail(s,NOS_CAMERA);
   native_source_reflection(&pose,0,w->weapons.secondary_x_offset,w->weapons.secondary_y_offset,raw);
  }
 }
 unsigned forced=mode==AN_OUTPUT_DIRECT_FORCED||mode==AN_OUTPUT_PROJECTED_FORCED||
  (mode==AN_OUTPUT_SCRIPT&&(*display&64));
 const struct NativeDrawing*drawing;if(!native_drawing_resolve(a,&drawing))return fail(s,NOS_DRAWING);
 unsigned first=s->part_count;
 uint16_t count;struct Count counter={s,forced,0,pose.reflection,0};
 unsigned hidden_sparse=(e->cosmetic&ENTITY_COSMETIC_SPARSE)&&(e->index&3);
 if(hidden_sparse){
  if(!native_source_visit_inline(drawing,a->drawing_flags,&pose,count_only,&counter,&count))return fail(s,NOS_DRAWING);
 }else if(!native_source_visit_inline(drawing,a->drawing_flags,&pose,count_part,&counter,&count))return fail(s,NOS_DRAWING);
 s->submitted_maps+=count;
 if(!counter.admitted){s->part_count=first;return 1;}
 /* Keep original simulation/RNG/lifetimes and source strip accounting.
  * Only one in four cosmetic actors is captured, stable for its lifetime. */
 if(hidden_sparse){s->part_count=first;return 1;}
 if(s->count>=NATIVE_DRAW_CAPTURE_LIMIT)return fail(s,NOS_QUEUE);
 struct NativeObjectDraw*d=&s->draws[s->count];
 unsigned parts=count/(pose.reflection?2:1);
 int cached=!pose.reflection&&parts<=NATIVE_PART_CACHE_LIMIT-first;
 if(!cached)s->part_count=first;
 *d=(struct NativeObjectDraw){.drawing=drawing,.source_drawing=a->drawing,.last_draw_cursor=a->last_draw_cursor,
  .drawing_flags=a->drawing_flags,.part_first=cached?first:255,.part_count=cached?parts:0,.cosmetic=e->cosmetic,.pose=pose,
  .forced_pool=forced,.approved_frame=65535};
 /* Preserve water reflection; reflected actors use the complete source visitor. */
 /* Layer 3 stores a signed presentation offset in the existing player
  * union fields, avoiding any growth of the 210-entry capture buffer. */
 if(e->kind==ENTITY_ITEM&&e->payload.item.score_spacing_x){d->player_layer=3;d->player_owner=(uint16_t)(int16_t)e->payload.item.score_spacing_x;}
 d->palette_protected=e->kind==ENTITY_PLAYER||e->kind==ENTITY_PLAYER_ARM||e->kind==ENTITY_PROJECTILE||e->kind==ENTITY_SLUG||e->kind==ENTITY_SLUG_AUX||e->kind==ENTITY_ITEM||e->kind==ENTITY_INTERACTION;
 if(a->drawing_program==&player_animation_program&&e->kind==ENTITY_PLAYER&&e->payload.player.character==0){d->player_owner=e->index;d->player_layer=1;}
 if(a->drawing_program==&player_animation_program&&e->kind==ENTITY_PLAYER_ARM){struct NativeEntity*parent=entity_get(&w->entities,e->parent);if(parent&&parent->kind==ENTITY_PLAYER&&parent->payload.player.character==0){d->player_owner=parent->index;d->player_layer=2;}}
 if(!native_palette_capture(s->palettes,entity_ref(e),source,flags,fixed,&d->palette))return fail(s,NOS_PALETTE);
 s->count++;return 1;
}
/* v170: native missions run a copy of the hot draw path from the .m1hot
 * IWRAM overlay; other scenes (legacy/opening overlays) use the ROM copy. */
#if defined(__arm__)
__attribute__((section(".m1hot.objdraw"),noinline,long_call)) static int draw_m1(void*ctx,struct Animation*a,enum AnimationDrawMode mode){return draw_body(ctx,a,mode);}
#endif
static int draw(void*ctx,struct Animation*a,enum AnimationDrawMode mode){
#if defined(__arm__)
 if(m1_overlay_kind==1)return draw_m1(ctx,a,mode);
#endif
 return draw_body(ctx,a,mode);
}
static int end(void*ctx){
 struct NativeObjectScene*s=ctx;if(!s||s->fault||!s->collecting)return fail(s,NOS_STATE);
 for(unsigned i=0;i<s->count;i++)if(!native_palette_resolve_capture(s->palettes,&s->draws[i].palette))return fail(s,NOS_PALETTE);
 s->collecting=0;s->resolved=1;return 1;
}
static void approved(struct NativeObjectScene*s){
 for(unsigned i=0;i<s->count;i++){
  struct NativeObjectDraw*a=&s->draws[i];if(a->player_layer!=2)continue;
  for(unsigned frame=0;frame<35;frame++){
   const struct NativeApprovedPlayer*r=&native_approved_players[frame];
   if(a->source_drawing!=r->upper||a->last_draw_cursor!=r->cursor)continue;
   for(unsigned j=0;j<s->count;j++){
    struct NativeObjectDraw*b=&s->draws[j];
    if(b->player_layer!=1||b->player_owner!=a->player_owner||b->approved_frame!=65535||b->source_drawing!=r->lower)continue;
    if(a->pose.x!=b->pose.x||a->pose.y!=b->pose.y-r->offset_y||a->pose.flip!=b->pose.flip||a->pose.reflection!=b->pose.reflection||a->pose.reflection_y!=b->pose.reflection_y||a->forced_pool!=b->forced_pool)continue;
    unsigned color;for(color=0;color<16;color++)if(a->palette.state.colors[color]!=b->palette.state.colors[color])break;
    if(color<16)continue;
    b->approved_frame=frame;a->approved_skip=1;break;
   }
  }
 }
}
struct Emit {struct NativeObjectScene*scene;struct NativeSourceStripBudget*budget;struct NativeObjectDraw*draw;uint8_t forced,skip_cosmetics;uint16_t cached_strips;};
static int emit(void*ctx,unsigned map,const struct NativeObjectPlacement*p){
 struct Emit*c=ctx;
 int admitted=c->cached_strips==65535?native_source_strips_map(c->budget,map,p,c->forced):native_source_strips_cached(c->budget,c->cached_strips,c->forced);
 if(admitted<0)return fail(c->scene,NOS_QUEUE);if(!admitted)return 1;
 if(c->draw->approved_skip||(c->skip_cosmetics&c->draw->cosmetic))return 1;
 struct NativeObjectPlacement place=*p;
 if(c->draw->player_layer==3){
  int pixel=native_screen_x(place.x)+(int16_t)c->draw->player_owner;
  /* Invert floor(3*x/4) exactly, including negative screen coordinates. */
  int n=pixel*4;place.x=n>=0?(n+2)/3:n/3;
 }
 if(!c->scene->config.layer(c->scene->config.context,(uint16_t)((~p->order)>>16),c->forced,&place.priority)||place.priority>3)return fail(c->scene,NOS_LAYER);
 /* Only final results borrow affine BG2/3. Tutorial scene13 and the tiny
  * zoom variants retain their original OBJ path and strip accounting. */
 if(map==5026&&c->scene->world->session&&c->scene->world->session->state==SESSION_FINAL&&place.zoom_x>=2&&place.zoom_y>=2)return 1;
 c->scene->frame.palette_exact=c->draw->palette_protected;
 if(c->draw->approved_frame!=65535){if(!native_object_approved(&c->scene->frame,native_approved_player_pixels+2048*c->draw->approved_frame,&place))return fail(c->scene,NOS_HARDWARE);}
 else if(!native_object_map(&c->scene->frame,map,&place))return fail(c->scene,NOS_HARDWARE);
 c->scene->frame.palette_exact=0;
 return 1;
}
int native_object_scene_append_budgeted(struct NativeObjectScene*s,int skip_cosmetics){
 if(!s||s->fault||s->collecting||!s->resolved)return fail(s,NOS_STATE);
 approved(s);s->cosmetic_omitted=0;
 /* Pressure retries must allocate immutable player/projectile colors first.
  * Source strip admission and final front-to-back ordering remain unchanged. */
 if(skip_cosmetics&&s->world->weapons.scene_number==5)for(unsigned i=0;i<s->count;i++){
  struct NativeObjectDraw*d=&s->draws[i];
  if(d->palette_protected&&!d->approved_skip&&!(skip_cosmetics&d->cosmetic))
   if(!native_object_reserve_exact_palette(&s->frame,d->palette.state.colors))return fail(s,NOS_HARDWARE);
 }

 struct NativeSourceStripBudget budget;native_source_strips_begin(&budget);
 for(unsigned i=0;i<s->count;i++){
  struct NativeObjectDraw*d=&s->draws[i];struct Emit ctx={s,&budget,d,d->forced_pool,skip_cosmetics,65535};uint16_t count;
  if(skip_cosmetics&d->cosmetic)s->cosmetic_omitted++;
  d->pose.palette=d->palette.state.colors;
  if(d->part_first!=255){
   for(unsigned j=0;j<d->part_count;j++){
    const struct NativeCapturedPart*c=&s->parts[d->part_first+j];
    struct NativeObjectPlacement place={.x=c->x,.y=c->y,.order=~(((uint32_t)c->priority<<16)|(d->pose.sequence+j)),
     .palette=d->pose.palette,.flip=c->flip,.priority=d->pose.gba_priority,.zoom_x=d->pose.zoom_x,.zoom_y=d->pose.zoom_y};
    ctx.cached_strips=c->strips;if(!emit(&ctx,c->map,&place))return fail(s,NOS_DRAWING);
   }
  }else if(!native_source_visit_resolved(d->drawing,d->drawing_flags,&d->pose,emit,&ctx,&count))return fail(s,NOS_DRAWING);
 }
 s->budget_ready=s->world->video.pools.valid;
 if(s->budget_ready){
  s->staged_pools=s->world->video.pools;
  if(!native_source_strips_free(&s->strips,&s->staged_pools,s->staged_free))return fail(s,NOS_QUEUE);
 }
 return 1;
}
int native_object_scene_append(struct NativeObjectScene*s){return native_object_scene_append_budgeted(s,0);}
int native_object_scene_stage(struct NativeObjectScene*s){
 if(!s)return 0;native_object_begin(&s->frame,s->config.tile_budget);
 return native_object_scene_append(s);
}
int native_object_scene_prepare(struct NativeObjectScene*s){
 if(!native_object_scene_stage(s))return 0;
 if(!native_object_finish(&s->frame))return fail(s,NOS_HARDWARE);return 1;
}
int native_object_scene_commit(struct NativeObjectScene*s){
 if(!s||s->fault||!native_object_commit(&s->frame))return fail(s,NOS_HARDWARE);
 if(s->budget_ready){s->latched_budget[0]=s->staged_free[0];s->latched_budget[1]=s->staged_pools.lower_end;s->latched_budget[2]=s->staged_pools.upper_begin;s->latched_valid=1;}
 return 1;
}
void native_object_scene_latch(struct NativeObjectScene*s){
 if(!s||!s->latched_valid)return;
 s->world->weapons.sprite_budget=(struct WeaponSpriteBudget){s->latched_budget[0],s->latched_budget[1],s->latched_budget[2],1};s->latched_valid=0;
}
int native_object_scene_attach(struct NativeObjectScene*s,struct NativePlayWorld*w,struct NativePaletteWorld*p,const struct NativeObjectSceneConfig*c){
 if(!s||!w||!p||!c||w->fault||p->fault||p->world!=w||w->drawing_output||
  w->video.palette_context!=p||w->video.palette_frame!=native_palette_frame||!c->layer||
  !c->tile_budget||c->tile_budget>NATIVE_OBJ_TILE_LIMIT)return 0;
 *s=(struct NativeObjectScene){.world=w,.palettes=p,.config=*c,.resolved=1};
 s->output=(struct AnimationOutput){s,begin,draw,end};w->drawing_output=&s->output;
 w->video.sprite_context=s;w->video.sprite_reset=reset;
 return native_object_scene_prepare(s);
}
