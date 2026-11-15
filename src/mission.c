#include "mission.h"
#include "../generated/mission_data.h"

static uint16_t word(unsigned p){return (uint16_t)((mission_data[p]<<8)|mission_data[p+1]);}
static int16_t difference(int a,int b){return (int16_t)((unsigned)a-(unsigned)b);}
static void fault(struct Mission*m,unsigned code,unsigned at){
 if(!m->fault){m->fault=(uint16_t)code;m->fault_source=mission_source((uint16_t)at);}
}
/* $05E9B6 and $05E9E4 are DISTINCT generators sharing the same ring.
 * Only the bounded form rotates the selected word before XOR. */
uint16_t source_random_word(struct SourceRng*r){
 r->head=(r->head+2)&62;
 unsigned value=r->words[r->head>>1];
 unsigned tap=((r->head-21)&62)>>1;
 r->words[tap]^=(uint16_t)value;
 return r->words[tap];
}
void source_random_seed(struct SourceRng*r,uint16_t seed){
 /*5E998 stores BEFORE the multiply/add and resets the byte-offset head. */
 r->head=0;for(unsigned i=0;i<32;i++){r->words[i]=seed;seed=(uint16_t)((uint32_t)seed*0xb7c7u+0x81f5u);}
}
uint16_t source_random(struct SourceRng*r,uint16_t count){
 r->head=(r->head+2)&62;unsigned value=r->words[r->head>>1],tap=((r->head-21)&62)>>1;
 r->words[tap]^=(uint16_t)((value<<1)|(value>>15));return (uint16_t)(((uint32_t)r->words[tap]*count)>>16);
}
/* $05EA1C shares the rotated transition, then masks instead of scaling. */
uint16_t source_random_mask(struct SourceRng*r,uint16_t mask){
 r->head=(r->head+2)&62;unsigned value=r->words[r->head>>1],tap=((r->head-21)&62)>>1;
 r->words[tap]^=(uint16_t)((value<<1)|(value>>15));return r->words[tap]&mask;
}
uint16_t mission_skip(uint16_t offset){
 if((offset&1)||offset>=MISSION_DATA_SIZE)return 65535;
 if(mission_data[offset]==12&&mission_next[offset>>1]!=65535)return offset; /* $0447C4: RTS */
 return mission_next[offset>>1];
}
uint32_t mission_scene_source(unsigned scene){
 if(scene>=MISSION_SCENE_COUNT)return 0;
 uint32_t source=mission_scene_sources[scene];
 return source==0xf1cd4u?MISSION_REGION_FINAL:source;
}
uint16_t mission_offset(uint32_t source){
 if(source==MISSION_EMPTY_SOURCE)return MISSION_EMPTY_OFFSET;
 unsigned at=source-MISSION_SOURCE_BASE;
 return at<MISSION_EMPTY_OFFSET&&mission_skip((uint16_t)at)!=65535?(uint16_t)at:65535;
}
uint32_t mission_source(uint16_t at){return at==MISSION_EMPTY_OFFSET?MISSION_EMPTY_SOURCE:MISSION_SOURCE_BASE+at;}
void mission_init(struct Mission*m,uint16_t seed){
 *m=(struct Mission){0};
 /* Native boot seed; subsequent RNG transitions are source exact. */
 for(unsigned n=0;n<32;n++){seed=(uint16_t)(seed*109u+1021u);m->rng.words[n]=seed;}
}
static int allocate(struct Mission*m,unsigned at){
 for(unsigned n=0;n<MISSION_TASKS;n++)if(!m->tasks[n].active){
  m->tasks[n]=(struct MissionTask){.offset=(uint16_t)at,.active=1};return (int)n;
 }
 fault(m,2,at);return -1;
}
int mission_start(struct Mission*m,uint32_t source){
 unsigned at=mission_offset(source);
 if(at==65535){fault(m,1,at);m->fault_source=source;return -1;}
 return allocate(m,at);
}
/* $0448A6: player-count/Slug, subtype and coordinate flags; NOT BIOS rank.
 * Native output is world Y
 * down, whereas the source entity stores screen Y up with an origin at 512. */
int mission_decode_spawn(const struct MissionInput*in,uint16_t at,struct MissionSpawn*out){
 if(mission_skip(at)==65535||mission_data[at]!=0)return -1;
 unsigned flags=mission_data[at+1],level=flags&3;
 if(level && (level==3?!in->slug_present:level!=in->player_count))return 0;
 int sx=in->secondary_valid?in->secondary_x:in->camera_x;
 int sy=in->secondary_valid?in->secondary_y:in->camera_y;
 *out=(struct MissionSpawn){.source=MISSION_SOURCE_BASE+at,.template=word(at+4),
  .x=(int16_t)(word(at+6)+in->camera_x-((flags&128)?0:sx)),
  .y=(int16_t)(word(at+8)+in->camera_y-((flags&64)?0:sy)),
  .counted=!(flags&16),.subtype=(flags>>2)&3};
 for(unsigned n=0;n<8;n++)out->params[n]=mission_data[at+10+n];
 return 1;
}
static unsigned choose(const struct MissionInput*in,unsigned op,unsigned at){
 int v=0,w=0,bound=(int16_t)word(at+4),local=mission_data[at+1];
 const struct MissionPlayer*a=&in->players[0],*b=&in->players[1];
 if(op==4){ /* $04446C: Y of the RIGHTMOST active player. */
  int ax=a->active?a->x:0,bx=b->active?b->x:0;
  v=ax>=bx?(a->active?a->y:512):(b->active?b->y:512);
  if(!local)v=(int16_t)(v+in->camera_y);
  return difference(v,bound)>=0;
 }
 if(op==10){ /* $0444E8: unsigned minimum screen X, then signed subtraction. */
  unsigned ax=a->active?(uint16_t)a->x:65535,bx=b->active?(uint16_t)b->x:65535;
  v=(int16_t)(ax<=bx?ax:bx);if(!local)v=(int16_t)(v+in->camera_x);
  return difference(v,bound)>=0;
 }
 /* $04455A: absent Y substitutes the other player's Y. The actual ROM uses
  * OR D0,D7 at $445E6 (not D1). Preserve that observable asymmetry. */
 v=a->active?512-a->y:0;w=b->active?512-b->y:0;
 if(!v)v=w;if(!w)w=v;v=512-v;w=512-w;
 if(!local){v=(int16_t)(v+in->camera_y);w=(int16_t)(w+in->camera_y);}
 int va=difference(v,bound)<0,vb=difference(w,bound)<0;
 return va&&vb?0:va?1:2;
}
static void command_fault(struct MissionExecutor*m,unsigned code,unsigned at){
 if(!m->fault){m->fault=(uint16_t)code;m->fault_source=mission_source((uint16_t)at);}
}
static uint16_t execute(struct MissionExecutor*m,struct MissionTask*t,const struct MissionInput*in,
 uint16_t at,unsigned depth){
 uint16_t next=mission_skip(at);
 if(next==65535||depth>=16){command_fault(m,1,at);return at;}
 m->executed++;
 unsigned op=mission_data[at],child=at;
 if(op==0){
  struct MissionSpawn s;int result=mission_decode_spawn(in,at,&s);
  if(result>0){if(!m->spawn||!m->spawn(m->context,&s)){command_fault(m,3,at);return at;}m->spawned++;}
 }else if(op==1){ /* $0447C6: initial delay and repeat period are distinct. */
  struct MissionTask q={.offset=(uint16_t)(at+10),.active=1,.periodic=1,
   .delay=(uint16_t)(mission_data[at+5]<<(mission_data[at+8]?5:0)),
   .period=(uint16_t)(mission_data[at+6]<<(mission_data[at+9]?5:0)),
   .repeats=mission_data[at+1],.enemy_limit=mission_data[at+7],
   .until=(uint16_t)(t->threshold+(mission_data[at+4]<<4))};
  if(!m->periodic||!m->periodic(m->context,&q)){command_fault(m,2,at);return at;}
 }else if(op==2){ /* Despite the upstream name "SkipTo0D", it EXECUTES children. */
  child=at+4;while(child<(unsigned)next-2&&!m->fault&&t->active){
   unsigned previous=child;child=execute(m,t,in,(uint16_t)child,depth+1);
   if(child==previous){command_fault(m,4,child);break;}
  }
 }else if(op==3){
  unsigned count=0;child=at+4;
  while(child<(unsigned)next-2){count++;child=mission_skip((uint16_t)child);if(child==65535){command_fault(m,1,at);return at;}}
  if(count){if(!m->rng){command_fault(m,5,at);return at;}
   unsigned selected=count-1-source_random(m->rng,(uint16_t)count); /* reverse stack order */
   child=at+4;while(selected--)child=mission_skip((uint16_t)child);
   execute(m,t,in,(uint16_t)child,depth+1);
  }
 }else if(op==4||op==10||op==11){
  unsigned selected=choose(in,op,at);child=at+6;
  while(selected--)child=mission_skip((uint16_t)child);
  execute(m,t,in,(uint16_t)child,depth+1);
 }else if(op==5){t->paused=1;m->edge_latch=1;}
 else if(op==6)t->paused=1;
 /* $04444C compares the LOW BYTE. A failed gate retains the cursor;
  * success advances without changing pause or the global edge latch. */
 else if(op==7){if((uint8_t)in->enemies>mission_data[at+1])return at;}
 else if(op==8){t->paused=0;m->edge_latch=0;}
 /* $09 is literally LEA 4(A1),A1; RTS: it does NOT clear paused. */
 else if(op==12)t->active=0;
 return m->fault?at:next;
}
uint16_t mission_command(struct MissionExecutor*m,struct MissionTask*t,const struct MissionInput*in,uint16_t at){
 return execute(m,t,in,at,0);
}
void mission_task_frame(struct MissionExecutor*m,struct MissionTask*t,const struct MissionInput*in){
 if(m->fault||!t->active)return;
 if(t->periodic){
  if(!in->active||in->progress>=t->until){t->active=0;return;}
  if(t->delay){t->delay--;return;}
  if(t->enemy_limit!=255&&in->enemies>t->enemy_limit)return;
  mission_command(m,t,in,t->offset);if(!t->active||m->fault)return;
  t->delay=t->period;if(t->repeats&&!--t->repeats)t->active=0;
 }else{
  for(unsigned budget=0;t->active&&!m->fault;budget++){
   unsigned at=t->offset;
   if(budget>=256){command_fault(m,4,at);break;}
   if(mission_skip((uint16_t)at)==65535){command_fault(m,1,at);break;}
   unsigned threshold=word(at+2);
   if(!t->paused){if(threshold==65535)break;t->threshold=(uint16_t)threshold;if(threshold>in->progress)break;}
   else {t->delay=t->delay?(uint16_t)(t->delay-1):(uint16_t)threshold;if(t->delay)break;}
   t->offset=mission_command(m,t,in,(uint16_t)at);
  }
 }
}
struct LegacyMissionBridge {struct Mission*m;MissionSpawnFn spawn;void*user;};
static int legacy_spawn(void*context,const struct MissionSpawn*s){
 struct LegacyMissionBridge*b=context;return !b->spawn||b->spawn(b->user,s);
}
static int legacy_periodic(void*context,const struct MissionTask*q){
 struct LegacyMissionBridge*b=context;int slot=allocate(b->m,q->offset);
 if(slot<0)return 0;b->m->tasks[slot]=*q;return 1;
}
static struct MissionExecutor legacy_executor(struct LegacyMissionBridge*b){
 return (struct MissionExecutor){.rng=&b->m->rng,.context=b,.spawn=legacy_spawn,.periodic=legacy_periodic,
  .executed=b->m->executed,.spawned=b->m->spawned,.fault_source=b->m->fault_source,
  .fault=b->m->fault,.edge_latch=b->m->edge_latch};
}
static void legacy_finish(struct Mission*m,const struct MissionExecutor*x){
 m->executed=x->executed;m->spawned=x->spawned;m->edge_latch=x->edge_latch;
 if(!m->fault){m->fault=x->fault;m->fault_source=x->fault_source;}
}
uint16_t mission_exec(struct Mission*m,struct MissionTask*t,const struct MissionInput*in,
 uint16_t at,MissionSpawnFn spawn,void*user){
 struct LegacyMissionBridge b={m,spawn,user};struct MissionExecutor x=legacy_executor(&b);
 uint16_t next=mission_command(&x,t,in,at);legacy_finish(m,&x);return next;
}
void mission_tick(struct Mission*m,const struct MissionInput*in,MissionSpawnFn spawn,void*user){
 if(m->fault)return;
 /* Retain the released M1 adapter's next-tick allocation policy. The new
  * mission_events module instead uses original first-child entity ordering. */
 uint8_t active[MISSION_TASKS];for(unsigned n=0;n<MISSION_TASKS;n++)active[n]=m->tasks[n].active;
 struct LegacyMissionBridge b={m,spawn,user};struct MissionExecutor x=legacy_executor(&b);
 for(unsigned n=0;n<MISSION_TASKS&&!x.fault;n++)if(active[n])mission_task_frame(&x,&m->tasks[n],in);
 legacy_finish(m,&x);
}
