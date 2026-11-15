#include "enemies.h"
#include "slug_animation.h"
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static struct SlugSharedState*mounted(struct WeaponWorld*w,struct EnemyActor*q){
 int p=q->infantry.target;struct SlugSharedState*s=w->enemies->slug;
 if(p<0||p>1||!w->players[p]||!s||!s->present||!s->rider_heartbeat||!slug_ridden_by(s,(unsigned)p))return 0;
 /* $02AC4C also copies the player's score owner on every successful query. */
 q->actor.owner=w->players[p]->actor.owner;return s;
}
static int point(struct SlugSharedState*s,unsigned i,int adjusted,int16_t*x,int16_t*y){
 if(i>2||!s->mount_valid||s->mount_index>=SLUG_MOUNT_COUNT)return 0;
 const struct SlugMount*m=&slug_mounts[s->mount_index];
 int16_t dx=i==0?m->turret_x:i==1?m->secondary_x:m->cannon_x;
 int16_t dy=i==0?m->turret_y:i==1?m->secondary_y:m->cannon_y;
 if(!(dx|dy))return 0; /* Empty source pair, before the anchor-0 adjustment. */
 *x=word((s->actor.transform.x>>8)+dx+(adjusted&&!i?16:0));
 *y=word(transform_y_up(&s->actor.transform)+dy+(adjusted&&!i?14:0));return 1;
}
uint8_t enemy_anchor_mask(struct WeaponWorld*w,struct EnemyActor*q){
 struct SlugSharedState*s=mounted(w,q);if(!s)return 255;
 uint8_t bits=s->input_masks[0]|s->input_masks[1];bits|=bits>>4;
 for(unsigned i=0;i<3;i++){int16_t x,y;if(!(bits&(1u<<i))&&!point(s,i,0,&x,&y))bits|=(uint8_t)(1u<<i);}
 return bits;
}
int enemy_anchor(struct WeaponWorld*w,struct EnemyActor*q,unsigned i,unsigned mode,int16_t*x,int16_t*y){
 struct SlugSharedState*s=mounted(w,q);if(!s||i>2)return 0;
 uint8_t bits=s->input_masks[0]|s->input_masks[1];
 /* Claim/approach reject occupied LOW and inhibited HIGH bits; a current
  * occupant refreshes despite its own low bit and only checks inhibition. */
 if((mode==2?bits>>4:(bits|(bits>>4)))&(1u<<i))return 0;
 if(mode==2)s->input_masks[(~w->frame_phase)&1]|=(uint8_t)(1u<<i);
 if(!point(s,i,1,x,y))return 0;
 if(mode==1)s->input_masks[(~w->frame_phase)&1]|=(uint8_t)(1u<<i);
 q->actor.transform.priority=(uint16_t)((s->actor.transform.priority&0xffe3)|24);return 1;
}
void enemy_anchor_detach_cannon(struct WeaponWorld*w,struct EnemyActor*q){
 struct SlugSharedState*s=mounted(w,q);if(s)s->input_masks[(~w->frame_phase)&1]|=8;
}
void enemy_zone_publish(struct WeaponWorld*w,const struct EnemyZone*z){
 struct EnemyZones*r=&w->enemies->zones;r->records[r->write]=*z;
 if(r->count<4){r->write=(r->write+1)&7;r->count++;}
}
int enemy_zone_contains(struct WeaponWorld*w,const struct WeaponActor*a){
 const struct EnemyZones*r=&w->enemies->zones;
 if(!w->secondary_view_valid||!w->interactions||!w->interactions->terrain){enemy_fault(w);return 0;}
 const struct TerrainWorld*t=w->interactions->terrain;int pending=!(a->hit.flags&64);
 uint16_t x=(uint16_t)((a->transform.x>>8)-w->camera_x+w->secondary_x_offset+(pending?t->delta_x:0)),y=(uint16_t)(transform_world_y(&a->transform)-w->camera_y+w->secondary_y_offset+(pending?t->delta_y:0));
 for(unsigned i=r->first;i!=r->end;i=(i+1)&7){const struct EnemyZone*z=&r->records[i];
  if(x>=z->left&&x<=z->right&&y>=z->top&&y<=z->bottom)return 1;
 }return 0;
}
static void zone_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct EnemyActor*q=&e->payload.enemy;if(w->fault)return;
 if(!w->secondary_view_valid){enemy_fault(w);return;}
 if(q->entry){q->entry=0;int16_t x=word((q->actor.transform.x>>8)-w->camera_x+w->secondary_x_offset),y=word(transform_world_y(&q->actor.transform)-w->camera_y+w->secondary_y_offset);q->actor.transform.x=(int32_t)x*256;q->actor.transform.y=(int32_t)y*256;q->infantry.radius=(uint16_t)(x+(q->params[0]<<4));q->infantry.lifetime=(uint16_t)(y+(q->params[1]<<4));}
 /* Source $08F18C stores world WORD bounds, without camera synchronization. */
 struct EnemyZone z={(uint16_t)(q->actor.transform.x>>8),(uint16_t)transform_world_y(&q->actor.transform),q->infantry.radius,q->infantry.lifetime};
 if(z.right<(uint16_t)w->secondary_x_offset){entity_delete_children(p,e);entity_delete(p,e);return;}
 enemy_zone_publish(w,&z);
}
int enemy_zone_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,const uint8_t params[8]){
 struct NativeEntity*e=enemy_allocate(w,p,o,t,zone_tick);if(!e)return 0;struct EnemyActor*q=&e->payload.enemy;
 if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];return 1;
}
