#ifndef NATIVE_ENEMY_ACTOR_H
#define NATIVE_ENEMY_ACTOR_H
#include "weapon_animation.h"
#include "flight_path.h"
struct MissionResultActor {
 uint16_t clock,cell,count,remaining,winner,names[8],index,roll;
 int16_t target_x,target_y;
 uint8_t slot,participants,counting,bonuses,skip,done,full,blink,mode,ack;
 uint8_t lengths[8],prefixes[8];
};
/* Source storage is expressed as semantic fields, never a 68000 memory image. */
struct EnemyActor {
 struct WeaponActor actor;
 struct HitAttackSet attack_set;
 uint8_t params[8];
 uint8_t entry,family,grounded,back_hit,sequence,turn_timer,turn_interval,dead;
 uint8_t scale_x,scale_y,gib_delays,source_flags,dispatch_depth,lifecycle_flags;
 uint8_t captor_done; /* Published to the enclosing compound prop by its captor child. */
 uint8_t private_palette_flags; /*29F2 allocation +2C66 bit8, for palette rendering. */
 uint16_t selector,sound_command,scale,support_slope;
 int16_t start_y;
 union {struct {uint16_t clock,delay,index,page,stream,cell,phase;uint8_t kind,held,edges,mask,latch,reserved_layout,done;} frontend;
 struct {uint16_t cell,string,index,period,angle,clock,selection,cursor,idle,blink,name[3],name_offset,rank;uint8_t slot,mode,ack,continues,prisoners,scale_hold,direction,repeat;} result;
 struct MissionResultActor mission_result;
 struct {uint16_t next,current,cursor,timeline,frame,clock,progress;} scene_text;
 struct {int16_t remaining,target[3],step[3],accumulator[3];uint8_t initial[3],done;} rgb_fade;
 struct {int16_t timer,period,delay;uint8_t triggered[2];} scripted_input;
 struct {
  uint16_t cursor,wait,full_hp,threshold,ratio,clock,burst,overlay,palette,template_id;
  uint16_t palettes[3];int16_t offset_y;uint8_t flags[9],resume,low_ready,render_flags,variant;
 } script151;
 struct {int16_t angle,anchor_x,muzzle_x,muzzle_y;uint16_t overlay,clock;uint8_t key,owner,pose;} ring_gun;
 struct {
  int16_t origin;
  uint16_t radius,cooldown,lifetime,probability;
  uint8_t kind,engaged,angry,attack_choice,continuation,flags,missing;
  int8_t target;
  /* Ordinary soldier: source $82..$8A; $88/$8A become grab offsets. */
  uint16_t retreat,advance,fear,leap,crouch,distance;
  uint8_t mood,abilities,weapon,anchor,key,grabs,signal,taunt;
 } infantry;
 /* Allen's body/child controller overlays the unrelated infantry storage. */
 struct {
  int16_t threshold,segment,previous_hp;
  int8_t target;
  uint8_t airborne,ground_seen,stage,in_front,retreat,visible,jump_chance;
  uint8_t cooldown,sensors[4],voice_timer,blood_timer;
 } allen;
 struct {
  int16_t command,angle,speed,wall_ticks,forced,pose_timer,pose,max_hp;
  uint16_t target_ticks,target_slot,cooldown,death_ticks,smoke_left,smoke_right;
 } boss2;
 struct {
  int16_t offset_x,offset_y,travel,return_travel,min_speed,max_speed,radius,priority,speed;
  uint16_t pause;
  uint8_t no_surface,blocked,delays;
 } crab;
 struct {
  struct FlightState path;
  int16_t speed,base_speed,offset;
  uint8_t pose,hold;
 } gunship;
 struct {
  struct FlightState path;
  int16_t speed,timer,threshold,segment,target_x,target_y,timeout;
  uint16_t contact;
  uint8_t stage,previous,rotor,motion,sine,crew,side,choice,upgraded,gate;
  int8_t offset_x,offset_y,priority,count;
  uint8_t flags,target;
 } m5;
 struct {
  uint8_t history[16];
  int16_t wait,target,step;
  uint8_t driver,fled,fire,retired,ready,held;
 } turret;
 struct {
  int16_t wait,attack_wait,second_wait,priority,offset_x,offset_y;
  uint16_t part;
  uint8_t commands[5],attached,side,pose,attack_stage,attack_count,second_stage,second_count;
  uint8_t heartbeat,alive,front,head_state;
 } final;
 struct {
  struct FlightState path;
  int16_t wait,speed,turn,priority;
  uint16_t child_index,child_generation;
  uint8_t stage,pose,drive_pose,direction,count,serial,height_a,height_b;
  int8_t offset_x,offset_y;
  uint8_t signal,watch,contact,table,mode,target;
 } tank;
 struct {
  int16_t wait,speed;
  uint16_t child_index,child_generation;
  uint8_t command,ack,count,direction,offset_x,offset_y,hurt,variant,clock,target,body_collision,target_clock,attack_kind,ally_defense;
 } bazooka;
 struct {
  int16_t period,clock,bob_angle,bob_y,cockpit_x,cockpit_y,direction,initial_hp;
  uint8_t variant,with_guards;
 } carrier;
 struct {int16_t stage,max_hp,offset,direction;uint8_t side,delay_row;} carrier_part;
 struct {int16_t goal,lane,offset,arc,slot,cooldown;uint8_t guard_variant;} carrier_crew;
 struct {int16_t deaths;uint8_t busy,clocks[3],period,guards;} carrier_spawner;
 struct {int16_t pose,reload,special,second;uint8_t hidden;} carrier_gunners;
 /* Template 117: $5C/$5E are selectors; $80/$82 are root anchors or part offsets. */
 struct {
  int16_t selector,shot,timer,speed,anchor_x,anchor_y;
  uint16_t pattern,remaining;
  uint8_t flags,upgraded,direction,mask,aimed,slot,sides,bob,count,target,delay,path_step,clock;
 } boss117;
 struct {
  int16_t margin,timer,angle,ground;
  uint16_t config,pattern,remaining;
  uint8_t delay,variant,closing;
 } siege;
 struct {
  uint32_t watch_script,final_script;
  int16_t margin,timer,previous_hp;
  uint16_t patch,defense,pattern,remaining,alternate_palette;
  uint8_t variant,watch,ladder,delay;
 } fort;
 struct {
  int16_t margin,timer,angle,speed,target_x,target_y,recoil,direction,clock;
  uint16_t patch,second_patch,pattern,remaining,alternate_palette,base_palette;
  uint8_t variant,watch,ladder,delay,signal,key,ground_seen,hurt_pose;
  int8_t owner;
 } airship;
 struct {int16_t choice,retries,timer;uint8_t resume,immune,variant;} mob;
 struct {int16_t timer;uint16_t remaining;uint8_t variant,watch,hurt,shake,direction,pose,delay,pattern;} m4;
 struct {int16_t timer,speed,life,phase,drift,descent;uint16_t delays;uint8_t repeats,can_fall,fallen,angle,mask,step,direction,turns,turn_gate,wall_gate,low,variant,air_mode,air_angle,air_state,form;int8_t target;} free104;
 struct {
  int16_t timer,cooldown,period,limit,speed,angle,freeze,priority_offset;
  uint16_t clock,driver_index,driver_generation,turret_index,turret_generation,impact_palette;
  uint8_t direction,repeats,aim,temporary,stamp,delay_mode,delay_row,turn_delay,palette_cycle;
  int8_t target;
 } light_tank;
 struct {uint16_t contacts[3][2];int16_t ground,speed,wait,slope;uint8_t command,ack,count,direction,offset_x,offset_y,hurt,variant,clock,target,body_collision,pose,angle,contact;} rocket_vehicle;
 struct {int16_t timer,avoid;uint16_t clock;uint8_t direction,temporary,stamp,repeats,delay_mode,delay_row,crew_phase;} ground_nest;
 struct {
  struct FlightState path;
  int16_t timer,period,speed,dash_speed,angle,goal_angle,target_x,target_y,bob_y,life;
  uint16_t serial;
  uint8_t commands[8],bob,count,upgraded,aimed,remaining,immune_shell,command,previous,offset_x,offset_y,priority,slot,ack,hit_count,alt_shell,target,position,missiles,stop_sound;
 } boss120;
 struct {
  struct FlightState path;
  int16_t timer,base_speed,speed,brake,launch_speed,config,slot,selector;
  uint8_t pose,pilot,tilt,target,variant,delay,route;
  struct {int16_t hp[8],idle,period;uint8_t live,seen,active;} manager;
 } formation;
 struct {int16_t timer,clock,count,limit,speed,serial,stage,angle,life,drive,offset;uint16_t delays;uint8_t variant,rider,cycle,form;} assault76;
 struct {int16_t timer,wait,repeats,angle,direction,count,bob;uint8_t hurt,shake,turns,releases,locked,sound_started,release_choice,release_valid;} emitter59;
 struct {int16_t target_x,target_y,wait,timer,travel,scroll_start;uint16_t palettes[2];uint8_t cycle,pose,hits,delay,count,variant;} turret142;
 struct {int16_t offset_x,offset_y;uint8_t step,variant;} crew23;
 struct {int16_t signal,timer,scroll_trigger,amplitude;uint8_t variant,bob,wait;} grunt153;
 struct {int16_t timer,floor;uint8_t stage,attached,variant,land_alias;} nest185;
 struct {int32_t velocity_x,velocity_y,accel_x,accel_y;int16_t timer,origin;} capsule;
 struct {int16_t timer;uint8_t kind;} scene_prop;
 struct {int16_t margin,stage;uint16_t clock,pattern,remaining;uint8_t variant,latch,delay;} urban;
 struct {int16_t margin,stage;uint16_t pattern,remaining;uint8_t watch,patch_seen,signal,cooldown,delay,part;} opening;
 struct {int16_t margin,timer,launch_y;uint16_t patch,pattern,remaining;uint8_t variant,watch,delay,blink;} village;
 struct {int16_t timer;int8_t target;} flame_trap;
 struct {uint16_t period,remaining;int16_t amount,step;uint8_t down,alternate;} scene_fade;
 struct {int16_t external,command,timer;uint8_t variant;} captor;
 struct {int16_t timer,repeats,serial,threshold,limit,x,y,priority;uint16_t hurt_graph;uint8_t direction,damaged,stamp,pose,kind,grenade_hits,signal;} mortar;
 struct {int16_t mode,timer,trigger,repeats,max_hp,limit;uint16_t palettes[3];uint8_t stamp,variant,cycle,smoke_only;} walker;
 struct {int16_t idle,interval,clock,wait,offset_y,speed;uint8_t variant,repeats,serial,hit;} gun_platform;
 struct {int16_t timer,period,repeats,limit;uint8_t variant,angle;} para;
 struct {int16_t bob;uint16_t palettes[2];uint8_t repeats,target;} barrel;
 struct {int16_t timer,distance,guard,life;uint16_t child_index,child_generation,palettes[3],impact_palette;uint8_t delays,variant,pose,forward_disabled,air_count,dropped;} shield;
 struct {
  uint16_t pattern,remaining,bounds,alternate_palette;
  int16_t launch_y,bounces,initial_bounces;
  uint8_t delay,blink;
 } fx;};
};
#endif
