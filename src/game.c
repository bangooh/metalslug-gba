#include "game.h"
#include "../generated/assets.h"
#if defined(COMBAT_FIXTURE)||defined(NATIVE_MISSION)
#include "combat.h"
#endif
#ifdef NATIVE_MISSION
#include "campaign.h"
#include "terrain_edits.h"
#endif
struct Game game;
static int clamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}

/* $0267F4/$0267F8: signed WORD comparisons. */
int16_t source_clamp_velocity(int16_t v,int16_t limit){return (int16_t)clamp(v,-limit,limit);}
/* $026814..$026836: WORD addition wraps before the signed clamp. */
int16_t source_velocity_step(int16_t v,int16_t a){
 return source_clamp_velocity((int16_t)((uint16_t)v+(uint16_t)a),0x800);
}
/* $027DB2: player excludes actor-only cells, filter bit $40 and redirects.
 * This variant addresses immutable world maps directly, without the Neo Geo
 * 512-pixel collision VRAM ring. Scene edits will use a separate sparse map. */
int terrain_shape(int x,int y){
 if(x<0||y<0||x>=WORLD_W||y>=WORLD_H)return 0;
 unsigned index=(y>>3)*CELL_W+(x>>3);
#ifdef NATIVE_MISSION
 unsigned raw=terrain_cell(index);
#else
 unsigned raw=terrain_cells[index];
#endif
 if((raw&0xc0)==0xc0)return 0;
 unsigned shape=raw&63;
 for(unsigned n=0;n<64;n++){
  const struct TerrainShape*s=&terrain_shapes[shape];
  if(!(s->filter&64)||s->replacement==255)return (int)shape;
  raw=s->replacement;if((raw&0xc0)==0xc0)return 0;shape=raw&63;
 }
 return 0;
}
/* $027E62: row bitmap, bit 7 is the leftmost pixel. */
int terrain_solid(int x,int y,int platforms){
 const struct TerrainShape*s=&terrain_shapes[terrain_shape(x,y)];
 return (platforms||!s->oneway) && !!(s->rows[y&7]&(128u>>(x&7)));
}
int terrain_floor(int x,int lo,int hi){
 if(lo<0)lo=0;if(hi>=WORLD_H)hi=WORLD_H-1;
 int previous=terrain_solid(x,lo-1,1);
 for(int y=lo;y<=hi;y++){
  int solid=terrain_solid(x,y,1);
  if(solid&&!previous)return y;
  previous=solid;
 }
 return -1;
}
static int wall(int x,int feet,int air){
 if(air){ /* $0274CC: current feet height before vertical movement. */
  switch(terrain_shape(x,feet-1)){
   case 1:case 12:case 13:case 14:case 15:case 16:case 38:case 39:return 1;
  }
 }
 for(int y=feet-28;y<=feet-12;y+=4)if(terrain_solid(x,y,0))return 1;
 return 0;
}
void game_reset(void){
 game=(struct Game){0};
 game.scene=SCENE_MOVEMENT;
 struct Player*p=&game.player;p->x=64*256;
 int floor=terrain_floor(64,128,415);
 p->y=(floor<0?340:floor)*256;p->grounded=floor>=0;
 game.camera_y=108;
#ifdef COMBAT_FIXTURE
 combat_fixture_init();
#endif
#ifdef NATIVE_MISSION
 campaign_reset();
#endif
}
void game_boot(void){game=(struct Game){0};}
static unsigned anim_frame(unsigned time,const uint8_t*hold,unsigned n){
 unsigned f=0;while(f+1<n&&time>=hold[f])time-=hold[f++];return f;
}
void game_step(unsigned keys){
 struct Player*p=&game.player;
 unsigned pressed=keys&~game.previous_keys;game.previous_keys=(uint16_t)keys;
 if(game.scene==SCENE_TITLE){
  game.ticks++;
  if(pressed&(KEY_START|KEY_A)){game_reset();game.previous_keys=(uint16_t)keys;}
  return;
 }
 if(pressed&KEY_SELECT){game_boot();game.previous_keys=(uint16_t)keys;return;}
 if(pressed&KEY_START){game_reset();game.previous_keys=(uint16_t)keys;return;}
#ifdef NATIVE_MISSION
 if(campaign.unsupported){campaign_step(p,0,0);return;}
 if(combat.player_dead){campaign_step(p,0,0);return;}
#endif
 game.ticks++;
 int direction=!!(keys&KEY_RIGHT)-!!(keys&KEY_LEFT);
 if(direction)p->facing=direction<0;
 if(p->grounded)p->vx=(int16_t)(direction*0x300); /* $034BA4/$034D9E */
 int jump=p->jump_pending&&p->grounded;p->jump_pending=0;
 if((pressed&KEY_B)&&p->grounded)p->jump_pending=1;
 if(jump){ /* $036A16: source constants and five-tick short-jump window. */
  p->vy=0x9cd;p->jump_window=5;p->jump_held=0;p->grounded=0;
  p->jump_kind=p->vx!=0;p->air_time=0;p->from_jump=1;game.jumps++;
 }
 if(!p->grounded){ /* $036A70 + $0324D0, clamp BEFORE integrate. */
  int16_t ax=(int16_t)(direction*0x180);
  if(!direction)ax=p->vx>0?-0x18:p->vx<0?0x18:0;
  p->vx=source_velocity_step(source_clamp_velocity(p->vx,0x300),ax);
 }
 int old_x=p->x, old_y=p->y;
 p->x=clamp(p->x+p->vx,((game.camera_x*4/3)+8)*256,(MOVEMENT_TEST_END-16)*256);
 if(wall(p->x>>8,p->y>>8,!p->grounded))p->x=old_x;
 if(p->grounded){
  int floor=terrain_floor(p->x>>8,(p->y>>8)-8,(p->y>>8)+8);
  if(floor>=0)p->y=floor*256;
  else {p->grounded=0;p->vy=0;p->from_jump=0;p->air_time=0;p->jump_window=0;}
 }
 if(!p->grounded){
  if(p->jump_window){
   p->jump_window--;
   if(p->jump_window==4&&(keys&KEY_B))p->jump_held=1;
   if(!p->jump_window&&!p->jump_held)p->vy=(int16_t)(p->vy>>2);
  }
  int16_t limit=p->from_jump?0x780:0x400;
  if(p->vy<0)p->vy=source_clamp_velocity(p->vy,limit);
  p->vy=source_velocity_step(p->vy,p->from_jump?-0xc1:-0x80);
  p->y-=p->vy;
  if(p->vy<=0){
   int lo=old_y>>8;
   /* A diagonal sweep can enter a rising slope ABOVE the previous Y.
    * The source floor/slope probes ($026D76..$027020) resolve this before
    * committing motion. Extend only for a new SOLID slope, never a one-way
    * platform entered from below. Wall-cell classes were rejected above. */
   if(terrain_solid(p->x>>8,lo-1,0)&&!terrain_solid(old_x>>8,lo-1,0))lo-=8;
   int floor=terrain_floor(p->x>>8,lo,(p->y>>8));
   if(floor>=0){p->y=floor*256;p->vy=0;p->grounded=1;p->anim=0;game.landings++;}
  }
 }
 if(p->y>WORLD_H*256){game_reset();game.resets++;return;}
 if(p->grounded){
  static const uint8_t idle[]={5,4,3,4,3,2};
  p->frame=direction?(uint16_t)(6+p->anim%12):(uint16_t)anim_frame(p->anim%21,idle,6);p->anim++;
 }else{
  static const uint8_t standing[]={2,2,2,2,2,10,2,2,2,2,2},running[]={3,3,3,10,5,5};
  p->frame=(uint16_t)((p->jump_kind?29:18)+anim_frame(p->air_time,p->jump_kind?running:standing,p->jump_kind?6:11));
  if(p->air_time<120)p->air_time++;
 }
 int cx=(p->x*3/4>>8)-110;
 if(cx>game.camera_x)game.camera_x=clamp(cx,0,MOVEMENT_TEST_END*3/4-240);
 /* Temporary camera adapter, NOT the full $0434F8 camera/scene program. */
 if(p->grounded){int cy=clamp((p->y*3/4>>8)-136,0,WORLD_H*3/4-160);
  game.camera_y+=clamp(cy-game.camera_y,-2,2);}
 game.ended=(p->x>>8)>=MOVEMENT_TEST_END-16; /* test boundary, never mission complete */
#ifdef COMBAT_FIXTURE
 combat_step(p,keys,pressed);
#endif
#ifdef NATIVE_MISSION
 campaign_step(p,keys,pressed);
#endif
}
