#include "interactions.h"
#include "items.h"
#include "slug_body.h"
static int16_t word(int x){return (int16_t)(uint16_t)x;}
static void axis(const struct PlayerTransform*t,const int16_t*r,unsigned y,int16_t*lo,int16_t*hi){
 int16_t l=r[y*2],h=r[y*2+1];if(t->facing&(1u<<y)){int16_t tmp=l;l=word(-h);h=word(-tmp);}
 int16_t origin=y?transform_y_up(t):word(t->x>>8);*lo=word(l+origin);*hi=word(h+origin);
}
static void contact(struct WeaponActor*a,const struct InteractionContact*s,const struct PlayerTransform*t,struct HitActor*h,uint8_t*motion,const struct InteractionContact*b){
 if(*motion&8)return;
 for(unsigned y=0;y<2;y++){int16_t al,ah,bl,bh;axis(&a->transform,s->rect,y,&al,&ah);axis(t,b->rect,y,&bl,&bh);if(bl>ah||al>bh)return;}
 int source_left=word((a->transform.x>>8)+s->bias)<word((t->x>>8)+b->bias);
 if(!(h->type_flags&64))a->movement_flags|=(uint8_t)(1u<<(source_left?0:1));
 *motion=(uint8_t)((*motion|(1u<<(source_left?1:0)))&~4u);if(s->kind==2)*motion|=4;
}
void interaction_body_contact(struct WeaponWorld*w,struct WeaponActor*a,const struct InteractionContact*source){
 if(w->items&&w->items->slug){struct SlugSharedState*s=w->items->slug;
  if(s->body_contact>=SBC_IDLE&&s->body_contact<=SBC_DESTROYED)contact(a,source,&s->actor.transform,&s->actor.hit,&s->actor.movement_flags,&interaction_contacts[1+s->body_contact]);
 }
 for(unsigned i=0;i<2;i++){struct PlayerBodyState*p=w->players[i];if(!p||!p->body_contact)continue;
  if(p->body_contact>2){interaction_fault(w);return;}contact(a,source,&p->actor.transform,&p->actor.hit,&p->actor.movement_flags,&interaction_contacts[p->body_contact-1]);
 }
}
void crate_body_contact(struct WeaponWorld*w,struct WeaponActor*a){interaction_body_contact(w,a,&interaction_contacts[5]);}
