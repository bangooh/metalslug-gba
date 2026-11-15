#ifndef NATIVE_ANIMATION_H
#define NATIVE_ANIMATION_H
#include <stdint.h>
/* Animation data, not CPU instructions. All indices address generated typed
 * arrays. No original PC, registers, RAM offsets or executable ROM pointers. */
enum AnimationOp {
 AN_WAIT,AN_JUMP,AN_DRAW,AN_LOOP,AN_SOUND,AN_SOUND_STOP,
 AN_SET,AN_AND,AN_OR,AN_XOR,AN_CALL,AN_DEFENSE,AN_ATTACK,AN_CONTACT,
 AN_PALETTE,AN_END,AN_PARAMETERS,AN_WAIT_TABLE,AN_SELECT
};
struct AnimationCommand {
 uint32_t value;
 uint16_t next,target;
 uint8_t op,width,argument,pad;
};
struct AnimationProgram {
 const struct AnimationCommand *commands;
 const uint16_t *choices;
 uint16_t command_count,choice_count;
};
struct Animation {
 /* The last published DRAW owns its program until another drawing replaces
  * it, even across animation_begin or graph changes. Renderer identity. */
 const struct AnimationProgram *drawing_program;
 uint16_t cursor,drawing,last_draw_cursor;
 uint8_t delay,counter,freeze,complete;
 uint8_t fault,pose,drawing_flags,pad; /* drawing_flags bit0: alternate1E draw */
};
enum AnimationFieldOperation {AN_FIELD_SET,AN_FIELD_AND,AN_FIELD_OR,AN_FIELD_XOR};
/* Fields are compiler-assigned semantic IDs resolved by each owner. The
 * owner has ordinary C members; it does not expose a byte-addressed bus. */
struct AnimationOwner {
 void *context;
 uint32_t (*read)(void*,unsigned field,unsigned width);
 int (*write)(void*,unsigned field,unsigned width,enum AnimationFieldOperation,uint32_t);
 int (*call)(void*,unsigned callback,struct Animation*);
 int (*bind)(void*,unsigned kind,uint32_t value);
 int (*sound)(void*,uint16_t command,int stop);
 int (*delay)(void*,unsigned table,unsigned pose,uint8_t*);
};
/* A serialized simulation frame may attach a real drawing sink. Invoke at
 * DRAW itself: an actor can move/change palette/delete itself afterwards.
 * The sink snapshots owner state synchronously and may not retain a pointer
 * to an Animation that the scheduler can subsequently recycle. NULL scope
 * is for source-only simulation; it is not a physical video backend. */
enum AnimationDrawMode {AN_OUTPUT_SCRIPT,AN_OUTPUT_DIRECT,AN_OUTPUT_PROJECTED,
 AN_OUTPUT_DIRECT_FORCED,AN_OUTPUT_PROJECTED_FORCED};
struct AnimationOutput {
 void*context;
 int (*begin)(void*);
 int (*draw)(void*,struct Animation*,enum AnimationDrawMode);
 int (*end)(void*);
};
const struct AnimationOutput*animation_output_scope(const struct AnimationOutput*);
int animation_output_draw(struct Animation*);
/* Manual source dispatch is explicit. Merely assigning a drawing in an
 * initializer must not submit it before that handler reaches its draw call. */
int animation_output_direct(struct Animation*,enum AnimationDrawMode);
void animation_begin(struct Animation*,uint16_t start);
void animation_drawing(struct Animation*,const struct AnimationProgram*,uint16_t);
/* A true result is the source completion/carry condition. fault is separate. */
int animation_step(struct Animation*,const struct AnimationProgram*,const struct AnimationOwner*);
#endif
