"""Native-size score digits and GO frames. Indexed pixels, no antialiasing.

The original FIX frame addresses and durations remain unchanged. Intermediate
GO views retain the original row-wise perspective; the three held poses use
explicit 24px masks with open counters and clean arrow edges.
"""
import json
import re
from pathlib import Path
import numpy as np
from import_scaled_drawings import unpack_tiles, scale

R = Path(__file__).resolve().parents[1]
D = R / 'art/gameplay-text'
DIGITS = {
 '0':['01110','10001','10001','10001','10001','10001','01110'],
 '1':['00100','01100','00100','00100','00100','00100','01110'],
 '2':['01110','10001','00001','00010','00100','01000','11111'],
 '3':['11110','00001','00001','01110','00001','00001','11110'],
 '4':['00010','00110','01010','10010','11111','00010','00010'],
 '5':['11111','10000','10000','11110','00001','00001','11110'],
 '6':['01110','10000','10000','11110','10001','10001','01110'],
 '7':['11111','00001','00010','00100','01000','01000','01000'],
 '8':['01110','10001','10001','01110','10001','10001','01110'],
 '9':['01110','10001','10001','01111','00001','00001','01110'],
}
G=['00111111','01111111','11000000','11000000','11000000','11000000',
   '11001111','11001111','11000011','11000011','11000011','01111111','00111110']
O=['0111110','1111111','1100011','1100011','1100011','1100011','1100011','1100011','1100011','1111111','0111110']
EX=['11','11','11','11','11','11','11','00','00','11','11']

def place(mask, rows, x, y):
    for yy, row in enumerate(rows):
        for xx, v in enumerate(row):
            if v == '1': mask[y+yy, x+xx] = True

def outlined(mask, shades):
    out = np.zeros(mask.shape, np.uint8)
    edge = np.zeros_like(mask)
    for y, x in np.argwhere(mask):
        for dy, dx in [(-1,0),(1,0),(0,-1),(0,1),(0,0)]:
            yy, xx = y+dy, x+dx
            if 0 <= yy < 24 and 0 <= xx < 24: edge[yy,xx] = True
    for y, x in np.argwhere(edge):
        if y+1 < 24 and x+1 < 24: out[y+1,x+1] = 9
    out[edge] = 15
    for y, x in np.argwhere(mask): out[y,x] = shades(x,y)
    return out

def go_pose(kind):
    letters = np.zeros((24,24),bool)
    place(letters,G,1,2 if kind < 2 else 7)
    place(letters,O,11,2 if kind < 2 else 12)
    place(letters,EX,20 if kind < 2 else 18,2 if kind < 2 else 12)
    out=outlined(letters,lambda x,y: 12 if x%12<4 else 11 if x%12<7 else 10)
    arrow=np.zeros((24,24),bool)
    if kind < 2:
        # Five-pixel shaft and a full, symmetric chevron at native size.
        arrow[19:22,2:17]=True
        for y in range(15,23):
            right=22-abs(y-19)
            arrow[y,16:right+1]=True
        if kind == 1:
            tilted=np.zeros_like(arrow)
            for y,x in np.argwhere(arrow):
                yy=y-(x-2)//5
                if 0 <= yy < 24: tilted[yy,x]=True
            arrow=tilted
    else:
        arrow[7:22,22:24]=True
        for y in range(2,11): arrow[y,max(16,23-y):24]=True
    a=outlined(arrow,lambda x,y: 6 if x<7 else 7 if x<15 else 8)
    out[a!=0]=a[a!=0]
    return out

def rows(a): return [''.join(format(int(v),'x') for v in row) for row in a]
def main():
    D.mkdir(parents=True,exist_ok=True)
    source=json.loads((R/'generated/item-source.json').read_text())
    commands=source['commands']
    s=(R/'generated/item-programs.c').read_text()
    entries=list(map(int,re.search(r'item_glyphs\[32\]=\{([^}]+)',s)[1].split(',')))
    objects=[]
    for ch,i in zip('0123456789',entries):
        q=commands[i]
        while q['op']!='DRAW': q=commands[q['next']]
        parts=source['drawings'][q['target']];assert len(parts)==1
        a=np.zeros((12,12),np.uint8)
        # 5x7 face + one-pixel dark lower/right shadow, six-pixel advance.
        # Keep the original map anchor; the taller face grows upward only.
        mask=np.array([[v=='1' for v in row] for row in DIGITS[ch]])
        for y,x in np.argwhere(mask): a[y+5,x+4]=15
        for y,x in np.argwhere(mask): a[y+4,x+3]=2+min(y,6)
        objects.append(dict(character=ch,source=parts[0]['map'],rows=rows(a)))
    fixed=unpack_tiles((R/'generated/native-fixed.4bpp').read_bytes())
    text=(R/'generated/scene-text-data.c').read_text()
    timelines=[]
    for t in re.findall(r'static const struct FixedTimelineFrame timeline\d\[\]=\{([^\n]+)',text):
        timelines.append([list(map(int,r.split(','))) for r in re.findall(r'\{([^{}]+)\}',t) if int(r.split(',')[0])!=65535])
    def source_frame(base):
        return scale(np.block([[fixed[base+y*16+x] for x in range(4)] for y in range(4)]))
    cells={};frames=[]
    for kind,timeline in enumerate(timelines):
        key=[0x840,0x980,0xe40][kind];target=go_pose(kind);original=source_frame(key)
        for n,(tile,w,h,ticks,jump) in enumerate(timeline):
            base=tile&4095
            if base==0x8cc: continue # original transparent hold stays blank
            if kind==1 and base in [v['base'] for v in frames]: continue
            before=source_frame(base)
            if base==key: after=target.copy()
            else:
                after=np.zeros((24,24),np.uint8)
                for y in range(24):
                    a=np.flatnonzero(before[y]);b=np.flatnonzero(original[y]);c=np.flatnonzero(target[y])
                    if not len(a) or not len(b) or not len(c): continue
                    # Apply the source row's horizontal perspective to the
                    # native master, retaining its holes and palette indices.
                    width=a[-1]-a[0]+1;den=b[-1]-b[0]+1
                    left=max(0,round(a[0]+(c[0]-b[0])*width/den))
                    right=min(24,round(a[0]+(c[-1]+1-b[0])*width/den))
                    if right<=left: right=min(24,left+1)
                    ix=np.minimum(c[-1],c[0]+np.arange(right-left)*(c[-1]+1-c[0])//max(1,right-left))
                    after[y,left:right]=target[y,ix]
                if n==7: after[after!=0]=15 # original full-white glint
            frames.append(dict(base=base,ticks=ticks,kind=kind,rows=rows(after)))
            for y in range(4):
                for x in range(4):
                    code=base+y*16+x;value=rows(after[y*6:y*6+6,x*6:x*6+6])
                    assert code not in cells or cells[code]==value
                    cells[code]=value
    data=dict(version=1,scope='GBA-native score popups and GO/arrow; source timing/positions/palettes unchanged',
              objects=objects,fixed_cells={str(k):v for k,v in sorted(cells.items())},go_frames=frames)
    (D/'glyphs.json').write_text(json.dumps(data,indent=2)+'\n')
    print('Native score digits:',len(objects),'GO frames:',len(frames),'FIX cells:',len(cells))

if __name__=='__main__': main()
