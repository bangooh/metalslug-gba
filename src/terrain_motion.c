#include "terrain_motion.h"
static int16_t word(int value){return (int16_t)(uint16_t)value;}
static int failure(struct TerrainWorld*w,unsigned code){if(!w->fault)w->fault=(uint8_t)code;return 0;}
static unsigned next(unsigned index){return (index+1)&15;}
static int16_t clamp_velocity(int value){int16_t v=word(value);return v>0x800?0x800:v<-0x800?-0x800:v;}
static int screen_x(const struct TerrainWorld*w,const struct TerrainMotion*m){
 return word((m->transform->x>>8)-w->camera_x+((m->hit->flags&64)?0:w->delta_x));
}
static int screen_y_up(const struct TerrainWorld*w,const struct TerrainMotion*m){
 return word(transform_y_up(m->transform)+w->camera_y-((m->hit->flags&64)?0:w->delta_y));
}
static int16_t horizontal_velocity(struct TerrainWorld*w,struct TerrainMotion*m){
 int16_t vx=m->vx;
 if(((m->movement_flags&1)&&vx>0)||((m->movement_flags&2)&&vx<0))vx=0;
 /* $028074 applies to the two player slots and a ridden Slug. */
 if(m->player_boundary){
  int x=screen_x(w,m);
  if(w->scene==9||(x>=30&&x<=279))m->hit->flags|=128;
  else {if((x<30&&vx<0)||(x>279&&vx>0))vx=0;m->hit->flags&=(uint8_t)~128u;}
 }
 return vx;
}
/* $0997B8 resets only the queue positions; the ID allocator survives. */
void terrain_platform_reset(struct SourcePlatforms*p){p->first=p->end=p->write=0;}
void terrain_platform_frame(struct SourcePlatforms*p){p->first=p->end;p->end=p->write;}
static const struct SourcePlatform*platform_id(struct SourcePlatforms*p,unsigned id){
 for(unsigned i=p->first;i!=p->end;i=next(i))if(p->records[i].id==id)return &p->records[i];
 return 0;
}
int terrain_platform_publish(struct TerrainWorld*w,struct TerrainMotion*m,int16_t x,int16_t y_up,int16_t width,int force_id14){
 struct SourcePlatforms*p=&w->platforms;unsigned id=m->platform_ids>>4;
 const struct SourcePlatform*old=0;
 /* $0997E2/$099812. Only an established ID contributes displacement. */
 if(force_id14&&id!=14)id=14;
 else if(id==15){id=(uint8_t)(p->last_id+1);if(id>=14)id=0;p->last_id=(uint8_t)id;}
 else old=platform_id(p,id);
 m->platform_ids=(uint8_t)((id<<4)|(m->platform_ids&15));
 p->records[p->write]=(struct SourcePlatform){.x=x,.y_up=y_up,.width=width,.id=(uint8_t)id,
  .dx=old?word(x-old->x):0,.dy_up=old?word(y_up-old->y_up):0};
 p->write=(uint8_t)next(p->write);return 1;
}
int terrain_platform_at(struct TerrainWorld*w,struct TerrainMotion*m,int16_t x,int16_t y_up){
 /* $0998CA: reverse order, exact surface Y, unsigned span comparison. */
 struct SourcePlatforms*p=&w->platforms;
 for(unsigned i=p->end;i!=p->first;){
  i=(i+15)&15;const struct SourcePlatform*v=&p->records[i];
  if(y_up==v->y_up&&(uint16_t)word(x-v->x)<(uint16_t)v->width&&(m->platform_ids>>4)!=v->id){
   m->platform_ids=(uint8_t)((m->platform_ids&0xf0)|v->id);return 1;
  }
 }
 return 0;
}
static int moving_platform(struct TerrainWorld*w,struct TerrainMotion*m,int16_t x,int16_t y_up,struct TerrainSample*s){
 struct SourcePlatforms*p=&w->platforms;
 for(unsigned i=p->first;i!=p->end;i=next(i)){
  const struct SourcePlatform*v=&p->records[i];
  if(y_up>v->y_up||y_up<=word(v->y_up-20)||(uint16_t)word(x-v->x)>=(uint16_t)v->width||(m->platform_ids>>4)==v->id)continue;
  s->shape=17;s->half=&source_terrain_shapes[17].halves[0];s->half_index=0;
  s->column=0;s->row=(uint8_t)word(v->y_up-y_up);s->platform=v->id;return 1;
 }
 if((m->platform_ids&15)==14)m->platform_ids|=15;
 s->platform=15;return 0;
}
int terrain_lookup(struct TerrainWorld*w,struct TerrainMotion*m,int16_t x,int16_t y_up,struct TerrainSample*s){
 if(w->fault)return 0;
 uint8_t raw=0;
 if(word(y_up+w->camera_y)<=511){
  if(!w->cell||!w->cell(w->map,x,word(512-y_up),&raw))return failure(w,1);
 }
 s->column=(uint8_t)((x+w->cell_offset_x)&7);s->row=(uint8_t)((512-y_up+w->cell_offset_y)&7);s->platform=15;s->half_index=0;
 for(unsigned budget=0;budget<64;budget++){
  if((m->collision_flags&4)&&(raw&0xc0)==0xc0)raw=0;
  s->raw=raw;s->shape=raw&63;s->half=&source_terrain_shapes[s->shape].halves[0];
  if(!(s->half->filter&m->hit->type_flags))return 1;
  s->shape=0;
  if(s->half->replacement==255)return 1;
  raw=s->half->replacement;
 }
 return failure(w,2); /* Invalid redirect cycle, never replace by empty ground. */
}
static int sample(struct TerrainWorld*w,struct TerrainMotion*m,int16_t x,int16_t y_up,struct TerrainSample*s){
 if(!terrain_lookup(w,m,x,y_up,s))return 0;moving_platform(w,m,x,y_up,s);return 1;
}
static int occupied(const struct TerrainSample*s){
 return s->row<20&&!!(s->half->rows[s->row]&(128u>>s->column));
}
static int both_halves(struct TerrainSample*s){
 if(occupied(s))return 1;
 s->half++;s->half_index=1;
 return occupied(s);
}
static int platform_carry(struct TerrainWorld*w,struct TerrainMotion*m){
 /* $044182 in world coordinates. Original screen deltas are reconstructed
  * only for the left-edge/mission-2 lock decisions; ordinary camera motion
  * does not change the world position of an actor. */
 if(m->hit->flags&64)return 1;m->hit->flags|=64;
 unsigned riding=m->platform_ids&15,own=m->platform_ids>>4;
 int dx=0,dy=0;
 if(riding!=15){
  if(riding==own)return failure(w,3);
  const struct SourcePlatform*v=platform_id(&w->platforms,riding);
  if(!v)m->platform_ids|=15;
  else {
   int screen_dx=word(v->dx-w->delta_x);int apply=1;
   if(!(m->hit->flags&128)&&(m->hit->type_flags&(64|2|1))){
    int x=word((m->transform->x>>8)-w->camera_x+w->delta_x);
    apply=(x>30)!=(screen_dx>=0);
   }
   dx=apply?v->dx:w->delta_x;dy=v->dy_up;
  }
 }
 if(w->scene==1&&w->scene1_horizontal_lock)dx=w->delta_x;
 transform_integer_offset(m->transform,dx,dy);return 1;
}
static int reject_surface(const struct TerrainMotion*m,const struct SourceTerrainProfile*p){
 /* $02822C signed WORD arithmetic, including steep-slope multiply wraps. */
 int16_t slope=p->slope,vy=m->vy;
 if(!slope)return vy>0;
 if(slope==-1)return 1;
 if(slope>0){if(slope==0x1000)vy=word(vy*4);return word(vy-m->vx)>0;}
 if(slope==-0x1000)vy=word(vy*4);return word(vy+m->vx)>0;
}
static int snap_surface(struct TerrainWorld*w,struct TerrainMotion*m,int16_t x,int16_t*y_up,
 struct TerrainSample*s,uint8_t*fraction){
 /* $0275B8..$02773A. The final fraction comes from the INITIAL profile,
  * while an overhang re-probe supplies the slope/height used for landing. */
 const struct SourceTerrainProfile*initial=&s->half->profile;unsigned shape=s->shape;int raise=0;
 switch(shape){
  case 12:case 13:case 14:case 15:case 28:case 29:case 30:case 31:case 52:
  case 32:case 33:case 48:case 49:raise=8;break;
  case 44:case 45:case 46:case 47:case 60:case 61:case 62:case 63:raise=16;break;
 }
 if(raise){*y_up=word(*y_up+raise);if(!sample(w,m,x,*y_up,s))return -1;}
 const struct SourceTerrainProfile*profile=&s->half->profile;
 m->slope=profile->slope;m->ground_tag=profile->tag;
 if(m->slope==128||m->slope==-128){failure(w,4);return -1;}
 if((m->hit->flags&128)&&m->slope==-1)return 0;
 *y_up=word(*y_up+s->row-profile->heights[s->column]+1);
 *fraction=initial->fractions[s->column];m->hit->events|=64;
 m->platform_ids=(uint8_t)((m->platform_ids&0xf0)|(s->platform&15));return 1;
}
static int same_direction(const struct TerrainMotion*m){
 return !m->previous_direction||((m->previous_direction<0)==(m->vx<0));
}
static int branch_allowed(const struct TerrainMotion*m,unsigned bit,int dx){
 return (m->hit->flags&bit)&&dx&&((m->collision_flags&bit)||same_direction(m));
}
static void priority_from_y(struct TerrainWorld*w,struct TerrainMotion*m){
 /* $028108: depth ordering only for priorities in the original dynamic band. */
 unsigned value=(uint16_t)(m->transform->priority-0x4000);
 if(value<0x7fff){
  unsigned row=(unsigned)word(transform_y_up(m->transform)+w->camera_y)&255;
  m->transform->priority=(uint16_t)((m->transform->priority&0xe01f)|((255-row)<<5));
 }
}
int terrain_synchronize(struct TerrainWorld*w,struct TerrainMotion*m){
 if(w->fault||!platform_carry(w,m))return 0;priority_from_y(w,m);return 1;
}
static int floor_reprobe(struct TerrainWorld*w,struct TerrainMotion*m,const struct TerrainSample*old,
 const struct SourceTerrainProfile*target,int16_t*x,int16_t*y,struct TerrainSample*out){
 /* $026E0A: move to the appropriate neighbouring cell from the OLD cell
  * boundary. This is why a broad downward raycast is not equivalent. */
 int direction=w->step_x?w->step_x:m->vx;unsigned table=direction<0?1:direction==0?2:0;
 unsigned row=m->slope==0||m->slope==-1?2:m->slope<0?4:0;
 if(target->slope==-1||target->slope<m->slope)row++;
 const int16_t*offset=source_floor_probe_offsets[table][row];
 *x=word((m->transform->x>>8)-old->column+offset[0]);
 *y=word(transform_y_up(m->transform)-1+old->row+offset[1]);
 if(!sample(w,m,*x,*y,out))return 0;
 int raise=0;
 switch(out->shape){
  case 12:case 13:case 14:case 15:case 28:case 29:case 30:case 31:case 52:raise=8;break;
  case 44:case 45:case 46:case 47:case 60:case 61:case 62:case 63:raise=16;break;
  case 32:case 33:case 48:case 49:if(m->hit->flags&16)raise=8;break;
 }
 if(raise){*y=word(*y+raise);if(!sample(w,m,*x,*y,out))return 0;}
 m->slope=out->half->profile.slope;m->ground_tag=out->half->profile.tag;
 if(m->slope==128||m->slope==-128)return failure(w,4);
 *y=word(*y-out->half->profile.heights[out->column]);return 1;
}
static int move_ground(struct TerrainWorld*w,struct TerrainMotion*m,struct WeaponContact*contact,int flat){
 /* $026B56/$026992/$027A92: the source's complete walking kernel, with
  * distinct primary/secondary slopes and directional branch entrances. */
 m->vx=clamp_velocity(m->vx+m->ax);m->vy=clamp_velocity(m->vy+m->ay);
 int16_t vx=horizontal_velocity(w,m);
 if((m->slope==0x400||m->slope==-0x400)&&!(w->scene==1&&w->scene1_horizontal_lock))vx=(int16_t)(((int32_t)vx*0xaa)>>8);
 int32_t up=131072-m->transform->y;
 uint8_t oldfx=(uint8_t)m->transform->x,oldfy=(uint8_t)up;
 int16_t sum=word(oldfx+vx);int dx=sum>>8;uint8_t fx=(uint8_t)sum,fy=oldfy;
 int sign=m->vx<0?-1:m->vx>0?1:0;
 if(w->stop_x&&w->stop_x==sign){dx=0;fx=0;}w->stop_x=w->stop_y=0;
 int dy=w->step_y;
 if(m->slope==-1){if(m->collision_flags&8)dy=0;}
 else if(!m->slope)dy=0;
 else if(m->slope==0x400||m->slope==-0x400)dy=m->slope<0?-dx:dx;
 else if(m->slope==0x1000||m->slope==-0x1000){
  int16_t delta=word(dx*64);if(m->slope<0)delta=word(-delta);
  sum=word(oldfy+delta);fy=(uint8_t)sum;dy=sum>>8;
 }else if(m->slope==128||m->slope==-128){dx=0;if(m->collision_flags&8)dy=0;}
 else {failure(w,4);return -1;}
 if(m->player_boundary&&w->scene==3&&screen_y_up(w,m)>=496&&dy>0){dx=dy=0;fx=oldfx;fy=oldfy;}
 uint8_t stepfy=fy;
 w->step_x=(int16_t)dx;w->step_y=(int16_t)dy;
 if(!platform_carry(w,m))return -1;
 int16_t x=word(m->transform->x>>8),y=word(transform_y_up(m->transform)-1);
 struct TerrainSample old,current;
 if(!terrain_lookup(w,m,x,y,&old))return -1;
 int16_t destx=word(x+dx),desty=word(y+dy);
 if(terrain_platform_at(w,m,destx,desty)){
  current=(struct TerrainSample){.shape=17,.raw=17,.half=&source_terrain_shapes[17].halves[0],.platform=m->platform_ids&15};
  old.column=old.row=0;fy=current.half->profile.fractions[0];
 }else{
  if(old.half->alternate==1&&!m->slope){old.half++;old.half_index=1;}
  if(!terrain_lookup(w,m,destx,desty,&current))return -1;
  if(current.half->alternate==1&&!m->slope){
   unsigned s=current.shape;
   int entrance=dx<0?(s==34||s==50):(s==35||s==51);
   if(!flat&&entrance&&branch_allowed(m,32,dx))m->hit->events|=8;
   else {current.half++;current.half_index=1;}
  }
  fy=current.half->profile.fractions[current.column];
 }
 const struct SourceTerrainProfile*profile=&current.half->profile;unsigned shape=current.shape;
 int even_branch=shape==32||shape==48,odd_branch=shape==33||shape==49;
 if(even_branch||odd_branch){
  int entrance=dx<0?odd_branch:even_branch;
  if(flat?(m->hit->flags&16):(entrance&&branch_allowed(m,16,dx))){if(!flat)m->hit->events|=8;}
  else profile=&source_terrain_shapes[1].halves[0].profile;
 }
 if((flat||same_direction(m))&&(shape==16||shape==39)){
  m->hit->events|=32;priority_from_y(w,m);
  *contact=(struct WeaponContact){.shape=current.shape,.raw=current.raw};return flat;
 }
 int changed=m->ground_tag!=profile->tag||m->slope!=profile->slope;
 /* $0272A8/$027AFC refuse slope transitions instead of probing a new floor.
  * They retain the existing position and fractions when C is set. */
 if(flat&&changed){priority_from_y(w,m);*contact=(struct WeaponContact){.shape=current.shape,.raw=current.raw};return 1;}
 if(changed){
  if(!floor_reprobe(w,m,&old,profile,&destx,&desty,&current))return -1;
  fx=0;fy=current.half->profile.fractions[current.column];
 }else if(!flat)m->hit->events&=(uint8_t)~8u;
 if(flat)fy=stepfy;
 desty=word(desty+1);m->transform->x=destx*256+fx;m->transform->y=131072-(desty*256+fy);
 priority_from_y(w,m);
 *contact=(struct WeaponContact){.shape=current.shape,.raw=current.raw,.solid=(uint8_t)occupied(&current)};
 return changed;
}
int terrain_move(struct TerrainWorld*w,struct TerrainMotion*m,enum WeaponMove policy,struct WeaponContact*contact){
 if(w->fault||(unsigned)policy>WMOVE_FLAT)return -1;
 if(policy==WMOVE_ITEM){
  /* $027EBA returns C=1 in AIR, whereas terrain_foot_contact returns
   * 1 on GROUND. $02741C BCS resumes gravity after a support disappears;
   * the standing branch alone returns landed without integrating. */
  if(m->slope!=-1){
   if(!platform_carry(w,m))return -1;
   int ground=terrain_foot_contact(w,m);if(ground<0)return -1;
   if(ground){*contact=(struct WeaponContact){0};return 1;}
  }
  m->vy=m->vy>0x400?0x400:m->vy<-0x400?-0x400:m->vy;m->ay=-64;
  return terrain_move(w,m,WMOVE_BODY,contact);
 }
 if(policy==WMOVE_SYNC){*contact=(struct WeaponContact){0};return terrain_synchronize(w,m)?0:-1;}
 if(policy==WMOVE_GROUND||policy==WMOVE_FLAT)return move_ground(w,m,contact,policy==WMOVE_FLAT);
 int body=policy==WMOVE_BODY||policy==WMOVE_JUMP;
 if(!body&&!platform_carry(w,m))return -1;
 m->vx=clamp_velocity(m->vx+m->ax);
 int16_t new_y=word(m->vy+m->ay);
 m->vy=policy==WMOVE_JUMP&&new_y>0?new_y:clamp_velocity(new_y);
 int16_t vx=horizontal_velocity(w,m),vy=m->vy;
 if(m->player_boundary&&w->scene==3&&screen_y_up(w,m)>=496){vx=0;if(vy>0)vy=0;}
 int32_t up=131072-m->transform->y;
 int16_t xsum=word(((uint32_t)m->transform->x&255)+vx),ysum=word(((uint32_t)up&255)+vy);
 int dx=xsum>>8,dy=ysum>>8;uint8_t fx=(uint8_t)xsum,fy=(uint8_t)ysum;
 int sign=m->vx<0?-1:m->vx>0?1:0;
 if(w->stop_x&&w->stop_x==sign){dx=0;fx=0;}w->stop_x=w->stop_y=0;
 w->step_x=(int16_t)dx;w->step_y=(int16_t)dy;
 if(body){m->slope=m->ground_tag=-1;if(!platform_carry(w,m))return -1;}
 int16_t x=(int16_t)(m->transform->x>>8),y_up=transform_y_up(m->transform);
 struct TerrainSample sample_data;
 if(body){
  if(!terrain_lookup(w,m,word(x+dx),y_up,&sample_data))return -1;
  switch(sample_data.shape){case 1:case 16:case 38:case 39:case 12:case 13:case 14:case 15:
   m->hit->events|=32;dx=0;fx=0;w->step_x=0;break;}
 }
 x=word(x+dx);y_up=word(y_up+dy-1);
 if(!sample(w,m,x,y_up,&sample_data))return -1;
 int collision=0;
 if(body){
  if(sample_data.shape==40&&m->vy>0)m->vy=0;
  if(both_halves(&sample_data)&&!reject_surface(m,&sample_data.half->profile)){
   collision=snap_surface(w,m,x,&y_up,&sample_data,&fy);
   if(collision<0)return -1;
   if(collision){fx=0;m->vy=m->ay=0;}
  }
 }else if(sample_data.shape&&occupied(&sample_data)&&!sample_data.half->point_pass){
  if(policy==WMOVE_POINT)collision=1;
  else if((dy?dy:m->vy)<0){
   collision=snap_surface(w,m,x,&y_up,&sample_data,&fy);if(collision<0)return -1;if(collision)fx=0;
  }
 }
 /* $02759A adds one to the probed feet point. A successful slope snap
  * already performed that increment and substituted its fractional Y. */
 if(!collision||policy==WMOVE_POINT)y_up=word(y_up+1);
 m->transform->x=x*256+fx;m->transform->y=131072-(y_up*256+fy);
 *contact=(struct WeaponContact){.shape=sample_data.shape,.raw=sample_data.raw,
  .solid=(uint8_t)(occupied(&sample_data)&&!sample_data.half->point_pass)};
 return collision;
}
int terrain_contact_at(struct TerrainWorld*w,struct TerrainMotion*m,int16_t x,int16_t y_up){
 struct TerrainSample s;
 if(!sample(w,m,x,word(y_up-1),&s))return -1;
 if(!both_halves(&s))return 0;m->hit->events|=64;return 1;
}
int terrain_move_ground_origin(struct TerrainWorld*w,struct TerrainMotion*m,enum WeaponMove policy,int16_t*ground_y_up){
 if(w->fault)return -1;
 if(policy!=WMOVE_SYNC&&policy!=WMOVE_GROUND&&policy!=WMOVE_BODY){failure(w,5);return -1;}
 int16_t body_y=transform_y_up(m->transform);uint16_t priority=m->transform->priority;
 transform_integer_offset(m->transform,0,word(*ground_y_up-body_y));
 struct WeaponContact contact;
 int result=policy==WMOVE_SYNC?(platform_carry(w,m)?0:-1):terrain_move(w,m,policy,&contact);
 *ground_y_up=word(((uint16_t)word(transform_y_up(m->transform)+w->camera_y)&1023)-w->camera_y);
 /* Keep the new fraction and integer X, but restore the displayed body Y.
  * The source copies scratch Y into $82, never into displayed $24. */
 transform_integer_offset(m->transform,0,word(body_y-transform_y_up(m->transform)));
 m->transform->priority=priority;
 if(result>=0&&policy!=WMOVE_BODY)priority_from_y(w,m);
 return result;
}
int terrain_foot_contact(struct TerrainWorld*w,struct TerrainMotion*m){
 return terrain_contact_at(w,m,(int16_t)(m->transform->x>>8),transform_y_up(m->transform));
}
int terrain_shape_at(struct TerrainWorld*w,struct TerrainMotion*m,int16_t x,int16_t y_up){
 struct TerrainSample s;if(!sample(w,m,x,y_up,&s))return -1;return !!s.shape;
}
static struct TerrainMotion weapon_motion(struct WeaponActor*a){
 return (struct TerrainMotion){.transform=&a->transform,.hit=&a->hit,.vx=a->vx,.vy=a->vy,.ax=a->ax,.ay=a->ay,
  .slope=a->ground_slope,.ground_tag=a->ground_tag,.movement_flags=a->movement_flags,
  .collision_flags=a->collision_flags,.platform_ids=a->platform_ids,.previous_direction=a->previous_direction};
}
int terrain_weapon_move(void*user,struct WeaponActor*a,enum WeaponMove policy,struct WeaponContact*contact){
 struct TerrainWorld*w=user;struct TerrainMotion m=weapon_motion(a);
 int result=terrain_move(w,&m,policy,contact);
 a->vx=m.vx;a->vy=m.vy;a->ax=m.ax;a->ay=m.ay;a->ground_slope=m.slope;a->ground_tag=m.ground_tag;
 a->platform_ids=m.platform_ids;return result;
}
int terrain_weapon_probe(void*user,const struct WeaponActor*a,struct WeaponContact*contact){
 struct TerrainWorld*w=user;struct HitActor actor={.type_flags=a->hit.type_flags};
 struct TerrainMotion m={.hit=&actor,.collision_flags=a->collision_flags};
 struct TerrainSample s;
 if(!terrain_lookup(w,&m,(int16_t)(a->transform.x>>8),word(transform_y_up(&a->transform)-1),&s))return 0;
 *contact=(struct WeaponContact){.shape=s.shape,.raw=s.raw,.solid=(uint8_t)(occupied(&s)&&!s.half->point_pass)};return 1;
}
int terrain_weapon_foot_contact(void*user,struct WeaponActor*a){
 struct TerrainMotion m=weapon_motion(a);int result=terrain_foot_contact(user,&m);
 a->platform_ids=m.platform_ids;return result;
}
int terrain_weapon_point_sample(void*user,const struct WeaponActor*a,int16_t x,int16_t y,struct WeaponContact*out){
 struct TerrainWorld*w=user;struct HitActor h=a->hit;
 struct TerrainMotion m={.hit=&h,.collision_flags=a->collision_flags,.platform_ids=a->platform_ids};
 struct TerrainSample s;if(!sample(w,&m,x,y,&s))return 0;
 *out=(struct WeaponContact){.shape=s.shape,.raw=s.raw,.solid=!!s.shape};return 1;
}
int terrain_weapon_point_shape(void*user,const struct WeaponActor*a,int16_t x,int16_t y,uint8_t*shape){
 struct WeaponContact out;if(!terrain_weapon_point_sample(user,a,x,y,&out))return 0;*shape=out.shape;return 1;
}

int terrain_weapon_ground_test(struct TerrainWorld*w,struct WeaponActor*a){
 int16_t x=(int16_t)(a->transform.x>>8),y=transform_y_up(&a->transform),vx=a->vx,vy=a->vy;
 transform_integer_offset(&a->transform,0,1);a->vx=0;a->vy=-256;
 struct WeaponContact hit;int r=terrain_weapon_move(w,a,WMOVE_BODY,&hit);
 transform_integer_offset(&a->transform,word(x-(a->transform.x>>8)),word(y-transform_y_up(&a->transform)));
 a->vx=vx;a->vy=vy;return r;
}
int terrain_weapon_ground_status(struct TerrainWorld*w,struct WeaponActor*a,uint8_t*grounded,int point){
 struct TerrainMotion m=weapon_motion(a);if(!terrain_synchronize(w,&m))return 0;
 struct TerrainSample s;int16_t x=(int16_t)(a->transform.x>>8),y=word(transform_y_up(&a->transform)-1);
 if(!terrain_lookup(w,&m,x,y,&s))return 0;
 if(!point)moving_platform(w,&m,x,y,&s);
 int solid=point?(occupied(&s)&&!s.half->point_pass):both_halves(&s);
 if(solid&&!point)a->hit.events|=64;
 *grounded=solid?255:0;
 /* Both solid branches in the original merge write FF; the comparison
  * matters only for the returned probe registers, not this stored byte. */
 a->platform_ids=m.platform_ids;return 1;
}

int terrain_stamp(struct TerrainWorld*w,const struct PlayerTransform*t,int16_t dx,int16_t dy,uint16_t width,uint16_t height,const uint8_t*cells){
 if(!w||!t||!cells||!width||!height)return 0;
 if(!w->write_cell)return failure(w,4);
 unsigned at=0;int16_t x=word((t->x>>8)+dx),y=word(transform_y_up(t)+dy);
 for(unsigned col=0;col<width;col++,x=word(x+8)){
  int16_t sx=word(x-w->camera_x);if(sx< -80||sx>392)continue;
  for(unsigned row=0;row<height;row++)if(!w->write_cell(w->map,x,word(512-y+8*row),cells[at++]))return failure(w,4);
 }return 1;
}
