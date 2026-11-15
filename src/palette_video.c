#include "palette_video.h"
#include "play_world.h"
#include "drawing.h"
#include "../generated/render-data.h"
_Static_assert(sizeof(struct NativePaletteWorld)<27648,"palette RAM budget");
static int fail(struct NativePaletteWorld*p){if(p)p->fault=1;return 0;}
static int source_valid(unsigned n){return n<1536&&native_palette(n)!=0;}
static unsigned fade_component(unsigned c,unsigned level,int white){
 unsigned change=native_color_ramp[white?31-c:c][level];return white?c+change:c-change;
}
static uint16_t faded(uint16_t color,const struct ScenePaletteFade*f){
 if(!f->mode)return color;int white=f->mode==255;
 return fade_component(color&31,f->red,white)|
  (fade_component((color>>5)&31,f->green,white)<<5)|
  (fade_component((color>>10)&31,f->blue,white)<<10);
}
static void render(struct NativePaletteState*s,const struct ScenePaletteFade*f){
 const uint16_t*from=native_palette(s->source),*to=native_palette(s->target);
 /* Source colour0 is a control word; transparent index0 is never shaded. */
 s->colors[0]=0;
 for(unsigned i=1;i<16;i++){
  uint16_t c=from[i];
  if(s->flags&2){
   c=0;for(unsigned shift=0;shift<15;shift+=5){
    int a=(from[i]>>shift)&31,b=(to[i]>>shift)&31,d=b-a;
    unsigned amount=s->progress?native_color_ramp[d<0?-d:d][(s->progress-1)&31]:0;
    c|=(uint16_t)(d<0?a-(int)amount:a+(int)amount)<<shift;
   }
  }
  /*013480 blends apply the global fade even to bit3-protected slots. */
  s->colors[i]=((s->flags&2)||!(s->flags&8))?faded(c,f):c;
 }
}
static void initialize(struct NativePaletteState*s,unsigned source,unsigned binding,unsigned flags,unsigned generation){
 uint32_t serial=s->serial+1;
 *s=(struct NativePaletteState){.serial=serial,.source=source,.target=source,.binding=binding,.generation=generation,.flags=flags|1,.valid=1};
}
static void blend(struct NativePaletteState*s,unsigned target,unsigned period,unsigned step){
 s->target=target;s->period=s->timer=period;s->progress=0;s->step=step;s->flags|=2;
}
static void step(struct NativePaletteState*s,const struct ScenePaletteFade*f,int fade_changed,int materialize){
 if(!s->valid||(s->flags&128))return;
 if(!(s->flags&(1|2|64))&&!fade_changed)return;
 if(s->flags&64){if(materialize)for(unsigned i=0;i<16;i++)s->colors[i]=0;s->flags=128;return;}
 int changed=0;
 if(s->flags&2){
  int finish=s->period==65535;
  if(!finish){
   s->timer=(uint16_t)(s->timer-1);
   if(!s->timer){
    changed=1;
    s->timer=s->period;s->progress=(uint8_t)(s->progress+(s->step?s->step:1));
    finish=(int8_t)s->progress>=32;
    if(!finish){
     const uint16_t*a=native_palette(s->source),*b=native_palette(s->target);unsigned matches=0;
     /* The original completion AND includes all16 RGB entries. */
     for(unsigned i=0;i<16;i++){
      unsigned equal=0;for(unsigned bit=0;bit<15;bit+=5){int x=(a[i]>>bit)&31,y=(b[i]>>bit)&31,d=y-x;
       unsigned amount=s->progress?native_color_ramp[d<0?-d:d][(s->progress-1)&31]:0;
       if((d<0?x-(int)amount:x+(int)amount)==y)equal++;}
      matches+=equal==3;
     }
     finish=matches==16;
    }
   }
  }
  if(finish){s->source=s->target;s->period=0;s->flags=(s->flags&~2)|1;changed=1;}
 }
 if(s->flags&2){
  /*2C86 skips the global update while interpolation remains active. Its
   *previous output must survive countdown frames, even during a global fade. */
  if(changed&&materialize)render(s,f);
  if(!f->mode&&(s->flags&1)){s->flags&=(uint8_t)~2;if(materialize)render(s,f);s->flags=(s->flags&~1)|2;}
  return;
 }
 if(materialize&&((s->flags&1)||(fade_changed&&!(s->flags&8))))render(s,f);
 s->flags&=(uint8_t)~1;
}
int native_palette_set(void*ctx,uint8_t slot,uint16_t source,uint8_t flags){
 struct NativePaletteWorld*p=ctx;if(!p||p->fault)return 0;
 if(slot>0xc3&&slot!=255)return fail(p);
 p->unfaded_revision++;
 if(!p->native_backgrounds||slot<16||slot>=118)p->fixed_revision++;
 p->fixed_pending[slot>>5]|=1u<<(slot&31);
 struct NativePaletteState*s=&p->slots[slot];
 if(slot>=16&&slot<118&&(!s->valid||s->source!=source||(s->flags&(64|128))))p->binding_revision++;
 if(source==65535){*s=(struct NativePaletteState){.flags=64,.valid=1};return 1;}
 if(!source_valid(source))return fail(p);initialize(s,source,source,flags,0);return 1;
}
int native_palette_blend(void*ctx,uint16_t slot,uint16_t target,uint16_t period,uint8_t increment){
 struct NativePaletteWorld*p=ctx;if(!p||p->fault)return 0;
 if(slot>=256||!p->slots[slot].valid||!source_valid(target)||increment>31)return fail(p);
 p->unfaded_revision++;
 p->fixed_pending[slot>>5]|=1u<<(slot&31);
 blend(&p->slots[slot],target,period,increment);return 1;
}
static int actor_source(struct NativePaletteWorld*p,struct EntityRef id,uint16_t*source,uint8_t*flags){
 struct NativeEntity*e=entity_get(&p->world->entities,id);if(!e)return fail(p);*flags=0;
 if(e->kind==ENTITY_PLAYER)*source=e->payload.player.actor.palette;
 else if(e->kind==ENTITY_PLAYER_ARM)*source=e->payload.arm.palette;
 else{struct WeaponActor*a=entity_weapon_actor(e);if(!a)return fail(p);*source=a->palette;
  if(e->kind==ENTITY_ENEMY)*flags=e->payload.enemy.private_palette_flags;}
 if(*source>=1024)return fail(p);return 1;
}
int native_actor_palette_blend(void*ctx,uint16_t index,uint16_t generation,uint16_t target,uint16_t period,uint8_t increment){
 struct NativePaletteWorld*p=ctx;if(!p||p->fault||!p->world)return 0;
 if(index>=ENTITY_CAPACITY||target>=1024||increment>31)return fail(p);
 uint16_t source;uint8_t flags;if(!actor_source(p,(struct EntityRef){index,generation},&source,&flags))return 0;
 p->actor_live[index>>5]|=1u<<(index&31);
 struct NativePaletteState*s=&p->actors[index];
 if(!s->valid||s->generation!=generation||s->binding!=source)initialize(s,source,source,flags,generation);
 s->flags=(s->flags&3)|flags;blend(s,target,period,increment);return 1;
}
static void fixed_step(struct NativePaletteWorld*p,unsigned slot,const struct ScenePaletteFade*f,int fade_changed){
 struct NativePaletteState*s=&p->slots[slot];if(!s->valid)return;
 /* Native BG assets own slots16..117. Their fixed RGB palettes and GPU
  * fade no longer need thousands of shadow colour calculations. Preserve
  * interpolation timers/completion; OBJ hit palettes start at118. */
 if(p->native_backgrounds&&slot>=16&&slot<118){if(!(s->flags&(1|2|64)))return;unsigned source=s->source;step(s,f,fade_changed,0);if(source!=s->source)p->binding_revision++;return;}
 if(!(s->flags&(1|2|64))&&(!fade_changed||(s->flags&8)))return;
 uint16_t before[16];for(unsigned j=0;j<16;j++)before[j]=s->colors[j];step(s,f,fade_changed,1);
 for(unsigned j=0;j<16;j++)if(before[j]!=s->colors[j]){p->fixed_revision++;break;}
}
int native_palette_frame(void*ctx,const struct ScenePaletteFade*f){
 struct NativePaletteWorld*p=ctx;if(!p||p->fault||!f)return 0;
 if(f->red>31||f->green>31||f->blue>31)return fail(p);
 int changed=p->fade.mode!=f->mode||p->fade.red!=f->red||p->fade.green!=f->green||p->fade.blue!=f->blue;p->fade=*f;
 /* Track pending fixed operations and live private palettes. Unchanged
  * hardware banks need no197-slot scan on every game tick. */
 for(unsigned word=0;word<8;word++){
  uint32_t allowed=word<6?0xffffffffu:word==6?15u:0x80000000u;
  uint32_t bits=changed?allowed:p->fixed_pending[word]&allowed;
  while(bits){unsigned bit=__builtin_ctz(bits),i=word*32+bit;bits&=bits-1;
   struct NativePaletteState*s=&p->slots[i];unsigned source=s->source,progress=s->progress,flags=s->flags;
   fixed_step(p,i,f,changed);
   /* Timer-only ticks retain the same interpolated RGB. Clears and blend
    * completion must invalidate too, even when the source slot is stable. */
   if(source!=s->source||progress!=s->progress||flags!=s->flags)p->unfaded_revision++;
   if(!s->valid||(s->flags&128)||!(s->flags&(1|2|64)))p->fixed_pending[word]&=~(1u<<bit);
  }
  bits=p->actor_live[word];if(!word)bits&=~1u;
  while(bits){unsigned bit=__builtin_ctz(bits),i=word*32+bit;bits&=bits-1;
   struct NativePaletteState*s=&p->actors[i];
   if(!s->valid){p->actor_live[word]&=~(1u<<bit);continue;}
   struct NativeEntity*e=entity_get(&p->world->entities,(struct EntityRef){i,s->generation});
   if(!e||e->deleting){s->valid=0;p->actor_live[word]&=~(1u<<bit);continue;}
   step(s,f,changed,1);
  }
 }
 return 1;
}
const uint16_t*native_palette_slot(const struct NativePaletteWorld*p,unsigned slot){
 return p&&!p->fault&&slot<256&&p->slots[slot].valid?p->slots[slot].colors:0;
}
/* Native 4bpp banks carry source slot/index bindings compiled offline.
 * Apply source interpolation before the shared GBA hardware/global fade. */
uint16_t native_palette_unfaded_color(const struct NativePaletteWorld*p,unsigned slot,unsigned index){
 if(slot>=256||index>=16||!p->slots[slot].valid)return 0;
 const struct NativePaletteState*s=&p->slots[slot];
 if(s->flags&(64|128))return 0;
 const uint16_t*from=native_palette(s->source);if(!from)return 0;
 uint16_t color=from[index];if(!(s->flags&2))return color;
 const uint16_t*to=native_palette(s->target);if(!to)return color;
 unsigned result=0;for(unsigned shift=0;shift<15;shift+=5){
  int a=(color>>shift)&31,b=(to[index]>>shift)&31,d=b-a;
  unsigned amount=s->progress?native_color_ramp[d<0?-d:d][(s->progress-1)&31]:0;
  result|=(unsigned)(d<0?a-(int)amount:a+(int)amount)<<shift;
 }return result;
}
int native_palette_background(struct NativeBackgroundPaletteCache*c,const struct NativePaletteWorld*p,const uint16_t*base,const uint16_t*bindings,uint16_t out[192]){
 const struct ScenePaletteFade*f=&p->fade;
 /* Uniform fades are performed by the LCD. Only per-channel fades alter
  * these source RGB values, so their key is independent of hardware fade. */
 unsigned shade=f->mode&&(f->red!=f->green||f->red!=f->blue)?
  f->mode|((unsigned)f->red<<8)|((unsigned)f->green<<16)|((unsigned)f->blue<<24):0;
 if(c->base==base&&c->bindings==bindings&&c->shade==shade&&(!bindings||c->revision==p->unfaded_revision))return 0;
 for(unsigned i=0;i<192;i++){
  unsigned color=base[i],binding=bindings?bindings[i]:0;
  if(binding)color=native_palette_unfaded_color(p,binding>>4,binding&15);
  if(shade)color=faded(color,f);
  out[i]=color;
 }
 *c=(struct NativeBackgroundPaletteCache){base,bindings,p->unfaded_revision,shade};return 1;
}
static struct NativePaletteState*actor_state(struct NativePaletteWorld*p,struct EntityRef id,uint16_t source,uint8_t flags){
 if(id.index>=ENTITY_CAPACITY||source>=1024){fail(p);return 0;}
 p->actor_live[id.index>>5]|=1u<<(id.index&31);
 struct NativePaletteState*s=&p->actors[id.index];
 if(!s->valid||s->generation!=id.generation||s->binding!=source)initialize(s,source,source,flags,id.generation);
 unsigned changed=(s->flags^flags)&8;
 s->flags=(s->flags&3)|flags;if(!(s->flags&2)&&((s->flags&1)||changed)){render(s,&p->fade);s->flags&=(uint8_t)~1;}
 return s;
}
int native_palette_actor(struct NativePaletteWorld*p,struct EntityRef id,uint16_t source,uint8_t flags,uint16_t out[16]){
 if(!p||p->fault||!out)return 0;
 struct NativePaletteState*s=actor_state(p,id,source,flags);if(!s)return 0;
 for(unsigned i=0;i<16;i++)out[i]=s->colors[i];return 1;
}
int native_palette_attach(struct NativePaletteWorld*p,struct NativePlayWorld*w){
 if(!p||!w||w->fault)return 0;*p=(struct NativePaletteWorld){.world=w};
 w->video.palette_context=p;w->video.set_palette=native_palette_set;
 w->video.palette_cycle=native_palette_blend;w->video.actor_palette_cycle=native_actor_palette_blend;
 w->video.palette_frame=native_palette_frame;return 1;
}
int native_palette_capture(struct NativePaletteWorld*p,struct EntityRef owner,uint16_t source,uint8_t flags,unsigned fixed_slot,struct NativePaletteCapture*c){
 if(!p||p->fault||!c||!p->world)return 0;
 const struct NativePaletteState*state;
 if(fixed_slot!=65535){if(!native_palette_slot(p,fixed_slot))return fail(p);state=&p->slots[fixed_slot];}
 else{
  if(!entity_get(&p->world->entities,owner))return fail(p);
  state=actor_state(p,owner,source,flags);if(!state)return fail(p);
 }
 /* One immutable snapshot. Avoid zeroing it, copying RGB into it, and then
  * overwriting that RGB with a second complete state copy for every DRAW. */
 *c=(struct NativePaletteCapture){.state=*state,.owner=owner,.slot=fixed_slot,.fixed=fixed_slot!=65535};return 1;
}
int native_palette_resolve_capture(struct NativePaletteWorld*p,struct NativePaletteCapture*c){
 if(!p||p->fault||!c||!c->state.valid)return 0;
 if(c->fixed){if(!native_palette_slot(p,c->slot))return fail(p);c->state=p->slots[c->slot];return 1;}
 if(c->owner.index>=ENTITY_CAPACITY)return fail(p);
 const struct NativePaletteState*s=&p->actors[c->owner.index];
 if(s->valid&&s->generation==c->owner.generation&&s->serial==c->state.serial)c->state=*s;
 else step(&c->state,&p->fade,1,1);
 /* Native retained-state adaptation: a queued image keeps its known RGB
  * binding through deletion/rebinding and receives this frame's fade once.
  * Original48-slot sharing/release/reuse is not yet modeled by this backend. */
 return 1;
}
