#include "frontend_internal.h"
static void tick(struct EntityPool*,struct NativeEntity*,void*);
struct PlayerTransform frontend_transform(struct WeaponWorld*w,int x,int y,unsigned priority){
 return (struct PlayerTransform){.x=(int32_t)tk_word(x+w->camera_x)*256,.y=(int32_t)tk_word(512-y+w->camera_y)*256,.priority=priority,.display_group=255};
}
void frontend_start(struct EnemyActor*q,unsigned id){animation_begin(&q->actor.animation,frontend_entries[id]);q->actor.movement_flags|=32;}
struct NativeEntity*frontend_allocate(struct WeaponWorld*w,struct EntityRef parent,unsigned kind,int x,int y,unsigned palette,unsigned priority,unsigned graph){
 struct PlayerTransform t=frontend_transform(w,x,y,priority);
 struct NativeEntity*e=enemy_allocate(w,parent,ENTITY_FIRST_CHILD,&t,tick);if(!e)return 0;
 struct EnemyActor*q=&e->payload.enemy;q->frontend=(__typeof__(q->frontend)){.kind=kind};q->actor.palette=palette;
 q->actor.vx=q->actor.vy=q->actor.ax=q->actor.ay=0;q->actor.phase=0;
 if(graph<FRA_COUNT)frontend_start(q,graph);return e;
}
struct Context{struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*v,unsigned f,unsigned width){struct Context*c=v;struct WeaponActor*a=&c->e->payload.enemy.actor;if(f==0&&width==1)return a->transform.facing;if(f==1&&width==2)return (uint16_t)a->vx;enemy_fault(c->w);return 0;}
static int write(void*v,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t n){struct Context*c=v;uint32_t old=read(v,f,width);if(op==AN_FIELD_AND)n&=old;else if(op==AN_FIELD_OR)n|=old;else if(op==AN_FIELD_XOR)n^=old;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(c->w->fault)return 0;if(f==0)c->e->payload.enemy.actor.transform.facing=n;else c->e->payload.enemy.actor.vx=n;return 1;}
static int call(void*v,unsigned id,struct Animation*a){(void)a;struct Context*c=v;if(id)return enemy_fault(c->w);struct WeaponActor*q=&c->e->payload.enemy.actor;q->vx=q->vy=q->ax=q->ay=0;return 1;}
static int sound(void*v,uint16_t n,int stop){struct Context*c=v;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
int frontend_animate(struct WeaponWorld*w,struct NativeEntity*e){struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=write,.call=call,.sound=sound};struct Animation*a=&e->payload.enemy.actor.animation;int done=animation_step(a,&frontend_program,&o);if(a->fault)enemy_fault(w);return done;}
int frontend_finish(struct NativePlayWorld*w){struct NativeEntity*p=entity_get(&w->entities,w->session->boot);if(!p)return enemy_fault(&w->weapons);p->payload.group.phase=1;return 1;}
int frontend_actors_done(const struct NativePlayWorld*w){const struct NativeSession*s=w->session;return s&&s->state==SESSION_PRESENTATION&&w->entities.nodes[s->boot.index].payload.group.phase;}
static int blend(struct WeaponWorld*w,struct NativeEntity*e,unsigned palette){struct SceneVideoWorld*v=w->video;return v&&v->actor_palette_cycle&&v->actor_palette_cycle(v->palette_context,e->index,e->generation,palette,1,1)?1:enemy_fault(w);}
static int headers(struct WeaponWorld*w){static const uint16_t cells[]={0x83,0x1a3,0x2c3};for(unsigned j=0;j<3;j++)for(unsigned i=0;i<8;i++){uint16_t v=frontend_ranking_headers[j][i];if(v==65535)break;if(!result_rect(w,cells[j]+64*i,v|0x4000,2,2,1))return 0;}return 1;}
static int row(struct NativePlayWorld*w,unsigned i){const struct HighScoreRecord*r=&w->results.table[i];uint16_t cell=0xa6+2*i;return result_rank(&w->weapons,cell,r->rank)&&result_packed_digits(&w->weapons,cell+0xc0,r->packed_score)&&result_name(&w->weapons,cell+0x300,r->name,3);}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*user){
 struct NativePlayWorld*w=user;struct WeaponWorld*k=&w->weapons;struct EnemyActor*q=&e->payload.enemy;
 if(w->fault||k->fault)return;
 switch(q->frontend.kind){
 case FR_LOGO:if(++q->frontend.clock>=200)frontend_finish(w);break;
 case FR_PIECE:if(!q->frontend.delay||!--q->frontend.delay)frontend_animate(k,e);if(++q->frontend.clock>=12)entity_delete(p,e);break;
 case FR_CENTER:
  if(q->frontend.delay){if(--q->frontend.delay)break;if(!blend(k,e,0x11f))break;}
  frontend_animate(k,e);
  if(q->scale_y!=255){if(q->frontend.index>=22){enemy_fault(k);break;}q->scale_y=frontend_logo_zoom[q->frontend.index++];}
  else if(++q->frontend.clock>=16&&!q->frontend.latch){q->frontend.latch=1;if(!blend(k,e,0x120))break;q->private_palette_flags|=8;if(!w->video.palette_cycle||!w->video.palette_cycle(w->video.palette_context,255,0x120,1,1))enemy_fault(k);}break;
 case FR_RANK_LOGO:
  if(!q->frontend.latch){q->frontend.clock+=32;q->scale_x+=32;if(q->frontend.clock>=255){q->scale_x=255;q->frontend.latch=1;struct NativeEntity*r=entity_get(p,e->parent);if(!r){enemy_fault(k);break;}r->payload.enemy.frontend.latch=1;}}
  frontend_animate(k,e);break;
 case FR_RANKING:
  if(!q->frontend.latch)break;
  if(q->frontend.phase==0){if(++q->frontend.clock<16)break;if(!headers(k))break;q->frontend.clock=0;q->frontend.phase=1;}
  if(q->frontend.phase==1){if(++q->frontend.clock<2)break;q->frontend.clock=0;if(!row(w,q->frontend.index++))break;if(q->frontend.index==10){q->frontend.phase=2;q->frontend.clock=60;}}
  else if(q->frontend.phase==2&&!--q->frontend.clock)frontend_finish(w);break;
 default:if(!frontend_tutorial_tick(w,e)&&!k->fault)enemy_fault(k);break;
 }
}
int frontend_actors_begin(struct NativePlayWorld*w,enum NativePresentation kind){
 if(!w||!w->session||!w->scene_runtime||kind<NPRESENT_LOGO||kind>NPRESENT_TUTORIAL)return 0;
 struct WeaponWorld*k=&w->weapons;struct NativeSession*s=w->session;
 s->state=SESSION_PRESENTATION;s->pause_allowed=0;s->hud_flags=0;
 w->results.live_active[0]=w->results.live_active[1]=0;
 w->video.start_prompt_valid=0;w->interactions.demo_mode=1;w->enemies.mission_time=1;
 struct NativeEntity*boot=entity_get(&w->entities,s->boot);if(!boot)return enemy_fault(k);boot->payload.group.phase=0;
 unsigned program=kind==NPRESENT_LOGO?13:kind==NPRESENT_RANKING?11:9;
 if(!scene_runtime_load(w->scene_runtime,w,program,program))return 0;
 struct NativeEntity*root=frontend_allocate(k,s->boot,kind==NPRESENT_LOGO?FR_LOGO:kind==NPRESENT_RANKING?FR_RANKING:FR_TUTORIAL,160,451,0x12e,0x4000,kind==NPRESENT_TUTORIAL?FRA_TUTORIAL:FRA_COUNT);if(!root)return 0;
 source_audio_byte(&w->scene_runtime->audio,6);
 if(kind==NPRESENT_LOGO){
  source_audio_game_command(&w->scene_runtime->audio,0x2c);
  for(unsigned i=0;i<9;i++){struct NativeEntity*e=frontend_allocate(k,entity_ref(root),FR_PIECE,frontend_logo_pieces[i][0],384,0x11d,0,FRA_LOGO_PIECE);if(!e)return 0;e->payload.enemy.frontend.delay=frontend_logo_pieces[i][1];e->payload.enemy.private_palette_flags|=8;}
  struct NativeEntity*e=frontend_allocate(k,entity_ref(root),FR_CENTER,160,384,0x11e,0,FRA_LOGO_CENTER);if(!e)return 0;e->payload.enemy.frontend.delay=13;e->payload.enemy.scale_y=42;e->payload.enemy.private_palette_flags|=8;
 }else if(kind==NPRESENT_RANKING){
  struct ResultBytes footer={frontend_ranking_footer,sizeof(frontend_ranking_footer)};if(!result_stream(k,0x19b,&footer,0x4300))return 0;
  struct NativeEntity*e=frontend_allocate(k,entity_ref(root),FR_RANK_LOGO,160,384,0x15f,0,(tk_random(k)&1)?FRA_RANK_B:FRA_RANK_A);if(!e)return 0;e->payload.enemy.scale_x=0;
 }else if(!frontend_tutorial_init(w,root))return 0;
 return !k->fault;
}
