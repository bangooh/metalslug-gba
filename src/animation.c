#include "animation.h"
static const struct AnimationOutput*output;
const struct AnimationOutput*animation_output_scope(const struct AnimationOutput*next){
 const struct AnimationOutput*previous=output;output=next;return previous;
}
int animation_output_direct(struct Animation*a,enum AnimationDrawMode mode){
 if(!output)return 1;
 if(!a)return 0;
 if(!output->draw||!output->draw(output->context,a,mode)){a->fault=13;return 0;}
 return 1;
}
int animation_output_draw(struct Animation*a){return animation_output_direct(a,AN_OUTPUT_SCRIPT);}
void animation_begin(struct Animation*a,uint16_t start){
 /* $028CD4 clears pose/delay/loop only. Changing the stream must not cancel
  * hit-stop ($44), and the previous drawing stays published until replaced. */
 uint8_t freeze=a->freeze,flags=a->drawing_flags;uint16_t drawing=a->drawing;
 const struct AnimationProgram*program=a->drawing_program;
 *a=(struct Animation){.drawing_program=program,.cursor=start,.drawing=drawing,.last_draw_cursor=start,.freeze=freeze,.drawing_flags=flags};
}
void animation_drawing(struct Animation*a,const struct AnimationProgram*p,uint16_t n){a->drawing_program=p;a->drawing=n;a->drawing_flags=0;}
static int fail(struct Animation*a,unsigned code){a->fault=(uint8_t)code;return 0;}
static uint32_t mask(unsigned width){return width==1?255u:width==2?65535u:0xffffffffu;}
int animation_step(struct Animation*a,const struct AnimationProgram*p,const struct AnimationOwner*o){
 if(a->fault)return 0;
 a->complete=0;
 /* $028D8E increments the drawing ordinal, saturating at $FF. */
 if(a->cursor!=a->last_draw_cursor&&a->pose!=255)a->pose++;
 /* Source $028D70..$029404 has a finite dispatch chain before each drawing.
  * Bound malformed generated data, without changing the actor tick rate. */
 for(unsigned budget=0;budget<512;budget++){
  if(a->cursor>=p->command_count)return fail(a,1);
  const struct AnimationCommand*c=&p->commands[a->cursor];
  switch(c->op){
   case AN_WAIT:
    if(!a->delay)a->delay=c->argument;
    a->cursor=c->next;break;
   case AN_JUMP:a->cursor=c->target;break;
   case AN_DRAW:
    animation_drawing(a,p,c->target);a->last_draw_cursor=a->cursor;a->drawing_flags=c->argument;
    if(!animation_output_draw(a))return 0;
    if(!a->delay){a->complete=1;return 1;}
    if(a->freeze&&!c->argument)return 0;
    if(!--a->delay){
     if(c->next>=p->command_count)return fail(a,1);
     if(p->commands[c->next].op==AN_END){a->complete=1;return 1;}
     a->cursor=c->next;
    }
    return 0;
   case AN_LOOP:
    /* $02902C: byte ADDQ, then SUBI/ADDX saturation at zero. */
    if(!a->counter)a->counter=(uint8_t)(c->argument+1);
    if(a->counter)a->counter--;
    a->cursor=a->counter?c->target:c->next;break;
   case AN_SOUND:case AN_SOUND_STOP:
    if(!o->sound||!o->sound(o->context,(uint16_t)c->value,c->op==AN_SOUND_STOP))return fail(a,2);
    a->cursor=c->next;break;
   case AN_SET:case AN_AND:case AN_OR:case AN_XOR:{
    if(c->width!=1&&c->width!=2&&c->width!=4)return fail(a,3);
    enum AnimationFieldOperation op=c->op==AN_SET?AN_FIELD_SET:c->op==AN_AND?AN_FIELD_AND:c->op==AN_OR?AN_FIELD_OR:AN_FIELD_XOR;
    if(!o->write||!o->write(o->context,c->target,c->width,op,c->value&mask(c->width)))return fail(a,4);
    a->cursor=c->next;break;
   }
   case AN_CALL:
    /* $029122 restores its saved stream continuation after the callback. */
    if(!o->call||!o->call(o->context,c->target,a))return fail(a,5);
    a->cursor=c->next;break;
   case AN_DEFENSE:case AN_ATTACK:case AN_CONTACT:case AN_PARAMETERS:
    if(!o->bind||!o->bind(o->context,c->op,c->value))return fail(a,6);
    a->cursor=c->next;break;
   case AN_PALETTE:
    if(!o->read||!o->bind||!o->bind(o->context,AN_PALETTE,o->read(o->context,c->target,2)))return fail(a,7);
    a->cursor=c->next;break;
   case AN_WAIT_TABLE:
    if(!a->delay){
     if(!o->read||!o->delay||!o->delay(o->context,c->target,o->read(o->context,c->argument,1),&a->delay))return fail(a,8);
    }
    a->cursor=c->next;break;
   case AN_SELECT:{
    if(!o->read)return fail(a,9);
    unsigned choice=o->read(o->context,c->argument,2);
    if(choice>=c->value||c->target+choice>=p->choice_count)return fail(a,10);
    a->cursor=p->choices[c->target+choice];break;
   }
   case AN_END:
    /* $029334 reloads the last drawing ($40) and enters ordinary DRAW
     * at $028DFE. END holds that picture; it does not hide the actor.
     * This also applies after zero-delay commands following alternate
     * DRAW ($1E), used by rocks, village props and the barrel soldier. */
    if(a->last_draw_cursor<p->command_count&&p->commands[a->last_draw_cursor].op==AN_DRAW)
     animation_drawing(a,p,p->commands[a->last_draw_cursor].target);
    if(a->drawing!=65535){a->drawing_flags=0;if(!animation_output_draw(a))return 0;}
    a->complete=1;return 1;
   default:return fail(a,11);
  }
 }
 return fail(a,12);
}
