"""GBA-native HUD lettering, inspired by Advance's clear label separation.

New authored indexed pixels; no Advance or prototype ROM assets are imported.
Preserve source FIX addresses, timer cadence, palettes and gameplay layout.
"""
import json
from pathlib import Path
import numpy as np
from compile_m1_native_maps import body, numbers

R = Path(__file__).resolve().parents[1]
cells = {}


def emit(a, base):
    h, w = a.shape
    for y in range(h // 6):
        for x in range(w // 6):
            cells[str(base+x+y*16)] = [''.join(format(int(c), 'x') for c in row)
                                      for row in a[y*6:y*6+6, x*6:x*6+6]]


digits = [
 '0111110/1100011/1000001/1000001/1000001/1000001/1000001/1100011/0111110',
 '0001000/0011000/0101000/0001000/0001000/0001000/0001000/0001000/0111110',
 '0111110/1000001/0000001/0000001/0001110/0010000/0100000/1000000/1111111',
 '1111110/0000001/0000001/0000001/0011110/0000001/0000001/0000001/1111110',
 '0000110/0001010/0010010/0100010/1000010/1111111/0000010/0000010/0000010',
 '1111111/1000000/1000000/1000000/1111110/0000001/0000001/1000001/0111110',
 '0011110/0100000/1000000/1000000/1111110/1000001/1000001/1000001/0111110',
 '1111111/0000001/0000010/0000100/0001000/0010000/0010000/0010000/0010000',
 '0111110/1000001/1000001/1000001/0111110/1000001/1000001/1000001/0111110',
 '0111110/1000001/1000001/1000001/0111111/0000001/0000001/0000010/0111100',
]
for rows, code in zip(digits, numbers(body((R/'generated/session-data.c').read_text(), 'session_time_digits')), strict=True):
    mask = np.array([[c == '1' for c in row] for row in rows.split('/')])
    a = np.zeros((12, 12), np.uint8)
    for y, x in np.argwhere(mask):
        for dy, dx in [(0, 0), (-1, 0), (1, 0), (0, -1), (0, 1)]:
            a[y+1+dy, x+2+dx] = 1
    for y, x in np.argwhere(mask):
        a[y+1, x+2] = 8 if y < 3 else 5 if y < 6 else 2
    emit(a, code & 4095)

letters = {'A':'010/101/111/101/101', 'R':'110/101/110/101/101',
           'M':'10001/11011/10101/10001/10001', 'S':'011/100/010/001/110',
           'B':'110/101/110/101/110', 'O':'010/101/101/101/010'}
for word, base, width in [('ARMS', 0xab1, 18), ('BOMB', 0xae4, 24)]:
    a = np.ones((6, width), np.uint8)
    text_width = sum(len(letters[c].split('/')[0]) for c in word)+len(word)-1
    x = (width-text_width)//2
    for ch in word:
        rows = letters[ch].split('/')
        for y, row in enumerate(rows):
            for xx, value in enumerate(row):
                if value == '1': a[y, x+xx] = 15
        x += len(rows[0])+1
    emit(a, base)

out = R/'art/hud152/fixed.json'
out.parent.mkdir(exist_ok=True)
out.write_text(json.dumps({'fixed_cells':cells, 'scope':'Timer 0-9, ARMS, BOMB',
                          'timer_face':[7,9], 'timer_cell':[12,12], 'timer_advance':12,
                          'source':'New authored pixels; Advance screenshot reference only'}, indent=2)+'\n')
print(f'{len(cells)} HUD FIX cells')
