#include "bosses.h"
struct FxStep {uint16_t drawing,cursor,next;uint8_t delay,flags;};
static const struct FxStep steps[]={
#include "../generated/fx-steps.inc"
};
int boss_fx_animate(struct WeaponWorld*w,struct NativeEntity*e){
 struct Animation*a=&e->payload.enemy.actor.animation;
 if(a->fault||a->cursor>=sizeof(steps)/sizeof(*steps))return boss_animate(w,e,0);
 const struct FxStep*s=&steps[a->cursor];
 if(!(s->flags&128))return boss_animate(w,e,0);
 a->complete=0;
 if(a->cursor!=a->last_draw_cursor&&a->pose!=255)a->pose++;
 if(!a->delay)a->delay=s->delay;
 a->cursor=s->cursor;a->drawing_program=&boss_program;a->drawing=s->drawing;
 a->last_draw_cursor=s->cursor;a->drawing_flags=s->flags&1;
 if(!animation_output_draw(a)){enemy_fault(w);return 0;}
 if(!a->delay){a->complete=1;return 1;}
 if(a->freeze&&!(s->flags&1))return 0;
 if(!--a->delay){if(s->flags&2){a->complete=1;return 1;}a->cursor=s->next;}
 return 0;
}
