#!/usr/bin/env python3
"""Measure committed GBA frames; never use host speed or predicted controller frames.

Example:
  python tools/measure_campaign134.py \
    build/route:build/route.log:144000 \
    build/continue:build/continue.log:0 \
    --rom build/native/native.gba --provenance button-only --output report.json

Each segment is DIRECTORY:TELEMETRY_JSONL:EXCLUSIVE_LCD_END (0 means all).
Segments are independent timelines; intervals never cross a resume boundary.
The caller must choose nonoverlapping same-ROM branches and retain their input
and state provenance. This script cannot prove that a run used only buttons.
"""
import argparse
from bisect import bisect_left, bisect_right
from collections import Counter, defaultdict
import csv
import hashlib
import html
import json
import math
from pathlib import Path

LCD_HZ = 16777216 / 280896
SCENE_TO_MISSION = {0: 1, 1: 2, 3: 3, 2: 4, 4: 5, 5: 6}
COMMIT_FIELDS = {
    'lcd', 'commits', 'ticks', 'playing', 'presentation', 'paused', 'camera_x',
    'entities', 'late', 'live0', 'demo', 'scene', 'palette_error', 'palette_spills',
}
TRACE_FIELDS = {'lcd_frame', 'session.state', 'world.weapons.scene_number', 'app.error'}
FAULT_FIELDS = (
    'app.error', 'audio.fault', 'world.fault', 'world.weapons.fault',
    'world.entities.fault', 'world.hits.fault', 'world.events.fault',
    'world.video.fault', 'world.action_fault[0]', 'world.action_fault[1]',
    'scene.fault', 'video.fault', 'video.objects.fault', 'video.objects.frame.fault',
    'video.backgrounds.fault', 'video.colors.fault', 'video.fixed.fault',
    'video.palettes.fault', 'session.fault', 'runtime.fault',
)


def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def rows_csv(path):
    with path.open(newline='') as stream:
        reader = csv.DictReader(stream)
        missing = COMMIT_FIELDS - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f'{path}: missing columns {sorted(missing)}')
        return [{key: int(value) for key, value in row.items()} for row in reader]


def rows_trace(path):
    rows = []
    ignored = 0
    for line_number, line in enumerate(path.read_text().splitlines(), 1):
        if not line.lstrip().startswith('{'):
            ignored += bool(line.strip())
            continue
        try:
            row = json.loads(line)
        except json.JSONDecodeError as exc:
            raise ValueError(f'{path}:{line_number}: malformed JSON telemetry') from exc
        missing = TRACE_FIELDS - row.keys()
        if missing:
            raise ValueError(f'{path}:{line_number}: missing telemetry {sorted(missing)}')
        rows.append(row)
    if not rows:
        raise ValueError(f'{path}: no telemetry')
    return rows, ignored


def ensure_increasing(rows, field, path):
    if any(a[field] >= b[field] for a, b in zip(rows, rows[1:])):
        raise ValueError(f'{path}: {field} must increase strictly; do not concatenate timelines')


def eligible_commit(row):
    return (row['playing'] == 1 and row['presentation'] == 0 and
            row['paused'] == 0 and row['demo'] == 0 and row['live0'] == 1)


def bracket_active(lcd, scene, trace, clocks, max_gap):
    """Both adjacent samples must show active session 1 in the same mission."""
    before = bisect_right(clocks, lcd) - 1
    after = bisect_left(clocks, lcd)
    if before < 0 or after == len(trace):
        return False
    for index in {before, after}:
        row = trace[index]
        if (abs(row['lcd_frame'] - lcd) > max_gap or row['session.state'] != 1 or
                row['world.weapons.scene_number'] != scene or row['app.error']):
            return False
    return True


def percentile(values, fraction):
    """Nearest-rank percentile of frame intervals (not instantaneous FPS)."""
    return sorted(values)[max(0, math.ceil(len(values) * fraction) - 1)]


def statistics(intervals):
    if not intervals:
        return {'status': 'unmeasured', 'interval_count': 0, 'fps': None}
    total = sum(intervals)
    return {
        'status': 'measured', 'interval_count': len(intervals),
        'lcd_periods': total, 'measured_seconds': total / LCD_HZ,
        'fps': LCD_HZ * len(intervals) / total,
        'mean_interval_ms': total / len(intervals) / LCD_HZ * 1000,
        'median_interval_ms': percentile(intervals, .5) / LCD_HZ * 1000,
        'p95_interval_ms': percentile(intervals, .95) / LCD_HZ * 1000,
        'p99_interval_ms': percentile(intervals, .99) / LCD_HZ * 1000,
        'worst_interval_ms': max(intervals) / LCD_HZ * 1000,
        'intervals_ge_100ms': sum(n / LCD_HZ >= .1 for n in intervals),
        'lcd_interval_histogram': dict(sorted(Counter(intervals).items())),
    }


def parse_segment(spec):
    try:
        directory, telemetry, end_text = spec.rsplit(':', 2)
        end = int(end_text)
    except ValueError as exc:
        raise ValueError('segment must be DIRECTORY:TELEMETRY_JSONL:EXCLUSIVE_LCD_END') from exc
    if end < 0:
        raise ValueError('exclusive LCD end must be nonnegative; 0 means all')
    return Path(directory).resolve(), Path(telemetry).resolve(), end


def analyze_segment(spec, max_gap):
    directory, trace_path, end = parse_segment(spec)
    commit_path = directory / 'commits.csv'
    full_commits = rows_csv(commit_path)
    full_trace, ignored = rows_trace(trace_path)
    ensure_increasing(full_commits, 'lcd', commit_path)
    ensure_increasing(full_trace, 'lcd_frame', trace_path)
    commits = [row for row in full_commits if not end or row['lcd'] < end]
    trace = [row for row in full_trace if not end or row['lcd_frame'] < end]
    if not trace:
        raise ValueError(f'{trace_path}: selected prefix has no telemetry')
    clocks = [row['lcd_frame'] for row in trace]
    intervals = defaultdict(list)
    excluded = Counter()
    late_increments = []
    for a, b in zip(commits, commits[1:]):
        if b['late'] > a['late']:
            late_increments.append({'lcd': b['lcd'], 'scene': b['scene'],
                                    'delta': b['late'] - a['late']})
        elif b['late'] < a['late'] and b['late']:
            late_increments.append({'lcd': b['lcd'], 'scene': b['scene'],
                                    'delta': b['late'], 'counter_reset': True})
        reason = None
        if ((b['commits'] - a['commits']) & 0xffffffff) != 1:
            reason = 'commit_delta_not_one'
        elif a['scene'] != b['scene'] or b['scene'] not in SCENE_TO_MISSION:
            reason = 'scene_boundary_or_nonmission'
        elif not eligible_commit(a) or not eligible_commit(b):
            reason = 'inactive_pause_demo_or_presentation'
        elif not (bracket_active(a['lcd'], a['scene'], trace, clocks, max_gap) and
                  bracket_active(b['lcd'], b['scene'], trace, clocks, max_gap)):
            reason = 'not_bracketed_by_active_session_telemetry'
        if reason:
            excluded[reason] += 1
        else:
            intervals[SCENE_TO_MISSION[b['scene']]].append(b['lcd'] - a['lcd'])
    transitions = []
    previous = None
    for row in trace:
        state = (row['world.weapons.scene_number'], row['session.state'])
        if state != previous:
            transitions.append({'lcd': row['lcd_frame'], 'source_scene': state[0],
                                'mission': SCENE_TO_MISSION.get(state[0]), 'session_state': state[1]})
            previous = state
    faults = [{key: row[key] for key in ('lcd_frame', *FAULT_FIELDS) if key in row}
              for row in trace if any(row.get(key, 0) for key in FAULT_FIELDS)]
    white_path = directory / 'white-frames.csv'
    whites = []
    if white_path.exists():
        with white_path.open(newline='') as stream:
            for raw in csv.reader(stream):
                row = dict(zip(('lcd', 'tick', 'scene', 'late'), map(int, raw)))
                if not end or row['lcd'] < end:
                    whites.append(row)
    execution_path = directory / 'execution.json'
    report = {
        'directory': str(directory), 'telemetry': str(trace_path), 'exclusive_lcd_end': end,
        'input_hashes': {'commits_csv': sha256(commit_path), 'telemetry_jsonl': sha256(trace_path)},
        'selected_commit_rows': len(commits), 'selected_telemetry_rows': len(trace),
        'selected_lcd_first': clocks[0], 'selected_lcd_last': clocks[-1],
        'ignored_non_json_log_lines': ignored, 'excluded_intervals': dict(excluded),
        'missions': {str(m): statistics(intervals[m]) for m in range(1, 7)},
        'transitions': transitions, 'fault_samples': faults,
        'observed_late_counter_increments': late_increments,
        'white_frames': whites,
        'whole_run_execution_not_prefix_evidence': (
            json.loads(execution_path.read_text()) if execution_path.exists() else None),
    }
    return report, intervals


def write_html(report, path):
    rows = []
    for mission, values in report['missions'].items():
        if values['status'] == 'unmeasured':
            rows.append(f'<tr><th>M{mission}</th><td colspan="5">측정 자료 없음</td></tr>')
        else:
            rows.append(f'<tr><th>M{mission}</th><td>{values["fps"]:.2f}</td>'
                        f'<td>{values["measured_seconds"]:.1f}</td>'
                        f'<td>{values["p95_interval_ms"]:.1f}</td>'
                        f'<td>{values["worst_interval_ms"]:.1f}</td>'
                        f'<td>{values["intervals_ge_100ms"]}</td></tr>')
    fault_count = sum(len(s['fault_samples']) for s in report['segments'])
    white_count = sum(len(s['white_frames']) for s in report['segments'])
    document = f'''<!doctype html><html lang="ko"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>M1–M6 실제 화면 갱신 측정</title>
<style>body{{font:16px/1.7 system-ui,sans-serif;color:#1f303b;background:#f2f5f7;margin:0}}
main{{max-width:960px;margin:auto;padding:32px 20px}}table{{width:100%;border-collapse:collapse;background:white}}
th,td{{padding:10px;border-bottom:1px solid #ccd7de;text-align:right}}th:first-child{{text-align:left}}
.note{{background:#fff0d7;padding:16px}}code{{word-break:break-all}}small{{color:#425b68}}</style>
<main><h1>M1–M6 실제 화면 갱신 측정</h1>
<p>GBA LCD 주기에서 실제로 제출된 프레임 사이의 간격을 측정했습니다. 호스트 실행 속도나 자동 조작기의 예측 프레임은 세지 않습니다.</p>
<table><thead><tr><th>미션</th><th>평균 FPS</th><th>측정 시간(초)</th><th>95% 간격(ms)</th><th>최장 간격(ms)</th><th>100ms 이상</th></tr></thead><tbody>{''.join(rows)}</tbody></table>
<p class="note">자동 플레이 경로의 결과입니다. 조작기가 적을 공격하거나 경로를 찾으며 기다린 시간도 활성 플레이에 포함됩니다. 동일 전투 상태의 전후 비교가 아니므로 성능 개선율이나 사람이 플레이할 때의 평균으로 해석할 수 없습니다. 이 표만으로 미션 완주나 네오지오 동작 일치를 판단하지 않습니다.</p>
<p>필터: 플레이 중·P1 활성·미션 세션(1), 일시정지·데모·연출 제외. 두 제출 번호가 연속이고 같은 미션인 구간만 포함합니다. 양 끝의 세션 상태는 최대 {report['method']['maximum_telemetry_gap_lcd']} LCD 간격 이내의 앞뒤 기록으로 확인합니다. 결과창·엔딩·사망 후 대기와 경계의 불확실한 구간은 평균에서 제외합니다.</p>
<p>선택한 경로의 오류 기록 {fault_count}개, 완전한 흰 화면 기록 {white_count}프레임입니다. 오류 기록 수는 고유 오류 건수가 아니며, 늦은 전송 카운터는 재설정 사이의 관측값이므로 정확한 발생 건수로 단정하지 않습니다.</p>
<p><small>ROM SHA-256: <code>{html.escape(report.get('rom', {}).get('sha256', '미지정'))}</code><br>
입력 경로 분류(호출자 선언): {html.escape(report['provenance_declaration'])}. 같은 ROM과 분기 연결은 별도 검증이 필요합니다.</small></p></main></html>'''
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(document, encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('segments', nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--html', type=Path)
    parser.add_argument('--rom', type=Path)
    parser.add_argument('--provenance', choices=['button-only', 'injected-diagnostic', 'unknown'], default='unknown')
    parser.add_argument('--max-telemetry-gap', type=int, default=60)
    args = parser.parse_args()
    if args.max_telemetry_gap < 1:
        parser.error('--max-telemetry-gap must be positive')
    combined = defaultdict(list)
    segments = []
    for spec in args.segments:
        segment, intervals = analyze_segment(spec, args.max_telemetry_gap)
        segments.append(segment)
        for mission, values in intervals.items():
            combined[mission].extend(values)
    report = {
        'schema': 'gba-campaign-committed-intervals-v1',
        'provenance_declaration': args.provenance,
        'method': {
            'lcd_hz': LCD_HZ, 'lcd_hz_exact': '16777216/280896',
            'average': 'count(intervals) * LCD_HZ / sum(intervals_in_LCD_periods)',
            'percentiles': 'nearest-rank frame intervals',
            'maximum_telemetry_gap_lcd': args.max_telemetry_gap,
            'scope': 'active P1 in mission session 1; pauses, demo, presentation, results and unbracketed boundaries excluded',
            'limitations': [
                'Feedback-controller combat and navigation delays remain in active-play workload.',
                'No equal-workload baseline or improvement percentage is established.',
                'Caller must verify same-ROM resume lineage and nonoverlapping segment cutoffs.',
                'Performance and no-crash evidence do not establish full Neo Geo equivalence.',
                'Fault samples and counter increments are observations, not unique fault event counts.',
            ],
        },
        'missions': {str(m): statistics(combined[m]) for m in range(1, 7)},
        'all_selected_active_intervals': statistics([n for values in combined.values() for n in values]),
        'segments': segments,
    }
    if args.rom:
        report['rom'] = {'path': str(args.rom.resolve()), 'bytes': args.rom.stat().st_size, 'sha256': sha256(args.rom)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if args.html:
        write_html(report, args.html)
    print(json.dumps({'output': str(args.output.resolve()), 'html': str(args.html.resolve()) if args.html else None,
                      'mission_fps': {k: v['fps'] for k, v in report['missions'].items()}}, ensure_ascii=False))


if __name__ == '__main__':
    main()
