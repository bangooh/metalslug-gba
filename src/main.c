#include <stdint.h>
#include <stddef.h>
#include "game.h"
#include "audio.h"
#include "../generated/assets.h"
#if defined(COMBAT_FIXTURE)||defined(NATIVE_MISSION)
#include "combat.h"
#include "../generated/combat-assets.h"
#endif
#ifdef NATIVE_MISSION
#include "campaign.h"
#include "../generated/actor-assets.h"
#endif
#define REG16(a) (*(volatile uint16_t*)(a))
#define REG32(a) (*(volatile uint32_t*)(a))
#define FAST __attribute__((section(".iwram"),noinline))
struct Metrics {uint32_t ready,presents,ticks,last_cycles,max_cycles,last_dma_bytes,max_dma_bytes,total_dma_bytes,late_vblank,reloads,total_work_cycles,normal_max_cycles,normal_max_dma,sprite_uploads,sprite_hits,sprite_peak,sprite_overflow,logic_max_cycles,render_max_cycles;};
volatile struct Metrics metrics;
void*memset(void*dst,int value,size_t size){uint8_t*p=dst;while(size--)*p++=(uint8_t)value;return dst;}
void*memcpy(void*dst,const void*src,size_t size){uint8_t*d=dst;const uint8_t*s=src;while(size--)*d++=*s++;return dst;}
static uint16_t slot_tile[768];
static int previous_left=-999,previous_top=-999,previous_frame=-1;
static int rendered_scene=-1;
static uint32_t dma_bytes;
static inline void dma(const void*src,void*dst,unsigned bytes){
 REG32(0x040000d4)=(uintptr_t)src;REG32(0x040000d8)=(uintptr_t)dst;
 REG32(0x040000dc)=0x84000000u|(bytes>>2);dma_bytes+=bytes;
}
static void clear_map(void*dst){
 const uint32_t zero=0;
 REG32(0x040000d4)=(uintptr_t)&zero;REG32(0x040000d8)=(uintptr_t)dst;
 REG32(0x040000dc)=0x85000000u|512;dma_bytes+=2048;
}
static uint32_t clock_now(void){
 unsigned hi,lo,end;
 do{hi=REG16(0x0400010c);lo=REG16(0x04000108);end=REG16(0x0400010c);}while(hi!=end);
 return (hi<<16)|lo;
}
static FAST void upload_cell(int x,int y){
 unsigned slot=(unsigned)(y%24)*32+((unsigned)x&31);
 unsigned id=(x>=0&&y>=0&&x<MAP_W&&y<MAP_H)?bg_map[y*MAP_W+x]:0;
 if(slot_tile[slot]!=id){dma(bg_tiles+id*64,(void*)(0x06000000+slot*64),64);slot_tile[slot]=(uint16_t)id;}
 volatile uint16_t*map=(void*)0x0600f800;map[(y&31)*32+(x&31)]=(uint16_t)slot;
}
static FAST void background(int camera_x,int camera_y){
 int left=camera_x>>3,top=camera_y>>3;
 /* Offline colour-lifetime allocation gives every co-visible colour a unique
  * index. Palette changes never require reindexing cached tiles. */
 if(left!=previous_left)dma(bg_palette+left*256,(void*)0x05000000,240*2);
 if(previous_left==-999){for(int y=top;y<=top+20;y++)for(int x=left;x<=left+30;x++)upload_cell(x,y);}
 else{
  for(int x=left;x<=left+30;x++)if(x<previous_left||x>previous_left+30)
   for(int y=top;y<=top+20;y++)upload_cell(x,y);
  for(int y=top;y<=top+20;y++)if(y<previous_top||y>previous_top+20)
   for(int x=left;x<=left+30;x++)if(x>=previous_left&&x<=previous_left+30)upload_cell(x,y);
 }
 previous_left=left;previous_top=top;
 REG16(0x04000014)=(uint16_t)(camera_x&255);REG16(0x04000016)=(uint16_t)(camera_y&255);
}
static void player_draw(void){
 const struct Player*p=&game.player;unsigned f=p->frame;
#if defined(COMBAT_FIXTURE)||defined(NATIVE_MISSION)
#ifdef NATIVE_MISSION
 if(combat.player_dead||(combat.player.invulnerable&&(game.ticks&2))){((volatile uint16_t*)0x07000000)[0]=0x200;return;}
#endif
 if(combat.active||combat.aiming){
  f=combat.active?combat.animation.frame:combat.aim.frame;
  if(previous_frame!=(int)(f+10000)){dma(combat_tiles+f*2048,(void*)0x06010000,2048);previous_frame=(int)f+10000;}
  int x=(p->x*3/4>>8)-game.camera_x-(p->facing?64-combat_origins[f][0]:combat_origins[f][0]);
  int y=(p->y*3/4>>8)-game.camera_y-combat_origins[f][1];
  volatile uint16_t*oam=(void*)0x07000000;
  oam[0]=(uint16_t)(y&255);oam[1]=(uint16_t)((x&511)|0xc000|(p->facing?0x1000:0));oam[2]=0x400;return;
 }
#endif
 if(previous_frame!=(int)f){dma(player_tiles+f*2048,(void*)0x06010000,2048);previous_frame=(int)f;}
 int x=(p->x*3/4>>8)-game.camera_x-(p->facing?64-player_origin_x[f]:player_origin_x[f]);
 int y=(p->y*3/4>>8)-game.camera_y-player_origin_y[f];
 volatile uint16_t*oam=(void*)0x07000000;
 oam[0]=(uint16_t)(y&255);oam[1]=(uint16_t)((x&511)|0xc000|(p->facing?0x1000:0));oam[2]=0x400;
}
#ifdef COMBAT_FIXTURE
static uint16_t crate_frames[COMBAT_CRATES];
static void combat_draw(void){
 volatile uint16_t*oam=(void*)0x07000000;
 for(unsigned i=1;i<128;i++)oam[i*4]=0x200;
 unsigned slot=1;
 for(unsigned i=0;i<COMBAT_CRATES;i++){
  const struct CombatCrate*c=&combat.crates[i];if(!c->active)continue;
  unsigned f=c->anim.frame;
  int x=(c->x*3/4>>8)-game.camera_x-combat_origins[f][0],y=(c->y*3/4>>8)-game.camera_y-combat_origins[f][1];
  if(x<=-64||x>=240||y<=-64||y>=160)continue;
  if(crate_frames[i]!=f){dma(combat_tiles+f*2048,(void*)(0x06010800+i*2048),2048);crate_frames[i]=(uint16_t)f;}
  oam[slot*4]=(uint16_t)(y&255);oam[slot*4+1]=(uint16_t)((x&511)|0xc000);oam[slot*4+2]=(uint16_t)(0x1400+(i+1)*64);slot++;
 }
 for(unsigned i=0;i<COMBAT_SHOTS;i++){
  const struct CombatShot*s=&combat.shots[i];if(!s->active||s->age<3)continue;
  int x=(s->x*3/4>>8)-game.camera_x-(s->vy?56:32),y=(s->y*3/4>>8)-game.camera_y-(s->vy?32:56);
  if(x<=-64||x>=240||y<=-64||y>=160)continue;
  /* Affine matrix 0 rotates the same resident bullet pixels upward. */
  if(s->vy){oam[3]=0;oam[7]=(uint16_t)-256;oam[11]=256;oam[15]=0;}
  oam[slot*4]=(uint16_t)((y&255)|(s->vy?0x100:0));oam[slot*4+1]=(uint16_t)((x&511)|0xc000|(s->vx<0?0x1000:0));oam[slot*4+2]=(uint16_t)(0x2400+14*64);slot++;
 }
}
#endif
#ifdef NATIVE_MISSION
/* All visible instances of one drawing share one OBJ tile allocation.
 * Reserve player slot 0 and bullet slot 14; pin every live cached frame before
 * replacing old frames, so a later OAM entry cannot reference overwritten art. */
struct Draw {int16_t x,y;uint16_t key;uint8_t flip,palette;};
static uint16_t object_keys[14];
static struct Draw draws[COMBAT_CRATES+ACTOR_CAPACITY];
static void mission_draw(void){
 volatile uint16_t*oam=(void*)0x07000000;
 for(unsigned i=1;i<128;i++)oam[i*4]=0x200;
 unsigned count=0,used=0,slot=1;
 for(unsigned i=0;i<COMBAT_CRATES+ACTOR_CAPACITY;i++){
  int32_t wx,wy;unsigned f,flip,pal,key;const uint8_t*origin;
  if(i<COMBAT_CRATES){const struct CombatCrate*c=&combat.crates[i];if(!c->active)continue;
   wx=c->x;wy=c->y;f=c->anim.frame;origin=combat_origins[f];key=f;pal=1;flip=0;
  }else{const struct Actor*a=&actors.slots[i-COMBAT_CRATES];if(!a->active||(a->kind==ACTOR_FOOD&&a->state&&!(a->timer&1)))continue;
   wx=a->x;wy=a->y;f=a->animation.frame;origin=actor_origins[f];key=f|0x8000;pal=origin[2]+3;flip=a->hit.flip;
  }
  int x=(wx*3/4>>8)-game.camera_x-(flip?64-origin[0]:origin[0]);int y=(wy*3/4>>8)-game.camera_y-origin[1];
  if(x<=-64||x>=240||y<=-64||y>=160)continue;
  draws[count++]=(struct Draw){(int16_t)x,(int16_t)y,(uint16_t)key,(uint8_t)flip,(uint8_t)pal};
  for(unsigned j=0;j<14;j++)if(object_keys[j]==key){used|=1u<<j;break;}
 }
 for(unsigned i=0;i<count;i++){
  const struct Draw*d=&draws[i];unsigned index=14;
  for(unsigned j=0;j<14;j++)if(object_keys[j]==d->key){index=j;metrics.sprite_hits++;break;}
  if(index==14){
   for(unsigned j=0;j<14;j++)if(!(used&(1u<<j))){index=j;break;}
   if(index==14){metrics.sprite_overflow++;campaign.unsupported=2;continue;}
   unsigned physical=index<13?index+1:15;
   const uint8_t*tiles=(d->key&0x8000)?actor_tiles:combat_tiles;
   dma(tiles+(d->key&0x7fff)*2048,(void*)(0x06010000+physical*2048),2048);
   object_keys[index]=d->key;used|=1u<<index;metrics.sprite_uploads++;
  }
  unsigned physical=index<13?index+1:15;
  oam[slot*4]=(uint16_t)(d->y&255);oam[slot*4+1]=(uint16_t)((d->x&511)|0xc000|(d->flip?0x1000:0));
  oam[slot*4+2]=(uint16_t)(0x400+physical*64+(d->palette<<12));slot++;
 }
 unsigned unique=0;for(unsigned j=0;j<14;j++)unique+=!!(used&(1u<<j));if(unique>metrics.sprite_peak)metrics.sprite_peak=unique;
 for(unsigned i=0;i<COMBAT_SHOTS;i++){
  const struct CombatShot*s=&combat.shots[i];if(!s->active||s->age<3)continue;
  int x=(s->x*3/4>>8)-game.camera_x-(s->vy?56:32),y=(s->y*3/4>>8)-game.camera_y-(s->vy?32:56);
  if(x<=-64||x>=240||y<=-64||y>=160)continue;
  /* Affine matrix 0 rotates the same resident bullet pixels upward. */
  if(s->vy){oam[3]=0;oam[7]=(uint16_t)-256;oam[11]=256;oam[15]=0;}
  oam[slot*4]=(uint16_t)((y&255)|(s->vy?0x100:0));oam[slot*4+1]=(uint16_t)((x&511)|0xc000|(s->vx<0?0x1000:0));oam[slot*4+2]=(uint16_t)(0x2400+14*64);slot++;
 }
}
#endif
static void text(unsigned x,unsigned y,const char*s){
 volatile uint16_t*map=(void*)0x0600f000;
 while(*s)map[y*32+x++]=(uint16_t)(0xf000+((unsigned char)*s++));
}
extern const uint8_t hud_font[];
extern const uint8_t title_tiles[];
extern const uint16_t title_maps[],title_palette[];
static void hide_objects(void){for(unsigned i=0;i<128;i++)((volatile uint16_t*)0x07000000)[i*4]=0x200;}
static void title_init(void){
 audio_command(0);audio_command(0x2b);
 hide_objects();
 dma(title_palette,(void*)0x05000000,512);dma(title_tiles,(void*)0x06000000,28864);
 dma(title_maps,(void*)0x0600e000,6144);dma(hud_font,(void*)0x0600c000,96*32);
 clear_map((void*)0x0600f800);
 REG16(0x050001e2)=0x7fff;
 REG16(0x04000008)=0x1f0c;REG16(0x0400000a)=0x1e01;
 REG16(0x0400000c)=0x1d02;REG16(0x0400000e)=0x1c03;
 for(unsigned i=0;i<8;i++)REG16(0x04000010+i*2)=0;
 /* title uses its own HUD screenblock */
 volatile uint16_t*map=(void*)0x0600f800;
 const char*s="PRESS START";for(unsigned i=0;s[i];i++)map[18*32+9+i]=(uint16_t)(0xf000+s[i]);
}
static void movement_init(void){
 audio_command(0);audio_command(0x21);
 /* Every visible background slot is overwritten below. Clearing all 64KiB
  * first wastes tens of milliseconds; only the HUD map needs a fill. */
 clear_map((void*)0x0600f000);
 hide_objects();for(unsigned i=0;i<768;i++)slot_tile[i]=65535;
 previous_left=previous_top=-999;previous_frame=-1;
 dma(player_palette,(void*)0x05000200,32);
#if defined(COMBAT_FIXTURE)||defined(NATIVE_MISSION)
 dma(combat_palettes,(void*)0x05000200,96);
 dma(combat_tiles+COMBAT_BULLET_FRAME*2048,(void*)0x06017000,2048);
#ifdef COMBAT_FIXTURE
 for(unsigned i=0;i<COMBAT_CRATES;i++)crate_frames[i]=65535;
#endif
#ifdef NATIVE_MISSION
 dma(actor_palettes,(void*)0x05000260,ACTOR_PALETTES*32);
 for(unsigned i=0;i<14;i++)object_keys[i]=65535;
#endif
#endif
 dma(hud_font,(void*)0x0600c000,96*32);REG16(0x050001e2)=0x7fff;
 REG16(0x04000008)=0x1e0c;REG16(0x0400000a)=0x1f82;
 REG16(0x04000010)=REG16(0x04000012)=0;
#ifdef COMBAT_FIXTURE
 text(0,0,"COMBAT FIXTURE - NOT MISSION 1");text(0,1,"A FIRE/KNIFE  B JUMP");
#elif defined(NATIVE_MISSION)
 text(0,0,"M1 NATIVE - WORK IN PROGRESS");text(0,1,"A FIRE/KNIFE  B JUMP");
#else
 text(0,0,"NATIVE 0.5.43  MOVEMENT TEST");text(0,1,"DPAD MOVE  B JUMP  SELECT MENU");
#endif
 background(game.camera_x,game.camera_y);player_draw();
}
int main(void){
 REG16(0x04000208)=0;REG16(0x04000204)=0x4317;REG16(0x04000000)=0x80;
 REG16(0x04000108)=0;REG16(0x0400010c)=0;REG16(0x0400010e)=0x84;REG16(0x0400010a)=0x80;
 audio_init();game_boot();title_init();rendered_scene=SCENE_TITLE;dma_bytes=0;
 REG16(0x04000000)=0x0f40;
 metrics.ready=1;
 unsigned phase=0,edges=0,last_keys=0;
 for(;;){
  /* Simulate in the visible period. Only commit VRAM/OAM in VBlank. */
  while(REG16(0x04000006)>=160){}
  uint32_t start=clock_now();dma_bytes=0;
  unsigned keys=(~REG16(0x04000130))&1023;
  edges|=keys&~last_keys;last_keys=keys;
  /* Exact nominal MVS actor rate (one tick / two MVS VBlanks),
   * mapped to GBA VBlanks, no faster game speed to disguise rendering cost. */
  phase+=2078125u;
  unsigned reloading=0,advanced=0;
  if(phase>=4194304u){
   phase-=4194304u;game_step(keys|edges);edges=0;
#if defined(COMBAT_FIXTURE)||defined(NATIVE_MISSION)
   for(unsigned n=0;n<combat.sounds;n++)audio_command(combat.sound[n]);combat_clear_sounds();
#endif
   metrics.ticks++;advanced=1;
  }
  unsigned logic_cycles=clock_now()-start;
  if(logic_cycles>metrics.logic_max_cycles)metrics.logic_max_cycles=logic_cycles;
  while(REG16(0x04000006)<160){}
  start=clock_now();
  if(advanced){
   if(rendered_scene!=(int)game.scene){
    REG16(0x04000000)|=0x80;reloading=1;metrics.reloads++;
    if(game.scene==SCENE_TITLE)title_init();else movement_init();
    rendered_scene=game.scene;
   }else if(game.scene==SCENE_MOVEMENT){
    int dx=(game.camera_x>>3)-previous_left,dy=(game.camera_y>>3)-previous_top;
    reloading=(dx>1||dx<-1||dy>1||dy<-1);
    if(reloading){REG16(0x04000000)|=0x80;metrics.reloads++;previous_left=previous_top=-999;}
    background(game.camera_x,game.camera_y);player_draw();
#ifdef COMBAT_FIXTURE
    combat_draw();
#endif
#ifdef NATIVE_MISSION
    mission_draw();
    if(campaign.unsupported){
     char line[]="PORT LIMIT - TEMPLATE 0000    ";const char*hex="0123456789ABCDEF";
     for(unsigned i=0;i<4;i++)line[22+i]=hex[(campaign.failed_template>>(12-4*i))&15];text(0,19,line);
    }else text(0,19,"                             ");
#else
    text(0,19,game.ended?"TEST END - COMBAT NOT PORTED  ":"                             ");
#endif
   }
  }
  unsigned render_cycles=clock_now()-start;
  if(!reloading&&render_cycles>metrics.render_max_cycles)metrics.render_max_cycles=render_cycles;
  metrics.last_cycles=logic_cycles+render_cycles;
  if(metrics.last_cycles>metrics.max_cycles)metrics.max_cycles=metrics.last_cycles;
  metrics.total_work_cycles+=metrics.last_cycles;
  if(!reloading){
   if(metrics.last_cycles>metrics.normal_max_cycles)metrics.normal_max_cycles=metrics.last_cycles;
   if(dma_bytes>metrics.normal_max_dma)metrics.normal_max_dma=dma_bytes;
  }
  metrics.last_dma_bytes=dma_bytes;metrics.total_dma_bytes+=dma_bytes;
  if(dma_bytes>metrics.max_dma_bytes)metrics.max_dma_bytes=dma_bytes;
  if(!reloading&&REG16(0x04000006)<160)metrics.late_vblank++;
  if(reloading){
   /* Explicit reset/warp loading. Never expose a partially replaced map. */
   while(REG16(0x04000006)>=160){}while(REG16(0x04000006)<160){}
   REG16(0x04000000)=game.scene==SCENE_TITLE?0x0f40:0x1340;
  }
  if(advanced)metrics.presents++;
 }
}
