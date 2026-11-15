#!/usr/bin/env python3
"""Compile all immutable mission events, including regional and auxiliary streams.

This is a game-command parser, not an instruction translator. No input trace is
used. Unknown commands, out-of-bounds children and overlapping records fail.
"""
import argparse, collections, hashlib, json, re, struct
from pathlib import Path
from import_source import COMMIT, PROM_SHA

R = Path(__file__).resolve().parents[1]
BASE, END = 0xE8524, 0xF2FFC
# The only non-event island is the M4 camera-floor table. Keep the original
# source offsets, but never mark those words as executable mission commands.
CAMERA_TABLE = (0xEC882, 0xEC898)

def compile_mission(source):
    rom = (source/'build/mslug_prom.bin').read_bytes()
    assert hashlib.sha256(rom).hexdigest() == PROM_SHA
    raw = rom[BASE:END] + rom[0x44262:0x44266]
    nodes, streams = {}, []
    def parse(at, depth=0):
        assert depth < 16 and BASE <= at < END and at % 2 == 0, hex(at)
        op = rom[at]
        assert op <= 12, (hex(at), op)
        children = []
        if op == 0: nxt = at+18
        elif op == 1:
            children.append(at+10); nxt = parse(at+10, depth+1)
        elif op in (2,3):
            nxt = at+4
            while rom[nxt] != (13 if op == 2 else 14):
                assert rom[nxt] != 12, hex(nxt)
                children.append(nxt); nxt = parse(nxt, depth+1)
            nxt += 2
        elif op in (4,10,11):
            nxt = at+6
            for _ in range(3 if op == 11 else 2):
                children.append(nxt); nxt = parse(nxt, depth+1)
        else: nxt = at+4
        assert nxt <= END
        n = dict(address=f'{at:06X}', opcode=op, next=f'{nxt:06X}',
                 threshold=struct.unpack_from('>H',rom,at+2)[0],
                 children=[f'{x:06X}' for x in children])
        if op == 0:
            template,x,y = struct.unpack_from('>Hhh',rom,at+4)
            initializer = struct.unpack_from('>I',rom,0xE8000+template*4)[0]
            n.update(template=template, initializer=f'{initializer:06X}',
                     flags=rom[at+1], x=x, y=y, params=rom[at+10:at+18].hex())
        if at in nodes: assert nodes[at] == n
        nodes[at] = n
        return nxt
    at = BASE
    while at < END:
        if at == CAMERA_TABLE[0]:
            at = CAMERA_TABLE[1]
            continue
        start = at
        while rom[at] != 12: at = parse(at)
        at = parse(at)
        streams.append(dict(address=f'{start:06X}', end=f'{at:06X}'))
    assert at == END
    labels = {}
    for f in (source/'asm').rglob('*.s'):
        for name,addr in re.findall(r'^([\w]+_([0-9a-fA-F]{5,6})):',f.read_text(),re.M):
            labels.setdefault(addr.upper().zfill(6),dict(symbol=name,file=str(f.relative_to(source))))
    templates = {}
    for n in nodes.values():
        if n['opcode'] != 0: continue
        t = templates.setdefault(n['template'],dict(id=n['template'],initializer=n['initializer'],
                    records=[],**labels.get(n['initializer'],{})))
        t['records'].append(n['address'])
    # Each entry is an offset in this immutable data block. 0xffff is invalid.
    skip = [65535]*(len(raw)//2)
    for addr,n in nodes.items(): skip[(addr-BASE)//2] = int(n['next'],16)-BASE
    skip[(END-BASE)//2] = END-BASE
    g=R/'generated';g.mkdir(exist_ok=True)
    (g/'mission.bin').write_bytes(raw)
    (g/'mission.skip').write_bytes(struct.pack('<'+'H'*len(skip),*skip))
    scene_streams = list(struct.unpack_from('>14I',rom,0x44266))
    assert [i for i,a in enumerate(scene_streams) if a == 0x44262] == [10,13]
    assert all(a == 0x44262 or a in nodes for a in scene_streams)
    assert 0xF260E in nodes
    (g/'mission_data.h').write_text('#ifndef MISSION_DATA_H\n#define MISSION_DATA_H\n#include <stdint.h>\n'
        f'#define MISSION_SOURCE_BASE 0x{BASE:x}u\n#define MISSION_DATA_SIZE {len(raw)}u\n'
        f'#define MISSION_EMPTY_OFFSET {END-BASE}u\n#define MISSION_EMPTY_SOURCE 0x44262u\n'
        '#define MISSION_SCENE_COUNT 14u\n#define MISSION_REGION_FINAL 0xf260eu\n'
        'static const uint32_t mission_scene_sources[MISSION_SCENE_COUNT] = {'+
        ','.join(f'0x{a:x}u' for a in scene_streams)+'};\n'
        'extern const uint8_t mission_data[];\nextern const uint16_t mission_next[];\n#endif\n')
    manifest=dict(commit=COMMIT,prom_sha256=PROM_SHA,source_range=[f'{BASE:06X}',f'{END:06X}'],
                  streams=streams,scene_sources=[f'{a:06X}' for a in scene_streams],
                  regional_final='0F260E',non_event_ranges=[[f'{a:06X}' for a in CAMERA_TABLE]],
                  opcodes=dict(sorted(collections.Counter(n['opcode'] for n in nodes.values()).items())),
                  templates=[templates[k] for k in sorted(templates)],nodes=[nodes[k] for k in sorted(nodes)])
    (g/'mission-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(dict(bytes=len(raw),streams=len(streams),nodes=len(nodes),templates=len(templates))))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True)
    compile_mission(p.parse_args().source)
