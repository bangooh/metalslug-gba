"""Consistent down-right HUD shadows and bold source-style timer faces."""
import json
from pathlib import Path
import numpy as np
from compile_m1_native_maps import body, numbers

R = Path(__file__).resolve().parents[1]
cells = json.loads((R/'art/hud152/fixed.json').read_text())['fixed_cells']
source = json.loads((R/'art/hud153/source.json').read_text())

def pixels(rows):
    return np.array([[int(c,16) for c in row] for row in rows], np.uint8)

def emit(a, base):
    h,w = a.shape
    for y in range(h//6):
        for x in range(w//6):
            cells[str(base+x+y*16)] = [''.join(format(int(c),'x') for c in row) for row in a[y*6:y*6+6,x*6:x*6+6]]

def shadow(a):
    # Remove old outlines/backgrounds, retaining every foreground pixel.
    face = a.copy(); face[face==1] = 0
    out = np.zeros_like(face)
    out[1:,1:][face[:-1,:-1]!=0] = 1
    out[face!=0] = face[face!=0]
    return out

for code,rows in source.items():
    if int(code) not in range(0x3f0,0x3f3): emit(shadow(pixels(rows)),int(code))
# Shadow the complete infinity glyph across cell boundaries, not each tile.
emit(shadow(np.hstack([pixels(source[str(c)]) for c in range(0x3f0,0x3f3)])),0x3f0)
for base,width in [(0xab1,3),(0xae4,4)]:
    emit(shadow(np.hstack([pixels(cells[str(base+i)]) for i in range(width)])),base)

digits = ['01110/10001/10001/10001/10001/10001/01110',
 '00100/01100/00100/00100/00100/00100/01110',
 '01110/10001/00001/00110/01100/11000/11111',
 '11110/00001/00001/01110/00001/00001/11110',
 '00010/00110/01010/10010/11111/00010/00010',
 '11111/10000/10000/11110/00001/00001/11110',
 '01110/10000/10000/11110/10001/10001/01110',
 '11111/00001/00010/00100/01000/01000/01000',
 '01110/10001/10001/01110/10001/10001/01110',
 '01110/10001/10001/01111/00001/00001/01110']
for digit,base in zip(digits,numbers(body((R/'generated/session-data.c').read_text(),'session_time_digits')),strict=True):
    m=np.array([[c=='1' for c in row] for row in digit.split('/')])
    m=np.repeat(np.repeat(m,[1,2,1,2,1,2,1],axis=0),[2,2,1,2,2],axis=1)
    a=np.zeros((12,12),np.uint8)
    for y,x in np.argwhere(m):
        for dy,dx in [(0,0),(-1,0),(1,0),(0,-1),(0,1)]: a[y+1+dy,x+1+dx]=1
    for y,x in np.argwhere(m):a[y+1,x+1]=8 if y<3 else 5 if y<7 else 2
    emit(a,base&4095)
(R/'art/hud153/fixed.json').write_text(json.dumps({'fixed_cells':cells,'shadow_offset':[1,1],
    'timer_face':[9,10],'timer_cell':[12,12],'scope':'HUD labels, score/ammo/lives digits, UP and infinity; bold timer'},indent=2)+'\n')
print(len(cells),'HUD cells')
