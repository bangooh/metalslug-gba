#include "hitbox.h"
static const uint8_t default_filter[36]={0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,1,1,1,0,0};

/* $0283CA. Deliberately leaves the accumulated event byte untouched. */
void hit_begin_attack(struct HitActor *actor){actor->flags&=(uint8_t)~6u;}
static void axis(const struct HitRect *rect,const struct HitActor *actor,
 unsigned y,int16_t *lo,int16_t *hi){
 int16_t a=y?rect->bottom:rect->left,b=y?rect->top:rect->right;
 if(actor->flip&(1u<<y)){int16_t t=a;a=(int16_t)-b;b=(int16_t)-t;}
 int16_t origin=y?actor->y:actor->x;
 *lo=(int16_t)((uint16_t)a+(uint16_t)origin);
 *hi=(int16_t)((uint16_t)b+(uint16_t)origin);
}
/* $02876E..$028924. Inclusive edges, WORD arithmetic and the unsigned
 * intersection-center comparisons are intentional source behavior. Point
 * attacks do not mirror their local point when the attacker flips. */
static int overlap(const struct HitAttack *attack,const struct HitActor *actor,
 const struct HitDefense *defense,struct HitActor *target){
 int16_t center[2];
 for(unsigned y=0;y<2;y++){
  int16_t a,b,c,d;axis(&defense->rect,target,y,&c,&d);
  if(attack->shape==0){
   a=(int16_t)((uint16_t)(y?attack->rect.bottom:attack->rect.left)+
              (uint16_t)(y?actor->y:actor->x));
   if(c>a||a>d)return 0;center[y]=a;
  }else if(attack->shape==4){
   axis(&attack->rect,actor,y,&a,&b);
   if(c>b||a>d)return 0;
   if((uint16_t)a<(uint16_t)c)a=c;
   if((uint16_t)b>=(uint16_t)d)b=d;
   center[y]=(int16_t)((int16_t)((uint16_t)a+(uint16_t)b)>>1);
  }else return 0; /* converter rejects source shapes other than 0 and 4 */
 }
 target->impact_x=center[0];target->impact_y=center[1];return 1;
}
static void mark(struct HitActor *actor,unsigned flags){actor->flags|=flags;actor->events|=flags;}
/* $028576..$028708. The source score dispatcher at $051914 is surfaced as
 * a typed damage event, called at the same point; no CPU callback survives.
 * Caller owns ordered target lists, offscreen filtering and per-tick reset. */
enum HitResult hit_pair(struct HitActor *actor,const struct HitAttack *attack,
 struct HitActor *target,const struct HitDefense *defenses,unsigned count,
 HitDamageSink sink,void *user){
 if(actor==target||!defenses||(actor->flags&2)||(target->flags&8)||attack->kind>34)return HIT_MISS;
 for(unsigned i=0;i<count;i++){
  const struct HitDefense *defense=&defenses[i];
  const uint8_t*filter=defense->filter?defense->filter:default_filter;
  if(filter[attack->kind]==1)continue;
  if(!overlap(attack,actor,defense,target))continue;
  actor->contact_resistance=255;
  if(target->invulnerable){mark(actor,2);return HIT_BLOCKED;}
  target->last_kind=attack->kind;target->last_attacker=actor->id;
  if(!attack->strength){mark(actor,2);return HIT_BLOCKED;}
  if(!attack->kind||((target->type_flags&1)&&attack->kind==11)){
   mark(actor,6);return HIT_CONTACT;
  }
  target->flags|=8;target->events|=1;
  int16_t before=target->hp;
  /* SUB.W then BGT uses signed overflow flags: compare operands BEFORE
   * wrapping, rather than testing the wrapped difference's sign. */
  if(before>attack->damage)target->hp=(int16_t)((uint16_t)before-(uint16_t)attack->damage);
  else {target->hp=0;target->flags|=1;}
  if(sink){struct HitDamage event={actor->id,target->id,before,target->hp,attack->kind,(uint8_t)!!(target->flags&1)};sink(user,&event);}
  /* CMP.B/BGT is signed, including resistance values with bit 7 set. */
  actor->contact_resistance=defense->resistance;
  mark(actor,(int8_t)attack->strength>(int8_t)defense->resistance?4:6);
  return HIT_DAMAGE;
 }
 return HIT_MISS;
}
