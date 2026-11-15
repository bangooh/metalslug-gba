#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "runtime.c"
static unsigned reference(const struct History*h,int x,int y){
 const struct M1Image*m=&assets->images[h->image];if(x<0||y<0||x>=m->width||y>=m->height)return 0;
 unsigned id=assets->maps[m->offset+y*m->width+x];
 for(unsigned i=0;i<h->count;i++){
  const struct M1Patch*p=&assets->patches[h->patches[i]];if(x<p->x||y<p->y||x>=p->x+p->width||y>=p->y+p->height)continue;
  const struct M1Op*o=&assets->ops[assets->patchmaps[p->offset+(y-p->y)*p->width+x-p->x]];
  if(o->replace!=65535)id=o->replace;
  else {unsigned j;for(j=0;j<o->count;j++)if(assets->transitions[o->offset+j][0]==id){id=assets->transitions[o->offset+j][1];break;}assert(j<o->count);}
 }return id;
}
static void run(unsigned mission){
 assets=&cartridges[mission-1];unsigned patch_count=mission==1?M1_PATCH_COUNT:M2_PATCH_COUNT;
 struct NativeVideo*v=calloc(1,sizeof(*v));assert(v);unsigned rng=1,checked=0;
 for(unsigned image=0;image<assets->image_count;image++)for(unsigned trial=0;trial<60;trial++){
  struct History h={.image=image,.count=trial%24};
  for(unsigned i=0;i<h.count;i++){rng=rng*1664525+1013904223;h.patches[i]=(rng>>8)%patch_count;}
  rng=rng*1664525+1013904223;int x=(int)((rng>>8)%(assets->images[image].width+32))-16;
  rng=rng*1664525+1013904223;int y=(int)((rng>>8)%(assets->images[image].height+24))-12;
  assert(viewport(v,0,&h,x,y));
  for(int yy=0;yy<21;yy++)for(int xx=0;xx<31;xx++){assert(v->backgrounds.maps[0][yy*31+xx]==reference(&h,x+xx,y+yy));checked++;}
 }
 /* Rebinding before a new image reaches the viewport retains old terrain. */
 const unsigned before[]={0,3,10},after[]={3,10,12},camera[]={1504,2976,3360},cy[]={144,144,32};
 for(unsigned k=0;k<(mission==1?3:0);k++){
  struct History a={.image=before[k]},b={.image=after[k]};
  int ox=(camera[k]*3/4)/8,oy=(cy[k]*3/4+16)/8;
  assert(viewport(v,0,&a,ox,oy));assert(viewport(v,1,&b,ox,oy));
  for(unsigned i=0;i<651;i++)assert(v->backgrounds.maps[0][i]==v->backgrounds.maps[1][i]);
 }
 struct NativePlayWorld*w=calloc(1,sizeof(*w));struct SceneRuntime*r=calloc(1,sizeof(*r));assert(w&&r);v->world=w;w->scene_runtime=r;reset(v,mission-1);
 for(unsigned p=0;p<4;p++){r->maps.planes[p].image=p<2?assets->first_image+(p?2:0):65535;w->video.views[p].valid=p<2;w->video.views[p].first_column=32+p*32;}
 for(unsigned tick=0;tick<1200;tick++){
  unsigned phase=tick%240;int x=(phase<120?phase:240-phase)*8-64,y=(tick%47)-16;
  r->maps.planes[0].x=(uint32_t)x<<16;r->maps.planes[0].y=(uint32_t)y<<16;
  r->maps.planes[1].x=(uint32_t)(x/2)<<16;r->maps.planes[1].y=(uint32_t)(y/2)<<16;
  w->video.views[1].valid=tick%19!=0;
  assert(m1_runtime_prepare(v));struct State*s=state(v);struct M1MapFrame*f=&s->frame;
  for(unsigned l=0;l<f->layers;l++)for(unsigned yy=0;yy<21;yy++)for(unsigned xx=0;xx<31;xx++){
   unsigned ref=f->refs[l*651+yy*31+xx],expected=m1_stream_find(&s->stream[(s->groups>>l)&1],assets->ids4[ref])|assets->attrs4[ref];
   unsigned row=((f->scroll_y[l]>>3)+yy)&31,col=((f->scroll_x[l]>>3)+xx)&31;
   assert(f->maps[l][row*32+col]==expected);
  }
  for(unsigned l=0;l<f->layers;l++)for(int yy=0;yy<21;yy++)for(int xx=0;xx<31;xx++)assert(v->backgrounds.maps[l][yy*31+xx]==reference(&s->history[s->cache_plane[l]],s->cache_x[l]+xx,s->cache_y[l]+yy));
  for(unsigned g=0;g<2;g++)s->stream[g].upload_count=0;for(unsigned l=0;l<4;l++)f->dirty_rows[l]=0;
 }
 /* Incremental writes, repeated/overlapping patches, image rebinding and
  * simultaneous camera shifts must equal composition from all history. */
 reset(v,mission-1);for(unsigned p=0;p<4;p++){r->maps.planes[p].image=p?65535:assets->first_image+(mission==1?3:0);w->video.views[p].valid=!p;}
 unsigned incremental=0;
 for(unsigned tick=0;tick<600;tick++){
  unsigned id=(tick*37)%patch_count;const struct M1Patch*p=&assets->patches[id];
  if(tick%150==0)r->maps.planes[0].image=assets->first_image+(mission==1?(const unsigned[]){3,7,12,3}[tick/150]:(const unsigned[]){0,4,0,4}[tick/150]);
  const struct M1Image*m=&assets->images[r->maps.planes[0].image-assets->first_image];
  int x=(p->x*8-m->origin_x-24)*4/3,y=(p->y*8-m->origin_y-48)*4/3;
  r->maps.planes[0].x=(uint32_t)x<<16;r->maps.planes[0].y=(uint32_t)y<<16;
  assert(m1_runtime_prepare(v));struct State*s=state(v);
  for(unsigned g=0;g<2;g++)s->stream[g].upload_count=0;
  if(tick%3==0){assert(add_patch(v,id,0));assert(add_patch(v,id,0));}
  else assert(add_patch(v,(id+1)%patch_count,0));
  incremental+=!!s->patched;
  r->maps.planes[0].x+=(uint32_t)((int)(tick%3)-1)*8*65536u;
  assert(m1_runtime_prepare(v));struct M1MapFrame*f=&s->frame;
  for(unsigned l=0;l<f->layers;l++)for(int yy=0;yy<21;yy++)for(int xx=0;xx<31;xx++){
   unsigned ref=reference(&s->history[s->cache_plane[l]],s->cache_x[l]+xx,s->cache_y[l]+yy);
   assert(v->backgrounds.maps[l][yy*31+xx]==ref);checked++;
   unsigned row=((f->scroll_y[l]>>3)+yy)&31,col=((f->scroll_x[l]>>3)+xx)&31;
   assert(f->maps[l][row*32+col]==(m1_stream_find(&s->stream[(s->groups>>l)&1],assets->ids4[ref])|assets->attrs4[ref]));
  }
  for(unsigned g=0;g<2;g++)s->stream[g].upload_count=0;for(unsigned l=0;l<4;l++)f->dirty_rows[l]=0;
 }
 assert(incremental>0);printf("incremental patch cache updated in %u trials\n",incremental);
 if(mission==2){
  reset(v,1);r->maps.planes[0].image=19;w->video.views[0].valid=1;
  for(unsigned p=1;p<4;p++)w->video.views[p].valid=0;
  state(v)->history[0]=(struct History){.image=4,.count=1,.patches={0}};
  r->maps.planes[0].image=SCENE_M5_RELINK_IMAGE;
  for(unsigned tick=0;tick<1000;tick++){
   w->enemies.m5_scene.relink_generation=1+tick/54;
   r->maps.planes[0].x=(uint32_t)(-328+(int)(tick*6%320))<<16;r->maps.planes[0].y=16u<<16;
   assert(m1_runtime_prepare(v));struct State*s=state(v);
   assert(s->frame.layers==1&&s->history[0].image==6&&s->history[0].count==0);
   for(int yy=0;yy<21;yy++)for(int xx=0;xx<31;xx++){
    assert(v->backgrounds.maps[0][yy*31+xx]==reference(&s->history[0],s->cache_x[0]+xx,s->cache_y[0]+yy));checked++;
   }
   for(unsigned g=0;g<2;g++)s->stream[g].upload_count=0;for(unsigned l=0;l<4;l++)s->frame.dirty_rows[l]=0;
  }
  puts("M2 separate arena image retained through 1000 wrap frames");
 }
 free(r);free(w);
 free(v);printf("M%u viewport/patch differential: %u tile checks passed\n",mission,checked);
}

int main(void){run(1);run(2);}
