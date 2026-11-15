#include "boss120_internal.h"
/* $0403E4..$040EFE: two actual constructors, sharing eight independently
 * scheduled parts. State IDs are semantic, never source PCs/registers. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void b120_schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.boss120;for(unsigned i=0;i<sizeof(e->payload.enemy.boss120);i++)p[i]=255;e->payload.enemy.selector=65535;b120_schedule(e,s);}
struct NativeEntity*b120_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
struct NativeEntity*boss_radial_blast(struct WeaponWorld*w,struct NativeEntity*e,unsigned quadrant){if(quadrant>3){enemy_fault(w);return 0;}struct NativeEntity*n=b120_child(w,e,B120_RADIAL);if(n)n->payload.enemy.boss120.position=quadrant;return n;}
void b120_start_entry(struct EnemyActor*q,uint16_t n){if(n!=65535){animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}}
void b120_start(struct EnemyActor*q,unsigned n){b120_start_entry(q,boss120_entries[n]);}
void b120_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:boss120_bindings[n].defense;q->actor.defense_count=n==65535?0:boss120_bindings[n].count;}
void b120_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){boss120_bindings[n].attack,boss120_bindings[n].count};q->actor.attack=&q->attack_set;}}
int16_t b120_rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=B120R_COUNT){enemy_fault(w);return 0;}unsigned i=(interaction_rank(w,&q->actor,runtime_owner_boss120[n]))*8+(q->source_flags&3)*2;const struct Boss120RankData*t=&boss120_rank[n];if(t->choices)i=t->choices[i+((uint8_t)(r->player_count_flag-1)&1)]*8+((tk_random(w)&14)>>1);else i+=(r->player_count_flag&2)>>1;return (int16_t)t->values[i];}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read_field(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(width==2){if(f==B120F_9A_2)return q->boss120.serial;if(f==B120F_76_2)return (uint16_t)q->boss120.angle;if(f==B120F_34_2)return q->selector;}if(width==1&&f==B120F_80_1)return q->boss120.command;enemy_fault(c->w);return 0;}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;uint32_t old=read_field(ctx,f,width);if(c->w->fault)return 0;if(op==AN_FIELD_AND)v&=old;else if(op==AN_FIELD_OR)v|=old;else if(op==AN_FIELD_XOR)v^=old;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(width==1)q->boss120.command=(uint8_t)v;else if(f==B120F_9A_2)q->boss120.serial=(uint16_t)v;else if(f==B120F_76_2)q->boss120.angle=(int16_t)v;else q->selector=(uint16_t)v;return 1;}
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(boss120_bindings)/sizeof(*boss120_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)b120_defense(q,n);else if(kind==AN_ATTACK){b120_attack(q,n);hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*an){(void)an;struct Context*c=ctx;return b120_callback(c->w,c->e,n);}
int b120_animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read_field,.write=field,.bind=bind,.sound=sound,.call=call};int done=animation_step(&a->animation,&boss120_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
int b120_path(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct FlightOwner o={0};int r=flight_step(&q->actor.transform,&q->boss120.path,q->boss120.speed,&boss120_flight_program,&o);if(r<0)enemy_fault(w);q->selector=q->boss120.path.angle;return r;}
void b120_velocity(struct EnemyActor*q,uint16_t angle){int speed=q->boss120.speed&32767;if(speed<=256)speed=256;q->actor.vx=tk_word(((int32_t)boss_trig[1][angle&255]*speed)>>8);q->actor.vy=tk_word(((int32_t)boss_trig[0][angle&255]*speed)>>8);}
int b120_turn(struct EnemyActor*q,int step){int16_t v=q->boss120.angle;if(v==q->boss120.goal_angle)return 1;if(v>q->boss120.goal_angle)step=-step;q->boss120.angle=(v+step)&255;return q->boss120.angle==q->boss120.goal_angle;}
static int16_t player_x(struct WeaponWorld*w,unsigned i){struct PlayerBodyState*p=i<2?w->players[i]:0;return p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1;}
static int16_t player_y(struct WeaponWorld*w,unsigned i){struct PlayerBodyState*p=i<2?w->players[i]:0;return p?tk_word(transform_y_up(&p->actor.transform)+w->camera_y):-1;}
static unsigned target(struct WeaponWorld*w,struct EnemyActor*q){(void)q;unsigned p1=enemy_player_live(w->players[0]),p2=enemy_player_live(w->players[1]);return p1&&p2?(tk_random(w)&1):p2?1:0;}
static int choose_position(struct WeaponWorld*w,struct EnemyActor*q){unsigned i=q->boss120.position&15;if(i>=12||!w->secondary_view_valid)return enemy_fault(w);int x=tk_word(boss120_targets[i][0]-w->secondary_x_offset),y=tk_word(512-tk_word(boss120_targets[i][1]-640)+w->secondary_y_offset);if(x>272)x=272;q->boss120.target_x=x;q->boss120.target_y=y;return 1;}
static int random_position(struct WeaponWorld*w,struct EnemyActor*q){q->boss120.position=tk_random(w)&3;if(!choose_position(w,q))return 0;q->boss120.speed=boss120_speeds[tk_random(w)&3];unsigned angle=enemy_angle(tk_word(q->boss120.target_x-tk_sx(w,&q->actor)),tk_word(q->boss120.target_y-tk_sy(w,&q->actor)));q->boss120.angle=(q->boss120.angle&255)|(angle<<8);return 1;}
static int attack_position(struct WeaponWorld*w,struct EnemyActor*q){unsigned i=target(w,q);q->boss120.position=4;if(!choose_position(w,q))return 0;q->boss120.position=(q->boss120.target_y<player_y(w,i)?8:10)+(tk_random(w)&1);return choose_position(w,q);}
static int steer(struct WeaponWorld*w,struct EnemyActor*q,unsigned radius){int16_t dx=tk_word(q->boss120.target_x-tk_sx(w,&q->actor)),dy=tk_word(q->boss120.target_y-tk_sy(w,&q->actor));uint16_t distance=(uint16_t)((dx<0?-(int32_t)dx:dx)+(dy<0?-(int32_t)dy:dy));if(distance<radius){q->actor.vx=q->actor.vy=0;return 1;}uint8_t angle=enemy_angle(dx,dy);if(dy<=32)q->boss120.angle=(q->boss120.angle&255)|(angle<<8);uint8_t old=(uint16_t)q->boss120.angle>>8;if(angle!=old){int16_t step=tk_word(-tk_word(q->boss120.speed*2));if((int8_t)(angle-old)>=0)step=tk_word(-step);q->boss120.angle=tk_word(q->boss120.angle+step);angle=(uint16_t)q->boss120.angle>>8;}b120_velocity(q,angle);return 0;}
static int speed(struct EnemyActor*q,int goal,int step){q->boss120.speed=tk_word(q->boss120.speed+step);if((step>0&&q->boss120.speed>=goal)||(step<0&&q->boss120.speed<=goal)){q->boss120.speed=goal;return 1;}return 0;}
static int bob(struct WeaponWorld*w,struct NativeEntity*e,unsigned shift){struct EnemyActor*q=&e->payload.enemy;int16_t y=tk_sy(w,&q->actor);unsigned at=q->boss120.bob;q->boss120.bob+=shift==8?128:16;q->boss120.bob_y=boss_trig[0][at]>>shift;transform_integer_offset(&q->actor.transform,0,q->boss120.bob_y);int r=b120_animate(w,e);m4_screen(w,&q->actor,tk_sx(w,&q->actor),y);return r;}
static int dispatch(struct WeaponWorld*w,struct NativeEntity*e,unsigned choice){static const uint8_t states[]={B120_CRUISE,B120_DASH_APPROACH,B120_BOMB_OPEN,B120_ALIGN,B120_MISSILE_OPEN,B120_BURST_OPEN};if(choice>=6)return enemy_fault(w);b120_schedule(e,states[choice]);return 1;}
static int choose_attack(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;unsigned i=target(w,q),n;q->boss120.position=4;if(!choose_position(w,q))return 0;if(q->boss120.target_y<=player_y(w,i))n=B120R_CHOICE_HIGH;else{q->boss120.position=5;if(!choose_position(w,q))return 0;if(q->boss120.target_y<=player_y(w,i))n=B120R_CHOICE_MID;else n=tk_word(tk_sx(w,&q->actor)-32)>=player_x(w,i)?B120R_CHOICE_BACK:B120R_CHOICE_FRONT;}return dispatch(w,e,(uint16_t)b120_rank(w,q,n+(q->boss120.upgraded==255)));}
static int spawn_parts(struct WeaponWorld*w,struct NativeEntity*e){for(unsigned i=0;i<8;i++){struct NativeEntity*n=b120_child(w,e,B120_PART_NEW);if(!n)return 0;struct EnemyActor*q=&n->payload.enemy;const uint8_t*p=boss120_part_offsets[i];q->boss120.offset_x=p[0];q->boss120.offset_y=p[1];q->boss120.priority=p[2];q->boss120.slot=p[3];}return 1;}
static void tail(struct WeaponWorld*w,struct NativeEntity*e,int guarded){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(guarded)q->boss120.missiles=0;a->hit.flags&=247;if(!tk_hit_sound(w,a))return;if(a->hit.flags&1){int x=tk_sx(w,a);if(guarded&&x>=144&&x<=288&&((enemy_player_live(w->players[0])&&!(w->players[0]->actor.hit.flags&1))||(enemy_player_live(w->players[1])&&!(w->players[1]->actor.hit.flags&1))))b120_schedule(e,B120_DIE);else{a->hit.hp=1;a->hit.flags&=254;}}}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;unsigned late=q->boss120.upgraded==255;
 switch(a->state){
 case B120_ROOT:case B120_SCRIPT:{unsigned scripted=a->state==B120_SCRIPT;a->palette=0x9a;a->hit_palette=0x87;interaction_priority(w,a,0x4000,20);if(!scripted){q->boss120.upgraded=0;a->hit.hp=b120_rank(w,q,B120R_HP);n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_DAMAGE_SMOKE_BODY);if(!n)return 0;n->payload.enemy.actor.attached_offset=0;}if(!spawn_parts(w,e))return 0;for(unsigned i=0;i<8;i++)q->boss120.commands[i]=128;b120_start(q,B120A_BODY);if(scripted){q->boss120.speed=1024;q->boss120.bob_y=0;m4_screen(w,a,384,480);q->boss120.path=(struct FlightState){.cursor=boss120_paths[0]};a->state=B120_SCRIPT_PATH;}else{q->boss120.timer=60;a->state=B120_DELAY;}break;}
 case B120_ENTER:if(!tk_sound(w,0x10a2))return 0;q->selector=0xc000;q->boss120.speed=512;m4_screen(w,a,352,352);q->boss120.target_x=240;q->boss120.target_y=384;break;
 case B120_DECEL:q->boss120.speed=512;break;
 case B120_OPEN:q->boss120.speed=256;q->dead=0;q->boss120.angle=0;q->boss120.timer=45;break;
 case B120_CRUISE:q->boss120.timer=b120_rank(w,q,B120R_WAIT+late);if(!random_position(w,q))return 0;q->dead=0;break;
 case B120_DASH_APPROACH:q->boss120.dash_speed=b120_rank(w,q,B120R_DASH_SPEED+late);q->boss120.angle=(int16_t)0xc000;if(!attack_position(w,q))return 0;q->boss120.speed=0;q->dead=0;q->boss120.commands[4]=0x85;break;
 case B120_DASH_BRAKE:break;
 case B120_DASH_WAIT:q->boss120.timer=b120_rank(w,q,B120R_DASH_WAIT+late);q->boss120.period=b120_rank(w,q,B120R_DASH_RELOAD+late);q->boss120.count=b120_rank(w,q,B120R_DASH_COUNT+late);q->dead|=16;break;
 case B120_DASH_REPEAT:q->dead=0;q->boss120.count--;q->boss120.commands[4]=0x86;q->boss120.timer=q->boss120.period;a->state=B120_DASH_WAIT;break;
 case B120_DASH_RETURN:q->boss120.angle=(int16_t)0xc000;q->boss120.target_x=256;q->boss120.target_y=400;q->boss120.commands[4]=0x84;break;
 case B120_BOMB_OPEN:q->boss120.commands[2]=4;q->boss120.commands[5]=9;q->dead=0;break;
 case B120_BOMB_EXIT:q->boss120.angle=(int16_t)0xc000;q->boss120.target_x=288;q->boss120.target_y=512;break;
 case B120_BOMB_RUN:{unsigned i=(uint16_t)b120_rank(w,q,B120R_BOMB_CONFIG+late);if(i>=8)return enemy_fault(w);q->boss120.life=boss120_bomb_configs[i][0];q->boss120.speed=boss120_bomb_configs[i][1];a->vx=tk_word(-q->boss120.speed);q->boss120.timer=0;q->dead|=32;break;}
 case B120_REENTER_WAIT:q->boss120.commands[2]=128;m4_screen(w,a,352,tk_sy(w,a));q->boss120.target_x=352;q->boss120.target_y=tk_sy(w,a);q->boss120.timer=60;break;
 case B120_ALIGN:q->boss120.target=target(w,q);q->boss120.commands[4]=0x84;break;
 case B120_SHOT:q->dead=0;q->boss120.commands[4]=0x86;break;
 case B120_SHOT_RESET:q->dead=0;q->boss120.commands[4]=0x84;b120_schedule(e,B120_CRUISE);return 2;
 case B120_MISSILE_OPEN:q->boss120.commands[0]=0;q->dead=0;break;
 case B120_MISSILE_PREP:q->boss120.count=b120_rank(w,q,B120R_MISSILE_COUNT+late);q->boss120.period=b120_rank(w,q,B120R_MISSILE_RELOAD+late);a->state=B120_MISSILE_NEXT;/* FALLTHROUGH */
 case B120_MISSILE_NEXT:if((int8_t)q->boss120.count<=0){q->boss120.commands[0]=1;a->state=B120_MISSILE_CLOSE;}else{q->boss120.count--;q->boss120.timer=q->boss120.period;q->boss120.commands[0]=11;a->state=B120_MISSILE_WAIT;}break;
 case B120_BURST_OPEN:q->dead=0;q->boss120.commands[3]=6;break;
 case B120_BURST_WAIT:q->dead=0;q->boss120.commands[3]=0x81;break;
 case B120_DIE:w->enemies->damage_enabled=0;w->enemies->scene_flags[0]=w->enemies->scene_flags[1]=0;b120_defense(q,65535);q->boss120.commands[4]=128;if(!b120_child(w,e,B120_WRECK))return 0;n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;interaction_priority(w,&n->payload.enemy.actor,0xd000,28);for(unsigned i=0;i<4;i++){n=b120_child(w,e,B120_RADIAL);if(!n)return 0;n->payload.enemy.boss120.position=i;n->payload.enemy.actor.transform.priority=0xc000;}q->boss120.timer=30;q->boss120.stop_sound=0;m4_reset(a);a->state=B120_FALL;break;
 case B120_EXPLODE:{w->enemies->scripted_player_lock=0;int16_t old=tk_sx(w,a);m4_screen(w,a,288,tk_sy(w,a));if(!tk_sound(w,0x1033))return 0;w->enemies->scene_flags[0]=4;w->enemies->scene_flags[1]=0;if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE))return 0;for(unsigned i=0;i<17;i++)if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_METAL_HEAVY))return 0;m4_screen(w,a,old,tk_sy(w,a));a->state=B120_RETIRE;/* source falls into retirement */}
 /* FALLTHROUGH */
 case B120_RETIRE:for(unsigned i=0;i<8;i++)q->boss120.commands[i]=0x8a;entity_delete(w->entities,e);return 0;
 case B120_SCRIPT_OPEN:q->dead=0;q->boss120.timer=45;q->boss120.commands[1]=2;q->boss120.commands[4]=8;break;
 case B120_SCRIPT_SHOT:q->boss120.timer=15;q->dead=0;q->boss120.commands[4]=0x88;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int move=WMOVE_SYNC,shift=7,guarded=1;
 switch(s){
 case B120_DELAY:if((q->boss120.timer=tk_word(q->boss120.timer-1))<=0)b120_schedule(e,B120_ENTER);return;
 case B120_ENTER:if(steer(w,q,20))b120_schedule(e,B120_DECEL);if(interaction_move(w,a,WMOVE_POINT)<0)return;bob(w,e,7);return;
 case B120_DECEL:if(speed(q,0,-32))b120_schedule(e,B120_OPEN);if(steer(w,q,8))b120_schedule(e,B120_OPEN);if(interaction_move(w,a,WMOVE_POINT)<0)return;bob(w,e,7);return;
 case B120_OPEN:
  /* Native correction:40550 prepares1 inD0, whereas41CE0 consumesD5.
   * Pass the authored1 explicitly. Camera backscroll can reach this arm. */
  if(tk_sx(w,a)>272){if(steer(w,q,1))q->boss120.target_y^=16;if(interaction_move(w,a,WMOVE_POINT)<0)return;}
  else if(interaction_move(w,a,WMOVE_SYNC)<0)return;
  bob(w,e,6);q->boss120.timer=tk_word(q->boss120.timer-1);if(!q->boss120.timer){q->boss120.commands[1]=2;q->boss120.commands[4]=8;}if(q->dead&16){q->boss120.commands[4]=0x82;b120_defense(q,B120B_BODY);b120_schedule(e,B120_CRUISE);}tail(w,e,1);return;
 case B120_CRUISE:if(steer(w,q,8)&&!random_position(w,q))return;if(interaction_move(w,a,WMOVE_POINT)<0)return;bob(w,e,6);q->boss120.timer=tk_word(q->boss120.timer-1);if(q->boss120.timer<=0&&tk_sx(w,a)<=272&&!q->boss120.missiles&&!choose_attack(w,e))return;if(b120_rank(w,q,B120R_THRESHOLD)>=a->hit.hp)q->boss120.upgraded=255;tail(w,e,1);return;
 case B120_DASH_APPROACH:case B120_DASH_BRAKE:
  speed(q,s==B120_DASH_APPROACH?q->boss120.dash_speed:0,s==B120_DASH_APPROACH?(uint16_t)q->boss120.dash_speed>>5:-((uint16_t)q->boss120.dash_speed>>5));if(!choose_position(w,q))return;if(steer(w,q,s==B120_DASH_APPROACH?32:4)&&(s==B120_DASH_APPROACH||(q->dead&16)))b120_schedule(e,s==B120_DASH_APPROACH?B120_DASH_BRAKE:B120_DASH_WAIT);if(interaction_move(w,a,WMOVE_POINT)<0)return;b120_animate(w,e);q->boss120.bob_y=0;tail(w,e,1);return;
 case B120_DASH_WAIT:q->boss120.timer=tk_word(q->boss120.timer-1);if(q->boss120.timer<=0&&(q->dead&16)){if((int8_t)q->boss120.count>0){/* $040768 is same-tick, including the next decrement. */q->dead=0;q->boss120.count--;q->boss120.commands[4]=0x86;q->boss120.timer=tk_word(q->boss120.period-1);}else b120_schedule(e,B120_DASH_RETURN);}break;
 case B120_DASH_RETURN:speed(q,512,32);if(!choose_position(w,q))return;if(steer(w,q,8))b120_schedule(e,B120_CRUISE);if(interaction_move(w,a,WMOVE_POINT)<0)return;b120_animate(w,e);q->boss120.bob_y=0;tail(w,e,1);return;
 case B120_BOMB_OPEN:if(q->dead&4)b120_schedule(e,B120_BOMB_EXIT);break;
 case B120_BOMB_EXIT:speed(q,512,32);if(steer(w,q,16))b120_schedule(e,B120_BOMB_RUN);q->boss120.bob_y=0;move=WMOVE_POINT;guarded=0;break;
 case B120_BOMB_RUN:q->boss120.timer=tk_word(q->boss120.timer-1);if(q->boss120.timer<=0&&(q->dead&32)){q->boss120.timer=q->boss120.life;/* MOVE.B #13 leaves Z clear, so the following BEQ never skips #14. */if(q->boss120.timer>18)q->boss120.commands[5]=13;q->boss120.commands[5]=14;}if(interaction_move(w,a,WMOVE_POINT)<0)return;bob(w,e,7);if(tk_sx(w,a)<=-30)b120_schedule(e,B120_REENTER_WAIT);tail(w,e,0);return;
 case B120_REENTER_WAIT:if(q->boss120.timer<=30){if(!random_position(w,q))return;a->state=B120_REENTER;}/* FALLTHROUGH */
 case B120_REENTER:steer(w,q,8);q->boss120.commands[5]=128;if(interaction_move(w,a,WMOVE_POINT)<0)return;bob(w,e,6);q->boss120.timer=tk_word(q->boss120.timer-1);if(q->boss120.timer<=0&&!dispatch(w,e,(uint16_t)b120_rank(w,q,B120R_REENTRY+(q->boss120.upgraded==255))))return;tail(w,e,0);return;
 case B120_ALIGN:if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(tk_word(tk_sx(w,a)-32)<=player_x(w,q->boss120.target)){transform_integer_offset(&a->transform,1,0);bob(w,e,6);}else{bob(w,e,6);if(q->boss120.aimed==1)b120_schedule(e,B120_SHOT);}tail(w,e,1);return;
 case B120_SHOT:if(q->dead&16)b120_schedule(e,B120_SHOT_RESET);shift=6;break;
 case B120_MISSILE_OPEN:if(q->dead&1)b120_schedule(e,B120_MISSILE_PREP);break;
 case B120_MISSILE_WAIT:if(interaction_move(w,a,WMOVE_SYNC)<0)return;bob(w,e,7);if((q->boss120.timer=tk_word(q->boss120.timer-1))<=0)b120_schedule(e,B120_MISSILE_NEXT);tail(w,e,1);return;
 case B120_MISSILE_CLOSE:if(interaction_move(w,a,WMOVE_SYNC)<0)return;bob(w,e,7);if(q->dead&1)b120_schedule(e,B120_CRUISE);tail(w,e,1);return;
 case B120_BURST_OPEN:if(q->dead&8)b120_schedule(e,B120_BURST_WAIT);shift=6;break;
 case B120_BURST_WAIT:if(q->dead&8){q->boss120.commands[3]=128;b120_schedule(e,B120_CRUISE);}shift=6;break;
 case B120_FALL:if(!q->boss120.stop_sound&&interaction_outside(w,a,boss120_bounds[0],0)){if(!w->sound||!w->sound(w->scene,0x10a2,1)){enemy_fault(w);return;}q->boss120.stop_sound=1;}if(interaction_move(w,a,WMOVE_FALL)>0)b120_schedule(e,B120_EXPLODE);bob(w,e,8);q->boss120.timer=tk_word(q->boss120.timer-1);if(!q->boss120.timer){a->vx=192;a->vy=-64;if(!tk_sound(w,0x1032)||!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_BOSS120_DEATH))return;}if(tk_sx(w,a)>=288)b120_schedule(e,B120_EXPLODE);return;
 case B120_SCRIPT_PATH:if(b120_path(w,e)>0)b120_schedule(e,B120_SCRIPT_OPEN);bob(w,e,6);return;
 case B120_SCRIPT_OPEN:if(interaction_move(w,a,WMOVE_SYNC)<0)return;bob(w,e,6);if((q->boss120.timer=tk_word(q->boss120.timer-1))<=0&&(q->dead&16))b120_schedule(e,B120_SCRIPT_SHOT);return;
 case B120_SCRIPT_SHOT:if((q->dead&16)&&(q->boss120.timer=tk_word(q->boss120.timer-1))<=0){q->dead=0;q->boss120.commands[4]=0x89;a->state=B120_SCRIPT_EXIT;}/* FALLTHROUGH */
 case B120_SCRIPT_EXIT:if(interaction_move(w,a,WMOVE_SYNC)<0)return;bob(w,e,6);if(interaction_outside(w,a,boss120_bounds[0],1))b120_schedule(e,B120_DELETE_TREE);return;
 default:enemy_fault(w);return;
 }if(interaction_move(w,a,(enum WeaponMove)move)<0)return;bob(w,e,shift);tail(w,e,guarded);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;for(unsigned budget=0;budget<8;budget++){unsigned s=q->actor.state;if(s==B120_DELETE||s==B120_DELETE_TREE){if(s==B120_DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}if(q->entry){q->entry=0;int r=s<B120_PART_NEW?enter(w,e):s<B120_WRECK?b120_part_enter(w,e):b120_effect_enter(w,e);if(!r||w->fault)return;if(r==2)continue;}if(e->deleting||!e->active)return;s=q->actor.state;if(s<B120_PART_NEW)frame(w,e,s);else if(s<B120_WRECK)b120_part_frame(w,e,s);else b120_effect_frame(w,e,s);return;}enemy_fault(w);}
int boss120_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id!=120&&id!=121)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,id==120?B120_ROOT:B120_SCRIPT);if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;}
