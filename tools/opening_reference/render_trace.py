import json,sys
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
sys.path.insert(0,str(Path('upstream/assets/tools').resolve()))
import sprites as S
p=S.PRom();tiles=S.Tiles();trace=json.load(open('opening-work/trace.json'));cache={}
def rawmap(mp,flags):
 key=mp,flags
 if key not in cache:
  parts=[(mp,flags|8,0,0)];l,box=S.compose(p,tiles,parts);cache[key]=S.draw(tiles,l,box,box),box
 return cache[key]
def render(f):
 im=Image.new('RGBA',(320,224),(0,0,0,255))
 for d in sorted(f['draws'],key=lambda d:d['priority']):
  idx,box=rawmap(d['map'],d['flags']);pa=np.array(f['palettes'][str(d['slot'])],dtype=np.uint8)*255//31
  # avoid uint8 overflow in palette expansion
  pa=np.array(f['palettes'][str(d['slot'])],dtype=np.uint16)*255//31
  tile=S.rgba(idx,pa.astype(np.uint8));sx=(d['sx']+1)/256;sy=(d['sy']+1)/256
  if sx!=1 or sy!=1:tile=tile.resize((max(1,round(tile.width*sx)),max(1,round(tile.height*sy))),Image.Resampling.NEAREST)
  x=d['x'];y=d['y'];x=x-65536 if x>32767 else x;y=y-65536 if y>32767 else y
  im.alpha_composite(tile,(x+round(box[0]*sx),512-y-16+round(box[1]*sy)))
 return im.convert('RGB')
if __name__=='__main__':
 nums=[15,40,60,90,115,145,175,190,205,220,250,300]
 sheet=Image.new('RGB',(960,224*4));d=ImageDraw.Draw(sheet)
 for i,n in enumerate(nums):
  im=render(trace['frames'][n]);im.save(f'opening-work/ref-{n}.png');sheet.paste(im,((i%3)*320,(i//3)*224));d.text(((i%3)*320,(i//3)*224),str(n),fill='red')
 sheet.save('/tmp/trace-sheet.png')
