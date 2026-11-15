"""Summarize read-only final-ROM mGBA measurements; does not rebuild the ROM."""
import csv, hashlib, io, json, statistics, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/diagnosis-099/evidence'
OUT.mkdir(parents=True, exist_ok=True)
archive = zipfile.ZipFile(ROOT / 'build/diagnosis-099/complete-measurement.zip')
archive.extractall(OUT)

def rows(name):
    result = list(csv.DictReader(io.StringIO(archive.read(name + '.csv').decode())))
    assert all(None not in r and None not in r.values() for r in result)
    return result

frames, calls, dma, blanks, groups, symbols = map(rows, ['frames', 'calls', 'dma', 'blank', 'groups', 'symbols'])
baseline = {r['lcd_frame']: r for r in map(json.loads, (ROOT / 'docs/play-validation-0.5.99/combat/trace.jsonl').open())}
compared = 0
for r in frames:
    old = baseline.get(int(r['lcd']))
    if old:
        compared += 1
        for a, b in [('tick', 'world.ticks'), ('camera', 'world.weapons.camera_x'), ('commits', 'video.commits'), ('late', 'video.late_transfers')]:
            assert int(r[a]) == old[b], (r['lcd'], a, r[a], old[b])

# The initial recorder read this diagnostic byte as 16 bits, including the
# adjacent half_rate byte. Correct a separate copy; retain raw evidence.
# This field is NOT used by any performance/white-frame calculation.
normalized = [dict(r, vblanks=str(int(r['vblanks']) & 255)) for r in frames]
with (OUT / 'frames-normalized.csv').open('w') as f:
    writer = csv.DictWriter(f, fieldnames=normalized[0].keys())
    writer.writeheader(); writer.writerows(normalized)

HZ = 16777216
VB = 1232 * 68
result = dict(version='0.5.99', date='2026-11-02', emulator='mGBA 0.10.2 / HLE BIOS',
              input_only=True, game_memory_writes=False, original_rom_modified=False,
              rom_sha256=hashlib.sha256((ROOT / 'dist/metal-slug-gba-native-v0.5.99.gba').read_bytes()).hexdigest(),
              elf_sha256=hashlib.sha256((ROOT / 'build/native/native.elf').read_bytes()).hexdigest(),
              timing_hz=HZ, vblank_cycles=VB, vblank_ms=VB / HZ * 1000,
              matched_baseline_samples=compared, baseline_mismatches=0, windows=[])
for w, (start, end, name) in enumerate([(600, 1200, '미션1 카메라 정지'), (3000, 3600, '미션1 스크롤'), (15000, 15600, '헬기 구간 카메라 정지')]):
    wr = lambda data: [r for r in data if r['window'] == str(w)]
    first = next(r for r in frames if int(r['lcd']) == start - 1)
    ff = [r for r in frames if start <= int(r['lcd']) < end]
    last = ff[-1]
    cycles = sum(int(r['cycles']) for r in wr(groups))
    assert abs(cycles - (int(last['cycle']) - int(first['cycle']))) <= 8
    ticks = int(last['tick']) - int(first['tick'])
    ww = dict(name=name, start_lcd=start, end_lcd_exclusive=end, seconds=cycles/HZ,
              start_tick=int(first['tick']), end_tick=int(last['tick']), tick_delta=ticks,
              simulation_fps=ticks*HZ/cycles, white_lcd_frames=sum(int(r['white']) for r in ff),
              late_transfers=int(last['late'])-int(first['late']), cycles=cycles,
              groups={r['group']: dict(cycles=int(r['cycles']), percent=int(r['cycles'])/cycles*100) for r in wr(groups)},
              calls={}, top_symbols=[], transfers=[])
    for name in ['native_video_prepare', 'native_background_prepare', 'native_fixed_objects', 'native_object_scene_stage', 'native_session_frame']:
        cc = [r for r in wr(calls) if r['function'] == name]
        ww['calls'][name] = dict(completed_calls=len(cc), mean_wall_ms=statistics.mean(int(r['wall_cycles']) for r in cc)/HZ*1000,
              mean_non_interrupt_ms=statistics.mean(int(r['non_interrupt_cycles']) for r in cc)/HZ*1000,
              failed_calls=sum(int(r['result']) == 0 for r in cc))
    for r in sorted(wr(symbols), key=lambda r: -int(r['cycles']))[:20]:
        ww['top_symbols'].append(dict(function=r['function'], address=r['address'], percent=int(r['cycles'])/cycles*100, calls=int(r['entries'])))
    for b in wr(blanks):
        if b['forced_blank'] != '1': continue
        t = int(b['cycle'])
        dd = [r for r in wr(dma) if t <= int(r['start_cycle']) < t+200000]
        irq = next(r for r in wr(calls) if r['function'] == 'native_video_irq_vblank' and int(r['start_cycle']) <= t < int(r['start_cycle'])+int(r['wall_cycles']))
        clear = next(r for r in wr(blanks) if r['forced_blank'] == '0' and int(r['cycle']) > t)
        frame = next(r for r in ff if r['lcd'] == b['lcd'])
        transfer = dict(lcd=int(b['lcd']), tick=int(b['tick']), start_cycle=t,
                        bytes=sum(int(r['bytes']) for r in dd), bg_tile_bytes=sum(int(r['bytes']) for r in dd if r['pc']=='080b575c'),
                        irq_ms=int(irq['wall_cycles'])/HZ*1000, last_dma_end_line=int(dd[-1]['end_line']),
                        forced_blank_ms=(int(clear['cycle'])-t)/HZ*1000, white=int(frame['white']))
        ww['transfers'].append(transfer)
    assert sum(t['white'] for t in ww['transfers']) == ww['white_lcd_frames'] == ww['late_transfers']
    ww['max_transfer_ms'] = max(r['irq_ms'] for r in ww['transfers'])
    ww['mean_transfer_ms'] = statistics.mean(r['irq_ms'] for r in ww['transfers'])
    ww['dma_cycle_percent'] = sum(int(r['cycles']) for r in wr(dma))/cycles*100
    ww['palette_render_and_fixed_step_percent'] = sum(int(r['cycles']) for r in wr(symbols) if r['function'] in ['render', 'fixed_step'])/cycles*100
    result['windows'].append(ww)
scroll = result['windows'][1]
assert len([t for t in scroll['transfers'] if t['bg_tile_bytes'] > 30000]) == 15
assert all(t['white'] == (t['irq_ms'] > result['vblank_ms']) for t in scroll['transfers'])
result['limitations'] = ['Emulator timings, not physical GBA measurements.',
 'Three 600-LCD windows of the same M1 input replay; not full-campaign worst cases.',
 'Calls crossing the measurement boundary are excluded from per-call means; cycle groups include the complete windows.',
 'Native session CPU time includes palette/render housekeeping, not only gameplay AI/physics.',
 'Raw frames.csv vblanks included the adjacent half_rate byte; frames-normalized.csv masks to the actual byte width. This field is unused in all reported conclusions.']
(OUT / 'summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
print(json.dumps({k:result[k] for k in ['rom_sha256', 'matched_baseline_samples', 'baseline_mismatches']}, indent=2))
print([(w['name'],round(w['simulation_fps'],3),w['white_lcd_frames'],round(w['max_transfer_ms'],3)) for w in result['windows']])
