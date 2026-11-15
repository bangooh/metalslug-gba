"""Exact offline nibble-usage metadata; pixel bytes are never modified."""
import hashlib,json,re
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];G=R/'generated'
def generate():
 s=(G/'scaled-drawing-data.c').read_text();body=re.search(r'const struct NativeScaledChunk native_scaled_chunks\[\]=\{(.*?)\};',s,re.S)[1]
 chunks=[tuple(map(int,row.split(','))) for row in re.findall(r'\{([^{}]+)\}',body)]
 data=(G/'native-scaled-objects.4bpp').read_bytes();cache={}
 def mask(blob):
  return int(np.bitwise_or.reduce(np.array([(1<<(int(v)&15))|(1<<(int(v)>>4)) for v in np.unique(np.frombuffer(blob,dtype=np.uint8))],dtype=np.uint16),initial=0))&65534
 def pair(blob,w,h):return (mask(blob[:w*h//4]),mask(blob[w*h//4:])) if h==64 else (mask(blob),0)
 rows=[]
 for offset,x,y,w,h,shape,size in chunks:
  key=offset,w,h
  if key not in cache:cache[key]=pair(data[offset:offset+w*h//2],w,h)
  rows.append(cache[key])
 approved=(G/'player.tiles').read_bytes();players=[pair(approved[i:i+2048],64,64) for i in range(0,len(approved),2048)]
 bg=np.frombuffer((G/'native-bg-blocks.4bpp').read_bytes(),dtype=np.uint8).reshape(-1,8)
 byte_masks=np.array([(1<<(i&15))|(1<<(i>>4)) for i in range(256)],dtype=np.uint16)
 bg_masks=np.bitwise_or.reduce(byte_masks[bg],axis=1)&65534
 h='#ifndef NATIVE_OBJECT_MASKS_H\n#define NATIVE_OBJECT_MASKS_H\n#include <stdint.h>\n'+f'extern const uint16_t native_chunk_masks[{len(rows)}][2];\nextern const uint16_t native_player_masks[35][2];\nextern const uint16_t native_bg_block_masks[{len(bg_masks)}];\n#endif\n'
 def array(name,values):return f'const uint16_t {name}[][2]={{'+',\n'.join('{%d,%d}'%r for r in values)+'};\n'
 (G/'object-masks.h').write_text(h);(G/'object-masks.c').write_text('#include "object-masks.h"\n'+array('native_chunk_masks',rows)+array('native_player_masks',players)+'const uint16_t native_bg_block_masks[]={'+','.join(map(str,bg_masks))+'};\n')
 sha=lambda name:hashlib.sha256((G/name).read_bytes()).hexdigest()
 report=dict(chunks=len(rows),bg_blocks=len(bg_masks),approved_players=len(players),source_pixels_unchanged=True,scope='Offline exact used-index masks, full texture is union of top/bottom masks',files={n:sha(n) for n in ['scaled-drawing-data.c','native-scaled-objects.4bpp','native-bg-blocks.4bpp','player.tiles','object-masks.c','object-masks.h']})
 (G/'object-masks-source.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='files'}))
if __name__=='__main__':generate()
