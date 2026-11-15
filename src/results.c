#include "result_internal.h"
#include "bosses.h"
#include "items.h"
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct EnemyActor*q,unsigned state){enemy_schedule(q,state);}
static void erase(struct WeaponWorld*w,struct NativeEntity*e){entity_delete_children(w->entities,e);entity_delete(w->entities,e);}
static int delay(struct EnemyActor*q,unsigned next){q->result.clock--;if(!q->result.clock)schedule(q,next);return 1;}
void results_reset(struct ResultWorld*r){*r=(struct ResultWorld){.ready=1};for(unsigned i=0;i<10;i++)r->table[i]=result_defaults[i];}
struct NativeEntity*result_allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert order,unsigned state){struct PlayerTransform t={.x=-256,.y=513*256,.priority=65535,.display_group=255};struct NativeEntity*e=enemy_allocate(w,p,order,&t,tick);if(e)schedule(&e->payload.enemy,state);return e;}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){return result_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,state);}
static int typewriter(struct WeaponWorld*w,struct NativeEntity*e,unsigned string,uint16_t cell){struct NativeEntity*n=child(w,e,R_TYPEWRITER);if(!n)return 0;n->payload.enemy.result.string=string;n->payload.enemy.result.cell=cell;n->payload.enemy.result.period=n->payload.enemy.result.clock=1;n->payload.enemy.actor.palette=4;return 1;}
static int wait_children(struct WeaponWorld*w,struct NativeEntity*e,unsigned next){if(!result_has_children(w,e))schedule(&e->payload.enemy,next);return !w->fault;}
static int palette(struct WeaponWorld*w){if(!w->video||!w->video->set_palette)return enemy_fault(w);for(unsigned i=0;i<8;i++){unsigned slot=result_palettes[i][0],id=result_palettes[i][1];if(!w->video->set_palette(w->video->palette_context,(uint8_t)slot,scene_palette_high(id),(slot>>8)|1))return enemy_fault(w);}return 1;}
static int audio(struct WeaponWorld*w,uint8_t value){return w->audio_control&&w->audio_control(w->scene,10,value)?1:enemy_fault(w);}
static int orbit_children(struct WeaponWorld*w,struct NativeEntity*e){static const uint8_t drawing[]={0,1,0,1},facing[]={0,1,1,0};for(unsigned i=0;i<4;i++){struct NativeEntity*n=child(w,e,R_ORBIT_CHILD);if(!n)return 0;n->payload.enemy.result.angle=(uint16_t)(i*0x4000);n->payload.enemy.result.string=drawing[i];n->payload.enemy.actor.transform.facing=facing[i];}e->payload.enemy.result.angle=0x4000;return 1;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct ResultWorld*r=w->results;
 switch(a->state){
 case R_ENDING:if(!result_allocate(w,w->projectile_group,ENTITY_FIRST_CHILD,R_ORBIT)||!palette(w)||!scene_fade_spawn(w,entity_ref(e),1,1,0)||!interaction_sound(w,0x2b))return 0;q->result.clock=30;break;
 case R_ENDING_TITLE:if(!child(w,e,R_TITLE_MISSION))return 0;break;
 case R_ENDING_WAIT30:q->result.clock=30;break;
 case R_ENDING_WIPE:if(!child(w,e,R_WIPE))return 0;break;
 case R_ENDING_WAIT15:q->result.clock=15;break;
 case R_ENDING_P1:{struct NativeEntity*n=child(w,e,R_PANEL);if(!n)return 0;n->payload.enemy.result.slot=0;q->result.clock=4;break;}
 case R_ENDING_P2:{struct NativeEntity*n=child(w,e,R_PANEL);if(!n)return 0;n->payload.enemy.result.slot=1;break;}
 /*59A1A writesA0 toD1, but2308 readsD0. The sole598FC caller15B6
  * waits in15F4, refreshing its45byte to2. Scheduler6B2 decrements
  * this to1 immediately before the first child. With all children
  * drained,59A1A receivesD0.low=1, so source2308 emits0A,21. */
 case R_ENDING_FADE_A:if(!audio(w,0x21))return 0;q->result.clock=200;break;
 case R_ENDING_FADE_B:if(!scene_fade_spawn(w,entity_ref(e),1,0,0)||!audio(w,0x60))return 0;q->result.clock=60;break;
 case R_ENDING_DONE:if(result_parent_signal(w,e,1,255,0))erase(w,e);return 0;
 case R_PANEL:if(!r->summary_valid)return enemy_fault(w);if(q->result.slot>1)return enemy_fault(w);if(r->active[q->result.slot]!=1){erase(w,e);return 0;}q->result.cell=q->result.slot?0x2a0:0x40;q->result.continues=r->continues[q->result.slot];q->result.prisoners=r->prisoners[q->result.slot];if(!result_string(w,q->result.cell+10,q->result.slot?RSTR_P2:RSTR_P1,3,0))return 0;q->result.clock=15;break;
 case R_CONTINUE_LABEL:if(!result_string(w,q->result.cell+13,RSTR_CONTINUE,3,0))return 0;q->result.clock=8;break;
 case R_CONTINUE_ROLL:case R_PRISONER_ROLL:q->result.clock=(q->result.clock&0xff00)|30;break;
 case R_CONTINUE_WAIT:case R_PRISONER_WAIT:q->result.clock=15;break;
 case R_PRISONER_LABEL:if(!result_stream(w,q->result.cell+16,&result_prisoner_stream,0x2300))return 0;q->result.clock=8;break;
 case R_SCORE:if(!w->items||!result_string(w,q->result.cell+20,RSTR_SCORE,3,0)||!result_digits(w,q->result.cell+0x114,w->items->scores[q->result.slot],8))return 0;q->result.clock=30;break;
 case R_HISCORE:q->result.ack=255;if(!results_highscore_enter(w,e,q->result.slot,q->result.cell+0x78))return 0;break;
 case R_TITLE_MISSION:if(!typewriter(w,e,RSTR_MISSION,0x8f))return 0;break;
 case R_TITLE_ALL:if(!typewriter(w,e,RSTR_ALL,0x26f))return 0;break;
 case R_TITLE_OVER:if(!typewriter(w,e,RSTR_OVER,0x34f))return 0;break;
 case R_TITLE_DONE:case R_PEACE_DONE:erase(w,e);return 0;
 case R_WIPE:q->result.clock=15;break;
 case R_PEACE_WAIT:q->result.clock=45;break;
 case R_PEACE_TEXT:if(!typewriter(w,e,RSTR_PEACE,0x11a))return 0;break;
 case R_TYPEWRITER:q->result.index=0;break;
 case R_ORBIT:if(!orbit_children(w,e))return 0;break;
 case R_ORBIT_CHILD:a->palette=0x15f;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int rolling(struct WeaponWorld*w,struct EnemyActor*q,int prisoners){if(!(q->result.clock&3)&&!interaction_sound(w,0x10d8))return 0;q->result.clock=(q->result.clock&0xff00)|(uint8_t)(q->result.clock-1);uint8_t n;if(!(uint8_t)q->result.clock){n=prisoners?q->result.prisoners:q->result.continues;schedule(q,prisoners?R_PRISONER_WAIT:R_CONTINUE_WAIT);}else{if(!w->rng)return enemy_fault(w);n=(uint8_t)source_random_word(w->rng);}return result_digits(w,q->result.cell+(prisoners?0x1b0:0x1ad),n,3);}
static int wipe(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;static const uint16_t col[]={4,19,26};static const uint8_t text[]={RSTR_MISSION,RSTR_ALL,RSTR_OVER},blank[]={RSTR_BLANK_MISSION,RSTR_BLANK_ALL,RSTR_BLANK_OVER};for(unsigned i=0;i<3;i++)if(!result_string(w,col[i]*32+q->result.clock,blank[i],4,1))return 0;q->result.clock--;for(unsigned i=0;i<3;i++)if(!result_string(w,col[i]*32+q->result.clock,text[i],4,1))return 0;if(q->result.clock==5)erase(w,e);return 1;}
static int orbit(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY)return enemy_fault(w);uint16_t angle=(uint16_t)(p->payload.enemy.result.angle+q->result.angle);a->animation.drawing=65535;if(angle&0x8000)return 1;int16_t x,y;weapon_vector(angle>>8,255,&x,&y);q->scale_x=(uint8_t)y;weapon_vector(angle>>8,136,&x,&y);a->transform.x=(int32_t)(int16_t)(uint16_t)(w->camera_x+x+160)*256;a->transform.y=(int32_t)(int16_t)(uint16_t)(w->camera_y+128)*256;if(q->result.string>1)return enemy_fault(w);animation_drawing(&a->animation,&result_program,result_orbit_drawings[q->result.string]);return animation_output_direct(&a->animation,AN_OUTPUT_DIRECT)||enemy_fault(w);}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;if(!w->results||!w->results->ready){enemy_fault(w);return;}struct EnemyActor*q=&e->payload.enemy;if(q->entry){q->entry=0;if(!enter(w,e))return;}if(e->deleting||!e->active)return;
 switch(q->actor.state){
 case R_ENDING:delay(q,R_ENDING_TITLE);break;
 case R_ENDING_TITLE:wait_children(w,e,R_ENDING_WAIT30);break;
 case R_ENDING_WAIT30:delay(q,R_ENDING_WIPE);break;
 case R_ENDING_WIPE:wait_children(w,e,R_ENDING_WAIT15);break;
 case R_ENDING_WAIT15:delay(q,R_ENDING_P1);break;
 case R_ENDING_P1:delay(q,R_ENDING_P2);break;
 case R_ENDING_P2:wait_children(w,e,R_ENDING_FADE_A);break;
 case R_ENDING_FADE_A:delay(q,R_ENDING_FADE_B);break;
 case R_ENDING_FADE_B:delay(q,R_ENDING_DONE);break;
 case R_PANEL:delay(q,R_CONTINUE_LABEL);break;
 case R_CONTINUE_LABEL:delay(q,R_CONTINUE_ROLL);break;
 case R_CONTINUE_ROLL:rolling(w,q,0);break;
 case R_CONTINUE_WAIT:delay(q,R_PRISONER_LABEL);break;
 case R_PRISONER_LABEL:delay(q,R_PRISONER_ROLL);break;
 case R_PRISONER_ROLL:rolling(w,q,1);break;
 case R_PRISONER_WAIT:delay(q,R_SCORE);break;
 case R_SCORE:delay(q,R_HISCORE);break;
 case R_HISCORE:if(!q->result.ack)erase(w,e);break;
 case R_TITLE_MISSION:wait_children(w,e,R_TITLE_ALL);break;
 case R_TITLE_ALL:wait_children(w,e,R_TITLE_OVER);break;
 case R_TITLE_OVER:wait_children(w,e,R_TITLE_DONE);break;
 case R_WIPE:wipe(w,e);break;
 case R_PEACE_WAIT:delay(q,R_PEACE_TEXT);break;
 case R_PEACE_TEXT:wait_children(w,e,R_PEACE_DONE);break;
 case R_TYPEWRITER:{if((int16_t)q->result.clock>0){q->result.clock--;break;}q->result.clock=q->result.period;unsigned string=q->result.string;if(string>=RSTR_COUNT||q->result.index>=result_strings[string].count){enemy_fault(w);break;}uint8_t c=result_strings[string].bytes[q->result.index];if(c==255||q->result.cell>1279){erase(w,e);break;}if(result_character(w,q->result.cell,c,(uint8_t)q->actor.palette,1)){q->result.index++;q->result.cell+=64;}break;}
 case R_ORBIT:q->result.angle-=0x100;break;
 case R_ORBIT_CHILD:orbit(w,e);break;
 default:enemy_fault(w);break;
 }
}
int results_spawn(struct WeaponWorld*w,struct EntityRef p,enum ResultEntry id){static const uint8_t states[]={R_ENDING,R_PANEL,R_PANEL,R_TITLE_MISSION,R_WIPE,R_PEACE_WAIT};if((unsigned)id>=sizeof(states))return enemy_fault(w);struct NativeEntity*e=result_allocate(w,p,ENTITY_FIRST_CHILD,states[id]);if(!e)return 0;if(id==RESULT_PANEL_P1||id==RESULT_PANEL_P2)e->payload.enemy.result.slot=id==RESULT_PANEL_P2;return 1;}
