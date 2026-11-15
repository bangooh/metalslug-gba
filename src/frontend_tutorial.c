#include "frontend_internal.h"
/* Tutorial explanation only: move the OBJ panel down eight source pixels
 * (six GBA pixels at 75%), and its FIX text/clear rectangles down one row. */
static int slug(struct NativePlayWorld*w){struct PlayerTransform t=frontend_transform(&w->weapons,160,511,0);return play_world_request_slug(w,&t,3,0);}
static struct EnemyActor*parent(struct NativePlayWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(&w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(&w->weapons);return 0;}return &p->payload.enemy;}
static int drop(struct NativePlayWorld*w,struct NativeEntity*e){
 struct NativeEntity*n=frontend_allocate(&w->weapons,entity_ref(e),FR_DROP,320,384,0x179,0x8000,FRA_DROP);if(!n)return 0;
 struct EnemyActor*q=&n->payload.enemy;q->actor.vx=-2048;q->attack_set=(struct HitAttackSet){&frontend_drop_attack,1};q->actor.attack=&q->attack_set;hit_begin_attack(&q->actor.hit);return 1;
}
static int text(struct NativePlayWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponWorld*k=&w->weapons;
 if(q->frontend.clock==65535)return 1;
 q->frontend.clock--;if((int16_t)q->frontend.clock>0)return 1;
 if(q->frontend.stream>=sizeof(frontend_text)/sizeof(*frontend_text))return enemy_fault(k);
 const struct FrontendTextStream*s=&frontend_text[q->frontend.stream];int wide_glyphs=0;
 /* Both original interpreters process the complete selected text stream
  * after their entry delay. B's shared branch for codes1/2/3 enters A. */
 while(q->frontend.index<s->count){
  const struct FrontendText*r=&s->rows[q->frontend.index++];q->frontend.clock=r->duration;
  if(r->duration==65535)return 1;
  if(r->animation!=65535){animation_begin(&q->actor.animation,r->animation);q->actor.movement_flags|=32;}
  unsigned code=r->code,cell=q->frontend.cell;
  if(code==1){if(!result_rect(k,0x85,0x2320,32,8,0))return 0;wide_glyphs=1;}
  else if(code==13)q->frontend.cell=wide_glyphs?0xaa:((cell&0xf)+0xa2);
  else if(code==2){if(!drop(w,e))return 0;wide_glyphs=1;}
  else if(code==3){if(!slug(w))return 0;wide_glyphs=1;}
  else if(code){
   if(wide_glyphs){unsigned high=code&0xff00,palette=(high==0x500||high==0x600||high==0x700||high>=0xd00)?9:4;if(!result_rect(k,cell,code|(palette<<12),2,2,1))return 0;q->frontend.cell+=64;}
   else{if(!result_rect(k,cell,code|0x2300,1,1,0))return 0;if(code==0xe6&&!result_rect(k,cell+1,(code|0x2300)+16,1,1,0))return 0;if(code>=0xe7&&code<=0xef&&!result_rect(k,cell-1,(code|0x2300)-16,1,1,0))return 0;q->frontend.cell+=32;}
  }
 }
 return enemy_fault(k);
}
static int timeline(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->frontend.clock==65535)return 1;
 q->frontend.clock--;q->frontend.edges=0;
 if((int16_t)q->frontend.clock<=0){
  if(q->frontend.index>=sizeof(frontend_input)/sizeof(*frontend_input)){enemy_fault(w);return 0;}
  const struct FrontendInput*i=&frontend_input[q->frontend.index++];q->frontend.clock=i->duration;q->frontend.page=i->page;q->frontend.held=i->held;q->frontend.edges=i->edges;
 }
 return 0;
}
int frontend_tutorial_init(struct NativePlayWorld*w,struct NativeEntity*root){
 struct WeaponWorld*k=&w->weapons;struct EnemyActor*q=&root->payload.enemy;q->scale_x=q->scale_y=0;q->actor.display_flags|=64;
 source_audio_game_command(&w->scene_runtime->audio,0x2b);
 w->enemies.damage_enabled=255;w->slug_player_query_enabled=w->slug_riders_enabled=1;w->results.live_active[0]=1;
 struct PlayerTransform t=frontend_transform(k,96,369,2);
 if(!slug(w)||!play_world_add_player(w,0,&t,PST_FALL))return 0;
 struct PlayerBodyState*p=w->weapons.players[0];p->actor.display_flags&=~16;p->actor.hit.flags|=64;
 struct NativeEntity*e=frontend_allocate(k,entity_ref(root),FR_DIRECTION,64,299,0xb1,0xe000,FRA_DIRECTION);if(!e)return 0;e->payload.enemy.actor.display_flags|=64;
 static const uint16_t palettes[]={0xae,0xaf,0xb0,0xb2};
 /* Keep action/palette identities A=fire/red, B=jump/yellow,
  * R=grenade/green, L=unused/blue; display them left-to-right B,A,L,R. */
 for(unsigned i=0;i<4;i++){
  unsigned x=112+48*(i^1u);
  e=frontend_allocate(k,entity_ref(root),FR_BUTTON,x,299,palettes[i],0xe000,FRA_BUTTON);if(!e)return 0;e->payload.enemy.actor.display_flags|=64;e->payload.enemy.frontend.mask=16u<<i;
  struct NativeEntity*lit=frontend_allocate(k,entity_ref(e),FR_LIT,x,299,palettes[i],0xe000,FRA_LIT1+i);if(!lit)return 0;lit->payload.enemy.actor.display_flags|=64;
 }
 /* L is now the third button: move its 24px FIX cross left by36px. */
 if(!result_rect(k,0x317,0x2e80,4,4,1))return 0;
 e=frontend_allocate(k,entity_ref(root),FR_ANCHOR,224,344,0x97,0x8000,FRA_ANCHOR);if(!e)return 0;e->payload.enemy.frontend.cell=0x85;
 return 1;
}
int frontend_tutorial_tick(struct NativePlayWorld*w,struct NativeEntity*e){
 struct WeaponWorld*k=&w->weapons;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(q->frontend.kind){
 case FR_TUTORIAL:{
  unsigned phase=q->frontend.phase;int value;
  switch(a->phase){
  case 0:case 1:
   value=frontend_zoom[0][phase];if(value>255)value=255;
   if(a->phase==0)q->scale_y=value;else q->scale_x=value;
   q->frontend.phase+=a->phase?4:8;
   if(q->frontend.phase>63){if(a->phase==0)q->scale_y=255;else q->scale_x=255;q->frontend.phase=0;a->phase++;}break;
  case 2:if(timeline(k,q)){if(!result_rect(k,0x85,0x2320,32,25,0))return 0;a->phase=3;q->frontend.phase=0;}break;
  case 3:case 4:
   value=frontend_zoom[1][phase]-(a->phase==3);if(value<0)value=0;
   if(a->phase==3)q->scale_x=value;else q->scale_y=value;
   q->frontend.phase+=a->phase==3?4:8;
   if(q->frontend.phase>63){q->frontend.phase=0;if(a->phase==3){q->scale_x=0;a->phase=4;}else{q->scale_y=0;a->phase=5;q->frontend.clock=60;source_audio_command(&w->scene_runtime->audio,0x0a80);if(!scene_fade_spawn(k,entity_ref(e),2,0,0))return 0;}}break;
  case 5:if(q->frontend.clock&&!--q->frontend.clock)frontend_finish(w);break;
  default:return enemy_fault(k);
  }
  /* Root precedes scene, player and Slug actors in the scheduler. Publish
   * the source tutorial stream to all consumers before their first read. */
  struct PlayerSourceInput input={q->frontend.held,q->frontend.edges};w->inputs[0]=w->raw_inputs[0]=k->inputs[0]=input;
  w->inputs[1]=w->raw_inputs[1]=k->inputs[1]=(struct PlayerSourceInput){0};
  w->results.edges[0]=q->frontend.edges;w->results.start[0]=q->frontend.held;w->raw_inputs_valid=1;
  scene_runtime_follow_players(w->scene_runtime,w);frontend_animate(k,e);break;
 }
 case FR_DIRECTION:{struct EnemyActor*r=parent(w,e);if(!r)return 0;uint16_t graph=frontend_directions[r->frontend.held&15];if(graph!=65535){animation_begin(&a->animation,graph);a->movement_flags|=32;}frontend_animate(k,e);break;}
 case FR_BUTTON:{struct EnemyActor*r=parent(w,e);if(!r)return 0;if(r->frontend.edges&q->frontend.mask){q->frontend.latch=1;frontend_start(q,FRA_PRESSED);}if(!(r->frontend.held&q->frontend.mask)&&q->frontend.latch){q->frontend.latch=0;frontend_start(q,FRA_RELEASED);}frontend_animate(k,e);break;}
 case FR_LIT:frontend_animate(k,e);break;
 case FR_ANCHOR:{
  struct EnemyActor*r=parent(w,e);if(!r)return 0;
  if(r->frontend.page){if(r->frontend.page>=44)return enemy_fault(k);if(q->frontend.page!=r->frontend.page){q->frontend.page=r->frontend.page;const uint16_t*page=frontend_pages[1][q->frontend.page];q->frontend.stream=page[0];q->frontend.cell=page[1]+1;q->frontend.index=q->frontend.clock=0;}if(!text(w,e))return 0;}
  if(interaction_move(k,a,WMOVE_POINT)<0)return 0;frontend_animate(k,e);
  if((uint16_t)tk_sx(k,a)<=336)entity_delete_children(&w->entities,e);break;
 }
 case FR_DROP:
  frontend_animate(k,e);if(interaction_move(k,a,WMOVE_POINT)<0||!enemy_attack(k,a))return 0;
  if((a->hit.flags&2)||interaction_outside(k,a,frontend_drop_bounds,0)){struct PlayerTransform t=a->transform;if(!boss_fx_spawn(k,entity_ref(e),ENTITY_FIRST_CHILD,&t,BFX_FIRE))return 0;entity_delete(&w->entities,e);}break;
 default:return enemy_fault(k);
 }
 return !k->fault;
}
