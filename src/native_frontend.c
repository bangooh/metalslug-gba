#include "native_frontend.h"
#include "native_inflate.h"
#include "../m1_native/runtime.h"
#include "native_link.h"
#include "../generated/result-data.h"
extern const uint8_t title_tiles[],hud_font[];
extern const uint8_t title_maps[];
extern const uint16_t title_palette[];
#define R16(a) (*(volatile uint16_t*)(a))
static void copy(const void*s,uintptr_t dest,unsigned bytes){
 volatile uint32_t*d=(volatile uint32_t*)0x040000d4;d[2]=0;d[0]=(uintptr_t)s;d[1]=dest;
 __asm__ volatile("":::"memory");d[2]=0x84000000u|(bytes/4);while(d[2]&0x80000000u){}
}
static void text(unsigned x,unsigned y,const char*s){
 volatile uint16_t*m=(volatile uint16_t*)0x0600f800;
 for(;*s&&x<30;s++,x++)m[y*32+x]=0xf000+(uint8_t)*s;
}
static void number(unsigned x,unsigned y,unsigned n,unsigned digits,int hex){
 char s[9];if(digits>8)return;s[digits]=0;
 for(unsigned i=digits;i;i--){unsigned v=hex?n&15:n%10;n=hex?n>>4:n/10;s[i-1]="0123456789ABCDEF"[v];}text(x,y,s);
}
static char glyph(unsigned value){
 static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890.?!- ";
 for(unsigned i=0;i<40;i++)if(result_alphabet[i]==value)return alphabet[i];return ' ';
}
void native_frontend_title_tick(unsigned age){
 if(age%30==0)text(10,16,(age/30)&1?"          ":"PUSH START");
}
void native_frontend_link_progress(unsigned percent){if(percent>100)percent=100;number(13,15,percent,3,0);text(16,15,"%");}
void native_frontend_show(enum NativeFrontendScreen screen,unsigned selection,const struct HighScoreRecord*table,unsigned detail){
 R16(0x04000000)=128;R16(0x04000050)=0;R16(0x04000054)=0;
 for(unsigned i=0;i<128;i++)((volatile uint16_t*)0x07000000)[i*4]=0x200;
 copy(hud_font,0x0600c000,96*32);
 for(unsigned i=0;i<1024;i++)((volatile uint16_t*)0x0600f800)[i]=0;
 for(unsigned i=0;i<8;i++)R16(0x04000010+i*2)=0;
 R16(0x04000008)=0x1f0c;R16(0x05000000)=0;R16(0x050001e2)=0x7fff;
 unsigned display=0x0140;
 switch(screen){
 case NFRONT_TITLE:{
  m1_runtime_load(2);
  copy(title_palette,0x05000000,512);native_inflate(title_tiles,(void*)0x06000000,28864,1);native_inflate(title_maps,(void*)0x0600e000,6144,1);
  R16(0x05000000)=0; /* Transparent backdrop must not expose asset magenta. */
  R16(0x050001e2)=0x7fff;R16(0x0400000a)=0x1e01;R16(0x0400000c)=0x1d02;R16(0x0400000e)=0x1c03;display=0x0f40;
  (void)selection;
  text(10,16,"PUSH START");text(3,18,"(C)1996 NAZCA CORPORATION");
  if(detail)text(10,19,"2P LINK OK");break;
 }
 case NFRONT_CONTROLS:
  text(9,2,"HOW TO PLAY");text(3,5,"D-PAD   MOVE / AIM");text(3,7,"A       FIRE");text(3,9,"B       JUMP");text(3,11,"R       GRENADE");text(3,13,"START   PAUSE / CONTINUE");text(2,17,"A: TUTORIAL   B: TITLE");break;
 case NFRONT_RANKING:
  text(9,1,"HIGH SCORES");text(2,3,"RANK   NAME      SCORE");
  if(table)for(unsigned i=0;i<10;i++){
   number(3,5+i,table[i].rank,2,0);char name[4];for(unsigned j=0;j<3;j++)name[j]=glyph(table[i].name[j]);name[3]=0;
   text(10,5+i,name);number(18,5+i,table[i].packed_score,8,1);
  }text(9,18,"B: TITLE");break;
 case NFRONT_LINK_WAIT:
  text(7,3,"2 PLAYER LINK");text(6,7,"CONNECTED AS 2P");text(5,10,"WAITING FOR MASTER");text(3,14,"GAME STARTS AUTOMATICALLY");break;
 case NFRONT_LINK_SYNC:
  text(7,3,"2 PLAYER LINK");text(8,8,"SYNCHRONIZING");text(4,12,"PLEASE KEEP CONNECTED");native_frontend_link_progress(0);break;
 case NFRONT_LINK_ERROR:{
  text(7,3,"LINK STOPPED");const char*reason=detail==3?"PARTNER LEFT":detail==4?"CONNECTION LOST":detail==5?"VERSIONS DO NOT MATCH":detail==6?"GAME STATES DIFFER":"TRANSFER ERROR";
  if(detail==5)switch(native_link_diagnostic.code){
   case 1:reason="LINK PROTOCOL DIFFERS";break;
   case 2:reason="PLAYER ROLE CONFLICT";break;
   case 4:reason="SESSION ID DIFFERS";break;
   case 5:reason="LINK SETTINGS DIFFER";break;
   case 6:reason="PACKET SIZE DIFFERS";break;
  }
  text(3,6,reason);text(2,8,"LINK CODE:");number(17,8,native_link_diagnostic.code,2,0);
  text(2,9,"SLOT LOCAL/PEER:");number(19,9,native_link_diagnostic.local_slot,2,1);number(23,9,native_link_diagnostic.peer_slot,2,1);
  text(2,10,"SIO INIT/LAST:");number(17,10,native_link_diagnostic.sio_initial,4,1);number(22,10,native_link_diagnostic.sio_last,4,1);
  text(2,11,"EXPECTED:");number(13,11,native_link_diagnostic.expected,8,1);
  text(2,12,"RECEIVED:");number(13,12,native_link_diagnostic.actual,8,1);
  text(2,13,"WORDS:");number(13,13,native_link_diagnostic.words,2,0);
  text(6,17,"A: RETRY  B: TITLE");break;
 }
 case NFRONT_RETURN:text(7,7,"RETURNING TO TITLE");break;
 case NFRONT_ERROR:
  text(6,4,"GAME STOPPED");text(4,8,"DETAIL");number(12,8,detail,8,1);text(5,15,"B: RETURN TO TITLE");break;
 }
 R16(0x04000000)=display;
}
