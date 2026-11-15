#!/usr/bin/env python3
"""Extract static evidence for three unresolved source inputs; execute no game code."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = '1b8eee0891b6f8c837348e60ea2923ae229e0fb603d34ae62ae06131332f0d3d'
RANGES = [
    (0x656, 0x6f0), (0x5dc80, 0x5dc9c), (0x5e9b6, 0x5e9e4),
    (0x630bc, 0x6315a), (0x5dd56, 0x5de18), (0x54d98, 0x54dc2),
    (0x403e4, 0x405b2), (0x41ce0, 0x41d6c),
    (0x45de4, 0x45f3a), (0x44f4a, 0x44f8a), (0x28c20, 0x28cb8),
    (0x28d70, 0x28dd4), (0x28f3e, 0x2902c), (0x5a9d6, 0x5aab6),
    (0x4ae, 0x518), (0x518, 0x546), (0x13600, 0x13624),
    (0x62a98, 0x62aa8), (0x62e44, 0x62e88), (0x63524, 0x635d8),
    (0x77fd6, 0x77ffa), (0x780de, 0x78130), (0x44048, 0x4405e),
    (0x28dd4, 0x28df2), (0x44df8, 0x44f4a), (0x440e4, 0x44182),
]

def linear_animation(ledger, root):
    """Sum authored draw delays; no actors, renderer, CPU or game loop run."""
    if ledger['prom_sha256'] != EXPECTED:
        raise ValueError('Animation ledger revision mismatch')
    commands = {c['native_index']: c for c in ledger['commands']}
    current = next(c for c in commands.values() if c['source'] == root)
    seen, result, elapsed, delay = set(), [], 0, 0
    while current['native_index'] not in seen:
        seen.add(current['native_index'])
        row = dict(current, authored_draw_ticks_before=elapsed)
        result.append(row)
        op = current['op']
        if op == 'WAIT':
            if not delay:
                delay = current['argument']
        elif op == 'DRAW':
            if delay <= 0:
                raise ValueError('Indefinite drawing cannot give a finite timeline')
            row['draw_ticks'] = delay
            elapsed += delay
            delay = 0
        elif op == 'END':
            return dict(root=f'{root:06X}', draw_ticks=elapsed, commands=result)
        elif op not in ('CALL', 'SOUND', 'DEFENSE', 'ATTACK'):
            raise ValueError('Nonlinear or unreviewed animation in timing evidence')
        current = commands[current['next']]
    raise ValueError('Loop cannot give a linear timeline')

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--prom', required=True, type=Path)
    args = ap.parse_args()
    data = args.prom.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED:
        raise SystemExit('PROM does not match the documented source revision')
    cpu = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)
    def offset(address):
        return address - 0x100000 if address >= 0x200000 else address
    snippets = []
    for lo, hi in RANGES:
        raw = data[offset(lo):offset(hi)]
        decoded = list(cpu.disasm(raw, lo))
        if not decoded or decoded[-1].address + decoded[-1].size != hi:
            raise SystemExit(f'Incomplete static disassembly at {lo:06X}..{hi:06X}')
        snippets.append(dict(start=f'{lo:06X}', end_exclusive=f'{hi:06X}',
                             bytes=raw.hex(), instructions=[
            f'{i.address:06X} {i.bytes.hex()} {i.mnemonic} {i.op_str}' for i in decoded]))
    tables = {}
    for address, count in [(0x45dd4, 8), (0x44d30, 8)]:
        tables[f'{address:06X}'] = list(struct.unpack_from('>'+str(count)+'h', data, address))
    tables['2C4A44'] = [f'{v:06X}' for v in struct.unpack_from('>4I', data, offset(0x2c4a44))]
    for address in (0x5dd4c, 0x2c49c4, 0x2df210):
        tables[f'{address:06X}'] = list(struct.unpack_from('>4h', data, offset(address)))
    targets = (0x6293e, 0x45de4, 0x403e4, 0x63106, 0x4052c, 0x40546, 0x28c20,
               0x44df8, 0x44e12, 0x44e2c)
    refs = {target: dict(long_literals=[], pc_relative_candidates=[]) for target in targets}
    # Candidates are not proof of a call. Tables and animation bytes can match.
    for pos in range(0, len(data)-4, 2):
        address = pos if pos < 0x100000 else pos+0x100000
        value = struct.unpack_from('>I', data, pos)[0]
        if value in refs:
            refs[value]['long_literals'].append(f'{address:06X}')
        op = struct.unpack_from('>H', data, pos)[0]
        if op & 0xf1ff == 0x41fa or op in (0x4eba, 0x4efa, 0x6000, 0x6100):
            target = address+2+struct.unpack_from('>h', data, pos+2)[0]
            if target in refs:
                refs[target]['pc_relative_candidates'].append(f'{address:06X}')
    mission = json.loads((ROOT/'generated/mission-source.json').read_text())
    placements = json.loads((ROOT/'generated/scene-placement-source.json').read_text())
    authored = {str(n): next((x['records'] for x in mission['templates'] if x['id']==n), [])
                for n in (59,120,121,318)}
    emitter = json.loads((ROOT/'generated/emitter59-source.json').read_text())
    boss = json.loads((ROOT/'generated/boss-source.json').read_text())
    animations = dict(landed=linear_animation(emitter, 0x2c51ca),
                      released=linear_animation(emitter, 0x2e2286),
                      smoke=linear_animation(boss, 0x2dd37e))
    calls = [r for r in animations['landed']['commands'] if r['op']=='CALL']
    release_ticks = [r['authored_draw_ticks_before'] for r in calls if r['target']==7]
    explosion_tick = next(r['authored_draw_ticks_before'] for r in calls if r['target']==1)
    release_life = animations['released']['draw_ticks']
    scene = json.loads((ROOT/'generated/scene-program-source.json').read_text())
    bounds = next(c for c in scene['commands'] if c['source']==0x93bc2)
    raw_bounds = list(struct.unpack_from('>6H', data, 0x93bc2))
    if raw_bounds != [15, *bounds['values'][:5]]:
        raise ValueError('Scene limits do not match PROM')
    event120 = next(n for n in mission['nodes'] if n['address']=='0F05FC')
    raw_data = [dict(start=f'{lo:06X}', end_exclusive=f'{hi:06X}',
                     bytes=data[offset(lo):offset(hi)].hex()) for lo,hi in
                [(0x2c51ca,0x2c5236),(0x2e2286,0x2e22e8),(0x2dd37e,0x2dd4ba),
                 (0x93bc2,0x93bd0),(0x916f0,0x916f8),(0xf05fc,0xf060e)]]
    report = dict(source_version='0.5.89', date='2026-10-31', prom_sha256=digest,
                  method='Static byte, disassembly and authored-data inspection only; no execution',
                  resolved_count=0, game_code_changed=False, tables=tables,
                  references={f'{k:06X}':v for k,v in refs.items()},
                  false_positive={'2DB804':'00040546 spans animation Y=4 and a following command; not a handler pointer'},
                  direct_event_records=authored,
                  indirect_emitter_placements=[x for x in placements['records']
                      if x['family']=='URBAN' and x['entry'] in
                      ('MULTISTAGE0','MULTISTAGE1','MULTISTAGE2','MULTISTAGE3')],
                  animation_timing=animations,
                  conditional_emitter_order=dict(
                      assumptions=['Successful allocations; no early culling/deletion',
                                   'No animation freeze or scheduler timer reset',
                                   'Final smoke survives with no intervening descendants'],
                      release_attempt_ticks=release_ticks,
                      explosion_tick=explosion_tick,
                      selection_ticks=[t+release_life for t in release_ticks[:3]],
                      two_releases=dict(predecessors=['release2','landed parent'],
                                        d1=[321,352], selectors=[1,0]),
                      three_releases=dict(predecessors=['release2','release3','77FD6 smoke'],
                                          d1=[321,321,360], selectors=[1,1,0]),
                      universal_fix=False),
                  boss120_scene=dict(scene_index=5, descriptor=scene['descriptors'][5],
                      mission_stream=mission['scene_sources'][5], event=event120,
                      final_limits=bounds, raw_limit_words=raw_bounds,
                      horizontal_backscroll_span=bounds['values'][2]-bounds['values'][0],
                      conclusion='Bounds permit backscroll; X<=272 is not a proved invariant. '
                                 'This is not a demonstrated gameplay route.'),
                  recovery318_callers=dict(known_point_helper_call_sites=['044F78','045F1A'],
                      both_use_a2_a0=True, known_correct_call_site=None,
                      unreferenced_sibling_entries=['044DF8','044E12','044E2C'],
                      caveat='Candidate reference search cannot rule out computed indirect calls'),
                  raw_data=raw_data, snippets=snippets)
    output = ROOT/'generated/exception-input-research.json'
    output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(output=str(output), snippets=len(snippets),
                         release_attempt_ticks=release_ticks, explosion_tick=explosion_tick,
                         selection_ticks=report['conditional_emitter_order']['selection_ticks'],
                         backscroll_span=report['boss120_scene']['horizontal_backscroll_span'],
                         resolved_count=0)))

if __name__ == '__main__':
    main()
