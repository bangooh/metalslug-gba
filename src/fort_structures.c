#include "fort_internal.h"
static const uint32_t root_scripts[5]={0xedc8c,0xedcd2,0xedd2a,0xedda6,0xede22};
static const uint32_t hatch_scripts[7]={0xede9e,0xededa,0xedefa,0xedf1a,0xedf52,0xedf72,0xedfaa};
static const uint32_t hatch_final[5]={0xedcb2,0xedd0a,0xedd86,0xede02,0xede7e};
static const uint16_t wall_patches[13]={FTV_P2ED990,FTV_P2ED9A4,FTV_P2ED9B8,FTV_P2ED9CC,FTV_P2EDA80,FTV_P2EDA94,FTV_P2ED9E0,FTV_P2ED9F4,FTV_P2EDA08,FTV_P2EDA1C,FTV_P2EDA30,FTV_P2EDA44,FTV_P2EDA58};
static const uint8_t wall_patterns[13]={FTP_P2ECFDE,FTP_P2ECFF0,FTP_P2ECFDE,FTP_P2ECFDE,FTP_P2ED002,FTP_P2ED002,FTP_P2ED014,FTP_P2ED026,FTP_P2ED038,FTP_P2ED038,FTP_P2ED038,FTP_P2ED038,FTP_P2ED038};
static const uint32_t wall_scripts[4]={0xffffffffu,0xedfe2,0xee002,0xee022};
static int spawn_parts(struct WeaponWorld*w,struct NativeEntity*e){
 unsigned v=e->payload.enemy.fort.variant;
 static const uint8_t counts[5]={5,5,5,6,5},base[5]={FTV_P2ED738,FTV_P2ED79C,FTV_P2ED800,FTV_P2ED88C,FTV_P2ED904};
 static const uint8_t types[5][4]={{3,0,3,0},{3,0,3,0},{4,4,4,0},{2,3,3,3},{4,4,4,0}};
 static const uint32_t scripts[5][4]={{0xee066,0xee07c,0xee092,0},{0xee0a8,0xee0be,0xee0d4,0},{0xee0ea,0xee100,0xee116,0},{0xee12c,0xee130,0xee146,0xee15c},{0xee172,0xee188,0xee19e,0}};
 unsigned pills=v==3?4:3;
 for(unsigned i=0;i<counts[v];i++){
  int pill=i<pills;struct NativeEntity*n=ft_child(w,e,pill?FT_PILL:FT_CRATE,ENTITY_FIRST_CHILD);if(!n)return 0;struct EnemyActor*q=&n->payload.enemy;
  q->fort.variant=pill?types[v][i]:(uint8_t)v;q->fort.timer=(int16_t)i;q->fort.patch=(uint16_t)(base[v]+i);
  int x=pill?(v==3?24+48*(int)i:40+64*(int)i):(i==pills?(v?40:48):(v==4?152:v?160:168));
  int y=pill?(v&1?64:80):(v==1||v==4?0:8);
  transform_integer_offset(&q->actor.transform,(int16_t)x,(int16_t)y);
  if(pill){q->actor.transform.priority=v&1?0x70:0x60;q->fort.watch_script=scripts[v][i];}
 }
 struct NativeEntity*n=ft_child(w,e,FT_HATCH,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.fort.variant=(uint8_t)v;
 transform_integer_offset(&n->payload.enemy.actor.transform,80,v&1?0:16);return 1;
}
static int pill_finish(struct WeaponWorld*w,struct NativeEntity*e,unsigned sound){
 struct EnemyActor*q=&e->payload.enemy;if(!ft_score(w,&q->actor,0x100)||!interaction_sound(w,(uint16_t)sound))return 0;
 struct NativeEntity*n=ft_child(w,e,FT_BLAST,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.fort.variant=0;transform_integer_offset(&n->payload.enemy.actor.transform,0,16);
 if(!ft_patch(w,q->fort.patch))return 0;n=ft_pattern(w,e,FTP_P2ED05C);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,16);
 if(q->fort.watch_script!=0xffffffffu&&!ft_watch(w,e,q->fort.watch_script))return 0;
 return 1;
}
static int propagate(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=ft_parent(w,e);if(!p)return 0;
 p->actor.hit.hp=ft_word(p->actor.hit.hp-ft_word(q->fort.previous_hp-q->actor.hit.hp));q->fort.previous_hp=q->actor.hit.hp;return 1;
}
int ft_structures_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;unsigned v=q->fort.variant,s=a->state;
 switch(s){
 case FT_ROOT:if(v>=5)return enemy_fault(w);a->phase=(uint8_t)v;if(!spawn_parts(w,e))return 0;q->fort.watch_script=root_scripts[v];q->fort.margin=192;a->hit.hp=320;q->dead=0;ft_defense(q,FTB_ROOT);break;
 case FT_ROOT_DESTROY:{
  static const uint8_t bases[5]={FTV_P2EDAA8,FTV_P2EDAF8,FTV_P2EDB34,FTV_P2EDB84,FTV_P2EDBC0};
  if(interaction_move(w,a,WMOVE_SYNC)<0||!interaction_sound(w,0x1028))return 0;
  if(!ft_pattern(w,e,FTP_P2ECF68)||!ft_pattern(w,e,FTP_P2ECF7A)||!ft_pattern(w,e,FTP_P2ED04A))return 0;
  if(a->phase>=5)return enemy_fault(w);for(unsigned i=0;i<(a->phase&1?3u:4u);i++)if(!ft_patch(w,bases[a->phase]+i))return 0;
  if(!ft_stamp(w,e,FTS_ROOT0+v))return 0;ft_attack(q,FTB_ROOT_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a)||!ft_watch(w,e,q->fort.watch_script))return 0;
  ft_schedule(e,FT_ROOT_END);return 0;
 }
 case FT_ROOT_END:ft_attack(q,65535);hit_begin_attack(&a->hit);entity_delete(w->entities,e);return 0;
 case FT_PILL:
  if(v>=6)return enemy_fault(w);a->palette=v%3==1?0xe4:0xe3;q->dead=(uint8_t)(v%3*0x30+(v>=3));a->transform.facing=0;a->hit.hp=q->fort.previous_hp=40;
  a->transform.priority=0x8000;ft_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;ft_start_entry(q,fort_pill[q->dead>>4]);a->animation.freeze=(uint8_t)q->fort.timer;break;
 case FT_PILL1:case FT_PILL2:ft_start_entry(q,fort_pill[(q->dead>>4)+(s==FT_PILL1?1:2)]);break;
 case FT_PILL_CRITICAL:break;
 case FT_PILL_COLLAPSE:if(!ft_child(w,e,FT_COLLAPSE,ENTITY_FIRST_CHILD))return 0;a->state=FT_PILL_COLLAPSED;break;
 case FT_COLLAPSE:a->palette=0x6c;a->transform.facing=0;a->transform.priority=0;transform_integer_offset(&a->transform,0,8);ft_start(q,FTA_COLLAPSE);break;
 case FT_CRATE:if(v>=5)return enemy_fault(w);a->palette=(uint16_t)(0xe7+v);q->dead=(uint8_t)v;a->transform.facing=0;a->hit.hp=20;a->transform.priority=0;break;
 case FT_CRATE_BURST:if(!ft_score(w,a,0x100)||!interaction_sound(w,0x1039))return 0;ft_start_entry(q,fort_crate[q->dead]);break;
 case FT_HATCH:
  if(v>=7)return enemy_fault(w);q->fort.margin=v==3||v==4?32:48;a->phase=(uint8_t)v;
  if(v<5){q->dead=1;q->fort.final_script=hatch_final[v];}q->fort.watch_script=hatch_scripts[v];q->fort.timer=90;break;
 case FT_HATCH_OPEN:case FT_HATCH_FINAL_OPEN:ft_start_entry(q,fort_hatch_open[a->phase]);if(s==FT_HATCH_FINAL_OPEN)a->state=FT_HATCH_FINAL;break;
 case FT_HATCH_WATCH:q->fort.timer=40;if(!ft_watch(w,e,q->fort.watch_script))return 0;break;
 case FT_HATCH_HOLD:q->fort.timer=80;break;
 case FT_HATCH_CLOSE:ft_start_entry(q,fort_hatch_close[a->phase]);break;
 case FT_HATCH_FINAL:break;
 case FT_PROP:q->fort.margin=32;ft_defense(q,FTB_PROP);break;
 case FT_SIGN:q->fort.margin=32;ft_start(q,FTA_SIGN);break;
 case FT_WALL:{
  static const int16_t offsets[13][2]={{-16,64},{16,160},{80,160},{128,160},{16,128},{96,128},{80,80},{0,0},{32,0},{64,0},{96,0},{128,0},{160,0}};
  for(unsigned i=0;i<13;i++){n=ft_child(w,e,FT_WALL_PART,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.fort.variant=(uint8_t)i;transform_integer_offset(&n->payload.enemy.actor.transform,offsets[i][0],offsets[i][1]);}
  entity_delete(w->entities,e);return 0;
 }
 case FT_WALL_PART:if(v>=13)return enemy_fault(w);q->fort.margin=48;q->fort.timer=(int16_t)v;q->fort.defense=fort_wall_defs[v];q->fort.patch=wall_patches[v];q->fort.pattern=wall_patterns[v];q->fort.watch_script=v<4?wall_scripts[v]:0xffffffffu;a->hit.hp=20;ft_defense(q,q->fort.defense);break;
 case FT_QUAD:for(unsigned i=0;i<4;i++){n=ft_child(w,e,FT_QUAD_PART,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.fort.variant=(uint8_t)i;}entity_delete(w->entities,e);return 0;
 case FT_QUAD_PART:if(v>=4)return enemy_fault(w);ft_start(q,(enum FortAnimation)(FTA_QUAD0+v));q->fort.pattern=(uint16_t)(FTP_P2ED19A+v);a->palette=0xe4;q->fort.margin=64;a->hit.hp=10;a->transform.priority=0x8000;ft_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
int ft_structures_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p=0;int done=0;
 if(s==FT_ROOT){
  if(ft_sx(w,a)<=-64)ft_defense(q,65535);if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
  if(a->hit.flags&8){if(!ft_hurt(w,a,0))return 0;a->hit.flags&=247;}
  if(ft_sx(w,a)>=192)a->hit.hp=320;if(a->hit.hp<=0){ft_defense(q,65535);q->dead=255;ft_schedule(e,FT_ROOT_DESTROY);}
  if(ft_cull(w,e)){q->dead=255;entity_delete(w->entities,e);}return !w->fault;
 }
 if(s>=FT_PILL&&s<=FT_CRATE_BURST){
  p=ft_parent(w,e);if(!p)return 0;
  if(s==FT_COLLAPSE){struct NativeEntity*pe=entity_get(w->entities,e->parent);struct EnemyActor*g=ft_parent(w,pe);if(!g)return 0;if(g->dead==255){entity_delete(w->entities,e);return 1;}}
  else if(p->dead==255){if(s<FT_COLLAPSE&&!ft_stamp(w,e,FTS_PILL))return 0;entity_delete(w->entities,e);return 1;}
 }
 if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
 if((s>=FT_PILL&&s<=FT_COLLAPSE)||s==FT_CRATE_BURST||s==FT_HATCH_OPEN||s==FT_HATCH_CLOSE||s==FT_HATCH_FINAL||s==FT_SIGN||s==FT_QUAD_PART)done=ft_animate(w,e);
 if(s>=FT_PILL&&s<=FT_PILL_CRITICAL){
  if(a->hit.flags&8){if(!ft_hurt(w,a,1)||!propagate(w,e))return 0;a->hit.flags&=247;}
  if(s==FT_PILL&&a->hit.hp<=30)ft_schedule(e,FT_PILL1);
  if(s==FT_PILL1&&a->hit.hp<=26)ft_schedule(e,FT_PILL2);
  if(s==FT_PILL2&&a->hit.hp<=13)ft_schedule(e,FT_PILL_CRITICAL);
  if(s==FT_PILL_CRITICAL&&(a->hit.flags&1)){
   a->hit.flags&=254;ft_defense(q,65535);if(q->dead&1)ft_schedule(e,FT_PILL_COLLAPSE);
   else{if(!pill_finish(w,e,0x102f)||!ft_stamp(w,e,FTS_PILL))return 0;entity_delete(w->entities,e);}
  }return !w->fault;
 }
 if(s==FT_PILL_COLLAPSED){if((q->dead&15)==15){if(!pill_finish(w,e,0x102c)||!ft_stamp(w,e,FTS_PILL))return 0;entity_delete(w->entities,e);}return !w->fault;}
 if(s==FT_COLLAPSE){if(done){p->dead|=15;entity_delete(w->entities,e);}return !w->fault;}
 if(s==FT_CRATE){
  if(a->hit.flags&8){if(!ft_hurt(w,a,0)||!propagate(w,e))return 0;a->hit.flags&=247;}
  if(a->hit.flags&1){ft_defense(q,65535);ft_schedule(e,FT_CRATE_BURST);}
  q->fort.timer=ft_word(q->fort.timer+1);ft_defense(q,q->fort.timer&3?65535:fort_crate_defs[q->dead]);return !w->fault;
 }
 if(s==FT_CRATE_BURST){
  if(done){if(!interaction_sound(w,0x102f))return 0;struct NativeEntity*n=ft_child(w,e,FT_BLAST,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.fort.variant=1;
   if(!ft_patch(w,q->fort.patch)||!ft_pattern(w,e,FTP_P2ED05C))return 0;entity_delete(w->entities,e);}return !w->fault;
 }
 if(s>=FT_HATCH&&s<=FT_HATCH_FINAL){
  if(s==FT_HATCH||s==FT_HATCH_HOLD){if(!--q->fort.timer)ft_schedule(e,s==FT_HATCH?FT_HATCH_OPEN:FT_HATCH_CLOSE);}
  else if(s==FT_HATCH_OPEN&&done)ft_schedule(e,FT_HATCH_WATCH);
  else if(s==FT_HATCH_WATCH&&!ft_children(w,e))ft_schedule(e,FT_HATCH_HOLD);
  else if(s==FT_HATCH_CLOSE&&done)ft_schedule(e,FT_HATCH);
  else if(s==FT_HATCH_FINAL&&done){if(!ft_watch(w,e,q->fort.final_script))return 0;entity_delete(w->entities,e);return 1;}
  if((s==FT_HATCH||s==FT_HATCH_WATCH||s==FT_HATCH_HOLD)&&q->dead==1){p=ft_parent(w,e);if(!p)return 0;if(p->dead==255)ft_schedule(e,s==FT_HATCH?FT_HATCH_FINAL_OPEN:FT_HATCH_FINAL);}
 }else{
  if(a->hit.flags&8){if(s!=FT_SIGN&&!ft_hurt(w,a,0))return 0;a->hit.flags&=247;if(s==FT_SIGN){if(!interaction_sound(w,0x10a9))return 0;ft_start(q,FTA_SIGN_HIT);}}
  if(s==FT_SIGN)a->hit.hp=32767;
  if((s==FT_PROP||s==FT_WALL_PART||s==FT_QUAD_PART)&&(a->hit.flags&1)){
   if(s==FT_PROP){if(q->fort.variant==0&&!interaction_sound(w,0x10a6))return 0;if(!ft_pattern(w,e,FTP_P2ED0AE))return 0;
    static const uint8_t patches[3][2]={{FTV_P2ED878,FTV_P2ED864},{FTV_P2ED968,FTV_P2ED97C},{FTV_P2EDA6C,255}};
    for(unsigned i=0;i<2;i++)if(patches[q->fort.variant][i]!=255&&!ft_patch(w,patches[q->fort.variant][i]))return 0;
   }else{ft_defense(q,65535);if(!interaction_sound(w,s==FT_WALL_PART?0x1027:0x102e))return 0;
    if(s==FT_WALL_PART&&!ft_patch(w,q->fort.patch))return 0;if(!ft_pattern(w,e,q->fort.pattern))return 0;
    if(s==FT_WALL_PART&&q->fort.watch_script!=0xffffffffu&&!ft_watch(w,e,q->fort.watch_script))return 0;
   }entity_delete(w->entities,e);return !w->fault;
  }
 }
 if(ft_cull(w,e)){entity_delete(w->entities,e);return !w->fault;}
 if(s==FT_WALL_PART){q->fort.timer=ft_word(q->fort.timer+1);ft_defense(q,q->fort.timer&3?65535:q->fort.defense);}
 return !w->fault;
}
