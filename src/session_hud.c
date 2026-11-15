#include "session_internal.h"
#include "slug_support.h"
/* The HUD's single cells need neither rectangle traversal nor atlas math.
 * Keep the generic backend, clipping and both video/weapon fault states. */
static __attribute__((noinline)) int cell(struct NativePlayWorld*w,uint16_t at,uint16_t tile){
 struct SceneVideoWorld*v=w->weapons.video;
 if(v&&v->write_fixed){if(at>=1280||v->write_fixed(v->context,at>>5,at&31,tile))return 1;}
 if(v)v->fault=1;return enemy_fault(&w->weapons);
}
int session_hud_clear(struct NativePlayWorld*w,unsigned slot){return result_rect(&w->weapons,slot?0x2c3:0x43,0x300,16,3,0);}
/*260EE shows spare lives, not the life currently in play. */
static int lives(struct NativePlayWorld*w,unsigned slot){
 unsigned n=w->session->player[slot].lives;if(n)n--;if(n>99)n=99;uint16_t at=slot?0x465:0xe5;
 return cell(w,at,0x5330+(n>=10?n/10:n))&&cell(w,at+32,n>=10?0x5330+n%10:0x5320);
}
/*2614E: integer approach then the original wrapping16.16 damped bar.
 * Preserve its one-frame mounting transition and low-HP palette pulses. */
static int health(struct NativePlayWorld*w,unsigned slot){
 struct NativePlayerControl*c=&w->session->player[slot];
 int mounted=w->slug.present&&slug_ridden_by(&w->slug,slot),amount=0,changed=0,low=0;
 int16_t current=(int16_t)(c->hud.hp>>16),target=w->slug.actor.hit.hp;
 if(!c->hud.hp_phase){if(mounted)c->hud.hp_phase=1;changed=current;c->hud.hp=0;}
 else if(c->hud.hp_phase==1){
  if(!mounted){changed=1;c->hud.hp_phase=0;c->hud.hp=0;}
  else {int16_t d=(int16_t)(uint16_t)(target-current);if(d>2)d=2;if(d< -2)d=-2;changed=d;
   c->hud.hp+=((uint32_t)(int32_t)d<<16);amount=(int16_t)(c->hud.hp>>16);
   if(!d){c->hud.hp_phase=2;c->hud.hp_speed=0;}}
 }else if(!mounted){changed=1;c->hud.hp_phase=0;c->hud.hp=c->hud.hp_speed=0;}
 else {
  low=(uint16_t)target<17;uint32_t desired=(uint32_t)(uint16_t)target<<16;
  uint32_t spring=(c->hud.hp-desired)<<1;
  uint32_t damping=(uint32_t)((int32_t)c->hud.hp_speed>>2)+spring;
  uint32_t velocity=c->hud.hp_speed-damping;c->hud.hp_speed=velocity;c->hud.hp+=velocity;
  amount=(int16_t)(c->hud.hp>>16);changed=(int16_t)(velocity>>16);
  if(!((uint16_t)(velocity>>16)|(uint16_t)(spring>>16))){if(c->hud.hp!=desired)changed=1;c->hud.hp=desired;c->hud.hp_speed=0;}
  if(amount<0)amount=0;
 }
 uint16_t palette=0x7a00;
 if(low){unsigned phase=w->weapons.frame_phase&7;if(!phase)palette=0xda00;else if(phase!=2&&!changed)return 1;}
 else if(!changed)return 1;
 uint16_t at=slot?0x3c4:0x44;if(!cell(w,at,palette|0xa5))return 0;at+=32;
 for(unsigned i=0;i<6;i++,at+=32){unsigned n=amount>=8?8:amount>0?(unsigned)amount:0;if(!cell(w,at,palette|(0xa6+n)))return 0;amount-=8;}
 return cell(w,at,palette|0xaf);
}
static int rescued(struct NativePlayWorld*w,unsigned slot){
 struct NativePlayerControl*c=&w->session->player[slot];uint8_t count=w->interactions.rescued[slot];
 if(count==c->hud.rescued)return 1;c->hud.rescued=count;if(count>14)count=14;
 uint16_t at=slot?0x2fc:0x7c;
 for(unsigned i=0;i<14;i++)if(!cell(w,at+32*i,(slot?i>=14u-count:i<count)?0x238a:0x2320))return 0;return 1;
}
static int ammunition(struct NativePlayWorld*w,unsigned slot){
 struct NativePlayerControl*c=&w->session->player[slot];struct PlayerBodyState*p=w->weapons.players[slot];if(!p)return 1;
 int16_t ammo=p->actor.local_weapon?(int16_t)p->ammo:-1;if(ammo<0)ammo=0;
 if(ammo!=c->hud.ammo){c->hud.ammo=ammo;c->hud.ammo_blink=4;}
 /* Numeric updates remain immediate. Cosmetic E/F palette flashing used
  * a second rectangle plane and palette bank for every changing counter. */
 if(c->hud.ammo_blink){--c->hud.ammo_blink;uint16_t pal=0xe300,at=slot?0x324:0x1a4;unsigned n=(uint16_t)ammo;
  for(unsigned i=0;i<3;i++,at-=32){uint16_t tile=ammo?(pal|((n%10)+0x30)):(0xe3f2-i);if(!cell(w,at,tile))return 0;n/=10;}}
 uint8_t mode=w->slug.present&&slug_ridden_by(&w->slug,slot)&&!(w->slug.status_flags&16)?255:0;
 if(mode!=c->hud.grenade_mode){
  c->hud.grenade_mode=mode;
  if(!result_rect(&w->weapons,slot?0x343:0x1c3,mode?0x7ae0:0x7ae4,4,1,1))return 0;
  c->hud.grenade_blink=4;
 }else {
  int n=(int8_t)(mode?w->slug.ammo:p->grenades);if(n<0)n=0;
  if(n!=c->hud.grenades){c->hud.grenades=n;c->hud.grenade_blink=4;}
 }
 if(c->hud.grenade_blink){--c->hud.grenade_blink;uint16_t pal=0xe300,at=slot?0x384:0x204;unsigned n=c->hud.grenades;
  if(!cell(w,at,pal|(0x30+n%10))||!cell(w,at-32,pal|(0x30+(n/10)%10)))return 0;}
 return 1;
}
static int score(struct NativePlayWorld*w,unsigned slot){
 unsigned value=w->items.scores[slot]%10000000;uint8_t digits[7];for(unsigned i=7;i;i--){digits[i-1]=value%10;value/=10;}
 unsigned seen=0;uint16_t at=slot?0x3c3:0x63;
 for(unsigned i=0;i<7;i++){seen|=digits[i];if(!cell(w,at+32*i,0x2300|((seen||i==6)?0x30+digits[i]:0x20)))return 0;}return 1;
}
int session_hud_player(struct NativePlayWorld*w,unsigned slot,int initialize){
 if(slot>1||!w->session)return session_fail(w);
 struct NativePlayerControl*c=&w->session->player[slot];
 if(initialize>0){
  /* A join can occur before the idle prompt's blinking clear. Score and
   * ammo cover only part of that text: erase it before drawing live HUD. */
  if(!session_hud_clear(w,slot))return 0;
  c->hud=(__typeof__(c->hud)){.ammo_blink=1,.grenade_blink=1};
  if(!result_stream(&w->weapons,slot?0x3e5:0x65,&session_up[slot],0x5300))return 0;
  uint16_t at=slot?0x3c4:0x44;
  if(!cell(w,at,0x7aa5)||!result_rect(&w->weapons,at+32,0x7aa6,6,1,0)||!cell(w,at+224,0x7aaf)||
   !result_rect(&w->weapons,slot?0x2c3:0x143,0x7ab0,8,3,1)||
   !result_rect(&w->weapons,slot?0x343:0x1c3,0x7ae4,4,1,1)||!lives(w,slot))return 0;
 }
 if(initialize>=0&&!health(w,slot))return 0;
 if(!rescued(w,slot))return 0;
 if(w->terrain.scene>=11&&w->terrain.scene<=13)return 1;
 return ammunition(w,slot)&&score(w,slot);
}
