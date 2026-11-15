#!/usr/bin/env python3
"""Preserve the completed R evidence chain; refuses to package an incomplete run.

This is a project-specific reproducible report packager, not a gameplay test.
All state-lineage assertions come from the recorded same-ROM controller run.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ROM_SHA = 'd15f3e88b721417e3cf5fa8b24b0ec71477468f3fa77695fe2118765b3e8a11a'
SEGMENTS = [('route', 13931), ('targets', 60000), ('priority', 0)]
CAMERAS = [(3, 3, 1264, 16), (4, 2, 3744, 8), (5, 4, 2848, 0), (6, 5, 1649, 640)]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text())


def trace(path):
    return [json.loads(line) for line in path.read_text().splitlines() if line.startswith('{')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', type=Path, default=ROOT / 'build/campaign134/r')
    parser.add_argument('--reference-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/campaign-0.5.134/evidence-r')
    args = parser.parse_args()
    base, reference, out = args.run_dir.resolve(), args.reference_dir.resolve(), args.output.resolve()
    if sha(base / 'rom.gba') != ROM_SHA:
        raise SystemExit('R ROM hash does not match frozen candidate; refusing mixed evidence')
    execution_path = base / 'priority/execution.json'
    if not execution_path.exists():
        raise SystemExit('R priority execution is still incomplete; no final report written')
    execution = read(execution_path)
    if execution['exit_code']:
        raise SystemExit(f'R priority failed with exit {execution["exit_code"]}; no completion claim')
    traces = {name: trace(base / name / 'telemetry.jsonl') for name, _ in SEGMENTS}
    selected = [row for name, end in SEGMENTS for row in traces[name] if not end or row['lcd_frame'] < end]
    mission_results = {row['world.weapons.scene_number'] for row in selected if row['session.state'] == 2}
    if mission_results != set(range(6)) or traces['priority'][-1]['session.state'] != 10:
        raise SystemExit('Missing all-six result sessions or final title return; no completion claim')
    if any(row['app.error'] for row in selected):
        raise SystemExit('Selected chain contains application error; review before packaging')
    # Do not publish the user report until all intended hardware checks are available.
    for mission, *_ in CAMERAS:
        hw = base / f'hardware-m{mission}/m{mission}-hardware.json'
        if not hw.exists():
            raise SystemExit(f'Missing coherent R hardware evidence {hw}')
        data = read(hw)
        if data['map_mismatched_bytes'] or data['tile_mismatched_bytes']:
            raise SystemExit(f'R hardware transfer mismatch in M{mission}')
    out.mkdir(parents=True, exist_ok=True)
    manifest = {'candidate': 'R', 'rom_sha256': ROM_SHA, 'files': [], 'chain': [],
                'button_only_campaign_completed': True, 'reference_gate_passed': False,
                'lineage_scope': 'Same frozen ROM, recorded host-only controller revisions; no game RAM injection'}

    def copy(src, name, compressed=False):
        dst = out / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        data = src.read_bytes()
        dst.write_bytes(gzip.compress(data, 9, mtime=0) if compressed else data)
        manifest['files'].append({'file': name, 'source': str(src), 'sha256': sha(dst),
                                  'bytes': dst.stat().st_size,
                                  'uncompressed_sha256': hashlib.sha256(data).hexdigest() if compressed else None})
        return dst

    def png(src, name):
        dst = out / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        Image.open(src).convert('RGB').save(dst)
        manifest['files'].append({'file': name, 'source': str(src), 'source_sha256': sha(src),
                                  'sha256': sha(dst), 'bytes': dst.stat().st_size})
        return dst

    perf = out / 'performance-r.json'
    subprocess.run([sys.executable, str(ROOT / 'tools/measure_campaign134.py'),
                    *[f'{base / name}:{base / name / "telemetry.jsonl"}:{end}' for name, end in SEGMENTS],
                    '--rom', str(base / 'rom.gba'), '--provenance', 'button-only', '--output', str(perf)], check=True)
    measurement = read(perf)
    native_build = read(ROOT / 'generated/native-build.json')
    if native_build['rom_sha256'] != ROM_SHA:
        raise SystemExit('Generated build metadata no longer identifies R; preserve matching metadata first')
    copy(ROOT / 'generated/native-build.json', 'native/native-build.json')
    for name in ['probe.txt', 'symbols.txt', 'offsets.c', 'play', 'play-targets', 'play-priority']:
        compressed = name.startswith('play')
        copy(base / name, 'native/' + name + ('.gz' if compressed else ''), compressed)
    runs = []
    for index, (name, end) in enumerate(SEGMENTS):
        for filename in ['telemetry.jsonl', 'commits.csv', 'inputs.csv', 'campaign.csv', 'stderr.log']:
            src = base / name / filename
            if src.exists():
                copy(src, f'native/{name}/{filename}.gz', True)
        record = {'label': {'route': '시작→M2 진입', 'targets': 'M2→M3 이어하기', 'priority': 'M3→M6·엔딩·복귀'}[name],
                  'kind': 'button-only', 'rom_sha256': ROM_SHA,
                  'last_late_transfers': max(row['video.late_transfers'] for row in traces[name]
                                             if not end or row['lcd_frame'] < end), 'notes': []}
        if end:
            summary_name = f'native/{name}/prefix-summary.json'
            (out / summary_name).write_text(json.dumps(measurement['segments'][index], indent=2) + '\n')
            record['prefix_summary'] = summary_name
            record['notes'] = [f'LCD {end} 미만만 채택합니다. 같은 ROM 저장 상태에서 다음 경로로 이어집니다. 호스트 조작기만 수정했습니다.']
        else:
            copy(base / name / 'execution.json', f'native/{name}/execution.json')
            record['execution'] = f'native/{name}/execution.json'
            record['notes'] = ['모든 미션 결과를 지나 전체 결과·엔딩·타이틀 복귀까지 확인했습니다. 게임 상태를 주입하지 않았습니다.']
        runs.append(record)
        manifest['chain'].append({'name': name, 'exclusive_lcd_end': end,
                                  'transitions': measurement['segments'][index]['transitions']})
    for rel in ['route/scene-1-013931.state', 'targets/scene-3-060000.state',
                'targets/scene-1-000000.state', 'priority/scene-3-000000.state']:
        copy(base / rel, 'native/' + rel + '.gz', True)
    for path in sorted(reference.iterdir()):
        if path.is_file():
            compressed = path.suffix == '.csv'
            copy(path, 'neogeo/' + path.name + ('.gz' if compressed else ''), compressed)
    comparisons = []
    full_run_captures = []
    for lcd, title in [(134400, '실제 전체 경로 · 최종 결과창'), (137700, '실제 전체 경로 · 엔딩 크레딧')]:
        row = next(row for row in traces['priority'] if row['lcd_frame'] == lcd)
        name = f'native/full-route-lcd-{lcd:06}.png'
        png(base / 'priority' / f'frame-{lcd:06}.ppm', name)
        full_run_captures.append({'title': title, 'image': name, 'scene': row['scene.scene'],
                                  'camera_x': row['scene.camera.x'] >> 16,
                                  'camera_y': row['scene.camera.y'] >> 16, 'lcd': lcd})
    for lcd in [143700, 144519]:
        png(base / 'priority' / f'frame-{lcd:06}.ppm', f'native/full-route-lcd-{lcd:06}.png')
    for mission, scene, x, y in CAMERAS:
        candidates = [row for row in traces['priority']
                      if row['world.weapons.scene_number'] == scene and row['session.state'] == 1 and
                      row['scene.camera.x'] >> 16 == x and row['scene.camera.y'] >> 16 == y and
                      row.get('world.results.live_active[0]') == 1 and
                      (base / 'priority' / f'frame-{row["lcd_frame"]:06}.ppm').exists()]
        if not candidates:
            raise SystemExit(f'No actual R M{mission} screenshot at exact source camera{x},{y}')
        row = candidates[0]
        lcd = row['lcd_frame']
        name = f'native/m{mission}-lcd-{lcd:06}.png'
        png(base / 'priority' / f'frame-{lcd:06}.ppm', name)
        comparisons.append({'title': f'M{mission} — 카메라 ({x}, {y})',
                            'original': {'image': f'neogeo/m{mission}-{x}-{y}-original.png',
                                         'scene': scene, 'camera_x': x, 'camera_y': y},
                            'gba': {'image': name, 'scene': scene, 'camera_x': x, 'camera_y': y, 'lcd': lcd},
                            'notes': ['배경 배치와 표시를 대조했습니다. 적·플레이어 위치, 파괴 상태와 시간은 동기화하지 않은 제한된 비교입니다.']})
        hwdir = base / f'hardware-m{mission}'
        for filename in [f'm{mission}-hardware.json', 'snapshot-state.json', 'bg-layout.c', 'bg-layout.bin',
                         'ewram.bin', 'iwram.bin', 'vram.bin', 'oam.bin', 'pal.bin', 'io.bin',
                         'telemetry.jsonl', 'inputs.csv', 'commits.csv']:
            compressed = filename.endswith(('.bin', '.csv', '.jsonl'))
            copy(hwdir / filename, f'hardware/m{mission}/' + filename + ('.gz' if compressed else ''), compressed)
        for path in hwdir.glob('frame-*.ppm'):
            if path.name != 'frame-000000.ppm':
                png(path, f'hardware/m{mission}/{path.stem}.png')
    png(base / 'final/frame-024000.ppm', 'native/fin-targeted-lcd-024000.png')
    comparisons.append({'title': 'FIN — 별 배경과 글자',
                        'original': {'image': 'neogeo/ending-fin-original.png', 'scene': 11, 'camera_x': 832,
                                     'camera_y': 120, 'progress': 17216, 'source_frame': 80400},
                        'gba': {'image': 'native/fin-targeted-lcd-024000.png', 'scene': 11, 'camera_x': 832,
                                'camera_y': 120, 'progress': 17216, 'lcd': 24000},
                        'notes': ['같은 최종 ROM의 별도 결과창 주입 진단에서 촬영한 안정된 FIN 장면입니다. 전체 캠페인 완주 증거와 구분합니다.',
                                  '별 배경 38,400픽셀의 소스 인덱스·RGB555와 실제 타일·맵·팔레트가 모두 일치했습니다. FIN 글자의 흰 획과 검은 그림자도 보존했습니다.']})
    for filename in ['validation.json', 'fin-starfield-hardware.json', 'fin-actual-source-gba-background.png']:
        copy(ROOT / 'docs/campaign-0.5.134/render' / filename, 'render/' + filename)
    fin_dump = ROOT / 'build/fin-render/r-snapshot'
    for filename in ['snapshot-state.json', 'ewram.bin', 'iwram.bin', 'vram.bin', 'oam.bin', 'pal.bin', 'io.bin',
                     'telemetry.jsonl', 'commits.csv', 'inputs.csv']:
        compressed = filename != 'snapshot-state.json'
        copy(fin_dump / filename, 'render/fin-hardware/' + filename + ('.gz' if compressed else ''), compressed)
    png(fin_dump / 'frame-002698.ppm', 'render/fin-hardware/actual-snapshot.png')
    for filename in ['r-campaign-completion.json', 'r-cartridge-static.json', 'r-final-stack.json']:
        copy(ROOT / 'docs/campaign-0.5.134' / filename, filename)
    copy(base / 'priority/iwram.bin', 'native/priority/iwram.bin.gz', True)
    copy(ROOT / 'docs/campaign-0.5.134/m6-object-capacity-validation.json', 'm6-object-capacity-validation.json')
    for filename in ['execution.json', 'telemetry.jsonl', 'commits.csv', 'inputs.csv']:
        compressed = filename != 'execution.json'
        copy(base / 'final' / filename, 'native/final-targeted/' + filename + ('.gz' if compressed else ''), compressed)
    runs.append({'label': '전체 결과·FIN 별도 진단', 'kind': 'injected-diagnostic',
                 'execution': 'native/final-targeted/execution.json', 'rom_sha256': ROM_SHA,
                 'last_late_transfers': 0,
                 'notes': ['M1 결과에서 진행값을5로 한 번 주입한 진단입니다. 엔딩 전송과 FIN의 상세 검증에만 사용했습니다.']})
    return_dir = base / 'return-view'
    if (return_dir / 'execution.json').exists():
        return_trace = trace(return_dir / 'telemetry.jsonl')
        title_row = next(row for row in return_trace if row['lcd_frame'] == 600)
        if title_row['session.state'] != 10 or title_row['playing'] or title_row['screen'] != 0:
            raise SystemExit('Return-view600 is not a title/menu frame')
        for filename in ['execution.json', 'telemetry.jsonl', 'commits.csv', 'inputs.csv', 'stderr.log']:
            compressed = filename != 'execution.json'
            copy(return_dir / filename, 'native/return-view/' + filename + ('.gz' if compressed else ''), compressed)
        copy(base / 'priority/scene-11-144000.state', 'native/return-view/parent-scene-11-144000.state.gz', True)
        png(return_dir / 'frame-000600.ppm', 'native/return-view/title-000600.png')
        provenance = {'kind': 'same-ROM button continuation', 'rom_sha256': ROM_SHA,
                      'parent': 'r/priority/scene-11-144000.state', 'stop_on_return': False,
                      'purpose': 'Verify title pixels after session10; excluded from primary FPS chain',
                      'title_frame': 600, 'title_state': {key: title_row[key] for key in ['screen', 'playing', 'session.state']},
                      'no_game_ram_injection': True}
        (out / 'native/return-view/provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
        runs.append({'label': '타이틀 화면 이어하기 확인', 'kind': 'button-only',
                     'execution': 'native/return-view/execution.json', 'rom_sha256': ROM_SHA,
                     'last_late_transfers': max(row['video.late_transfers'] for row in return_trace),
                     'notes': ['같은 ROM의 마지막 엔딩 상태에서 계속 진행했습니다. LCD600에 실제 타이틀과1P/2P 메뉴를 확인했고, 이후 데모로 넘어갑니다. 미션 FPS에는 포함하지 않았습니다.']})
        full_run_captures.append({'title': '같은 ROM 이어하기 · 실제 타이틀 복귀',
                                  'image': 'native/return-view/title-000600.png', 'scene': 255,
                                  'camera_x': 0, 'camera_y': 0, 'lcd': 600})
    spec = {
        'candidate': {'label': 'R', 'version': '0.5.134', 'rom': str(base / 'rom.gba')},
        'summary': '같은 최종 ROM의 버튼 입력 경로로 M1–M6 보스와 결과창, 엔딩을 거쳐 타이틀 복귀까지 확인했습니다. 평균30fps와 전체 원본 동작 일치는 아직 달성·검증되지 않았습니다.',
        'verified_outcomes': [
            '선택한 전체 경로의 오류 기록·완전한 흰 화면·전송 지연 카운터 증가가 모두0입니다.',
            'M3–M6의 원본 카메라 일치 장면과 실제 BG 타일·맵 전송을 확인했습니다.',
            '전체 결과창 타일 부족, 엔딩 후속 크레딧 누락, FIN 배경·글자와 M6 고밀도 타일 배치를 수정했습니다.',
        ],
        'changes': [
            'M3–M6 배경을 GBA8×8 타일·4bpp 팔레트·맵으로 사전 변환하고 M1/M2와 공통 하드웨어BG 경로에 연결했습니다.',
            '구역별 팔레트, 유지되는VRAM 타일 위치, 변경 구간만 묶는DMA 전송을 사용합니다.',
            '전체 결과창 도면 한 장을 두affine BG로 나눠 OBJ 타일 용량을 확보했습니다.',
            '장면 전환 때 엔딩 상위 이벤트 작업을 유지하여 이후 크레딧과 장식 오브젝트가 이어지도록 수정했습니다.',
            '화면 밖 OBJ 타일 부분의 정확한 생략·재배치로 M6 타일 용량 오류를 막았습니다. 보이는 픽셀의 대상·인덱스는 유지됩니다.',
            '실제로 사용 중인 HUD 타일만 업로드하고, 변경 타일 구간을 한 번에 전송하도록 묶었습니다.',
            'FIN 별 배경의 팔레트 연결을 복구하고 원본 흰 획·검은 그림자를 유지하는36×12 글자로 교체했습니다.',
        ],
        'runs': runs, 'performance': 'performance-r.json', 'transition_validation': 'render/validation.json',
        'completion': 'r-campaign-completion.json',
        'campaign_captures': full_run_captures,
        'hardware': [{'label': f'M{m}', 'report': f'hardware/m{m}/m{m}-hardware.json'} for m in range(3, 7)],
        'comparisons': comparisons,
        'remaining': [
            '평균30fps 목표는 달성하지 못했습니다. 자동 조작기의 대기·전투 시간이 포함되므로 사람의 플레이 평균이나 공정한 전후 개선율로 해석할 수 없습니다.',
            '장면·카메라는 맞췄지만 적·플레이어 위치, 파괴 상태와 시간은 동기화하지 않았습니다. 엄격한 전체 네오지오 비교 게이트는 미통과입니다.',
            '기존 고밀도 팔레트 공유는 색을 근사합니다. 한 압박 진단에서 보이는15,471픽셀의 대상·인덱스는 보존됐지만4,170픽셀은 원본RGB와 달랐습니다. 타일 배치 수정이 색까지 무손실이라는 뜻은 아닙니다.',
            '검증한 자동 입력 경로와 개별 고밀도 진단 밖의 모든 전투 조합, 실기GBA, 2P 링크는 검증하지 않았습니다.',
        ],
    }
    # Verify observed metrics before publishing the zero-error wording above.
    if (execution['all_white_game_frames'] or any(segment['white_frames'] or segment['fault_samples'] or
            segment['observed_late_counter_increments'] for segment in measurement['segments'])):
        raise SystemExit('Observed fault/white/late evidence conflicts with planned report wording; manual review required')
    gate = ROOT / 'docs/campaign-0.5.134/reference-gate-r.json'
    reference_manifest = ROOT / 'docs/campaign-0.5.134/reference-manifest-r.json'
    if gate.exists():
        copy(gate, 'reference-gate-r.json')
        spec['reference_gate'] = 'reference-gate-r.json'
    if reference_manifest.exists():
        copy(reference_manifest, 'reference-manifest-r.json')
    (out / 'report-spec.json').write_text(json.dumps(spec, ensure_ascii=False, indent=2) + '\n')
    manifest['comparisons'] = comparisons
    (out / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    subprocess.run([sys.executable, str(ROOT / 'tools/report_campaign134.py'), '--spec', str(out / 'report-spec.json'),
                    '--output', str(out / 'Metal-Slug-GBA-v0.5.134-report.html')], check=True)
    # Four mission pairs plus FIN; untouched captures at native dimensions2x.
    canvas = Image.new('RGB', (1168, 2915), '#edf1f4')
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 18)
    small = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 15)
    draw.text((20, 12), 'v0.5.134: actual original Neo Geo / GBA captures', font=font, fill='#173243')
    draw.text((20, 42), 'Each image2x at native dimensions. Camera matches; combat states and clocks are not synchronized.', font=small, fill='#486171')
    for i, case in enumerate(comparisons):
        y = 82 + i * 565
        label = f'M{i + 3}' if i < 4 else 'FIN - separate targeted diagnostic'
        draw.text((20, y), f'{label}: camera {case["original"]["camera_x"]},{case["original"]["camera_y"]}', font=font, fill='#173243')
        draw.text((20, y + 29), 'Original320x224', font=small, fill='#486171')
        draw.text((680, y + 29), 'GBA240x160', font=small, fill='#486171')
        canvas.paste(Image.open(out / case['original']['image']).resize((640, 448), Image.Resampling.NEAREST), (20, y + 58))
        canvas.paste(Image.open(out / case['gba']['image']).resize((480, 320), Image.Resampling.NEAREST), (680, y + 58))
    canvas.save(out / 'actual-original-gba-comparison.png')
    manifest['artifacts'] = [{'file': str(path.relative_to(out)), 'bytes': path.stat().st_size, 'sha256': sha(path)}
                             for path in sorted(out.rglob('*')) if path.is_file() and path.name != 'manifest.json']
    (out / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'evidence': str(out), 'rom_sha256': ROM_SHA,
                      'bytes': sum(path.stat().st_size for path in out.rglob('*') if path.is_file()),
                      'fps': {key: value['fps'] for key, value in measurement['missions'].items()}}, ensure_ascii=False))


if __name__ == '__main__':
    main()
