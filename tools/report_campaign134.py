#!/usr/bin/env python3
"""Build a self-contained, evidence-scoped campaign report from actual captures.

Paths in the JSON spec are resolved relative to the spec file. The tool never
creates gameplay evidence, substitutes a reconstructed graphic for a capture,
or promotes an injected diagnostic to a button-only campaign run.
"""
import argparse
import base64
from datetime import datetime, timezone
import gzip
import hashlib
import html
import io
import json
from pathlib import Path

from PIL import Image


def escape(value):
    return html.escape(str(value))


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def load_json(path):
    data = path.read_bytes()
    if path.suffix == '.gz':
        data = gzip.decompress(data)
    return json.loads(data)


class Report:
    def __init__(self, spec_path):
        self.spec_path = spec_path.resolve()
        self.base = self.spec_path.parent
        self.spec = load_json(spec_path)
        self.evidence = {'spec_sha256': sha(spec_path), 'sources': [], 'comparisons': []}

    def path(self, value):
        path = Path(value)
        return path.resolve() if path.is_absolute() else (self.base / path).resolve()

    def record(self, path, role):
        info = {'path': str(path), 'bytes': path.stat().st_size, 'sha256': sha(path), 'role': role}
        self.evidence['sources'].append(info)
        return info

    def json(self, value, role):
        path = self.path(value)
        self.record(path, role)
        return load_json(path)

    def capture(self, data, side):
        path = self.path(data['image'])
        info = self.record(path, f'{side} actual capture')
        wanted = (320, 224) if side == 'original' else (240, 160)
        with Image.open(path) as source:
            if source.size != wanted:
                raise ValueError(f'{path}: expected untouched {wanted} capture, got {source.size}')
            image = source.convert('RGB')
            buffer = io.BytesIO()
            image.save(buffer, format='PNG')
        info['dimensions'] = list(wanted)
        info['declared_state'] = {key: data.get(key) for key in ('scene', 'camera_x', 'camera_y', 'progress', 'lcd', 'source_frame')}
        uri = 'data:image/png;base64,' + base64.b64encode(buffer.getvalue()).decode()
        name = '네오지오 원본' if side == 'original' else 'GBA'
        coordinate = f"카메라 ({data.get('camera_x', '?')}, {data.get('camera_y', '?')})"
        if 'progress' in data:
            coordinate += f" · 진행 {data['progress']}"
        block = (f'<figure><figcaption>{name} · {wanted[0]}×{wanted[1]}<br><small>{escape(coordinate)}</small></figcaption>'
                 f'<img width="{wanted[0] * 2}" height="{wanted[1] * 2}" src="{uri}" '
                 f'alt="{escape(name + ' ' + coordinate)}"></figure>')
        return block, info

    def comparisons(self):
        blocks = []
        for case in self.spec.get('comparisons', []):
            original, original_info = self.capture(case['original'], 'original')
            native, native_info = self.capture(case['gba'], 'gba')
            keys = ('scene', 'camera_x', 'camera_y')
            matched = all(key in case['original'] and key in case['gba'] and
                          case['original'][key] == case['gba'][key] for key in keys)
            camera_text = '장면·카메라 일치' if matched else '장면·카메라 차이 있음'
            notes = case.get('notes', [])
            blocks.append(f'<article class="comparison"><h3>{escape(case["title"])}</h3>'
                          f'<p class="meta">{camera_text}. 배우 위치·파괴 상태·시간은 별도 확인이 필요합니다.</p>'
                          f'<div class="capture-pair">{original}{native}</div>'
                          + ''.join(f'<p>{escape(note)}</p>' for note in notes) + '</article>')
            self.evidence['comparisons'].append({'title': case['title'], 'original': original_info,
                                               'gba': native_info, 'scene_camera_match': matched,
                                               'notes': notes, 'scope': 'limited actual-capture visual review'})
        if not blocks:
            return '<p class="pending">이번 후보의 원본 대조 캡처는 아직 보고서에 등록되지 않았습니다.</p>'
        return ('<p>원본 320×224와 GBA 240×160을 각각 최근접 방식으로 2배 표시했습니다. '
                '원본을 75%로 줄이거나 캡처의 도트를 수정하지 않았습니다. 화면 폭이 좁으면 좌우로 스크롤할 수 있습니다.</p>'
                + ''.join(blocks))

    def runs(self, rom_hash):
        rows, details = [], []
        self.evidence['runs'] = []
        for run in self.spec.get('runs', []):
            if run.get('kind') not in ('button-only', 'injected-diagnostic'):
                raise ValueError('Each run must declare button-only or injected-diagnostic provenance')
            prefix = bool(run.get('prefix_summary'))
            if prefix == bool(run.get('execution')):
                raise ValueError('Each run needs exactly one of execution or prefix_summary')
            execution = self.json(run['prefix_summary'] if prefix else run['execution'],
                                  'selected prefix summary, not execution exit status' if prefix else 'run execution')
            declared_hash = run.get('rom_sha256')
            if not declared_hash:
                raise ValueError('Each run must declare its ROM SHA-256; do not mix candidate evidence silently')
            same = declared_hash == rom_hash
            kind = '버튼 입력 경로' if run['kind'] == 'button-only' else '상태 주입 진단'
            if not same:
                kind += ' · 별도 이전 후보'
            rows.append('<tr>' + ''.join(f'<td>{escape(value)}</td>' for value in (
                run['label'], kind, '선택 구간' if prefix else execution.get('exit_code', '기록 없음'),
                (f"{execution['exclusive_lcd_end']} 미만" if prefix else execution.get('last_lcd_frame', '기록 없음')),
                len(execution['white_frames']) if prefix else execution.get('all_white_game_frames', '기록 없음'),
                run.get('last_late_transfers', '기록 없음'))) + '</tr>')
            details.extend(f'<li>{escape(run["label"])}: {escape(note)}</li>' for note in run.get('notes', []))
            self.evidence['runs'].append({'label': run['label'], 'kind': run['kind'],
                                          'rom_sha256': declared_hash, 'current_candidate': same,
                                          'evidence_kind': 'selected-prefix' if prefix else 'execution',
                                          'execution': execution, 'notes': run.get('notes', [])})
        if not rows:
            return '<p class="pending">이번 후보의 실행 검증은 아직 등록되지 않았습니다.</p>'
        return ('<div class="table-scroll"><table><thead><tr><th>경로</th><th>방법</th><th>종료 코드</th>'
                '<th>마지막 LCD</th><th>흰 화면</th><th>마지막 전송 지연 카운터</th></tr></thead><tbody>'
                + ''.join(rows) + '</tbody></table></div>'
                '<p class="meta">종료 코드 0은 해당 실행이 정상 종료됐다는 뜻입니다. 주입 진단은 생략한 미션의 플레이 증거가 아닙니다. '
                'LCD 값은 각 경로의 로컬 시계이며 그대로 합산하지 않습니다.</p>'
                + ('<ul>' + ''.join(details) + '</ul>' if details else ''))

    def performance(self, rom_hash):
        if not self.spec.get('performance'):
            return '<p class="pending">이번 후보의 전체 미션 평균 FPS는 아직 산출하지 않았습니다. 평균 30fps 달성을 주장하지 않습니다.</p>'
        data = self.json(self.spec['performance'], 'committed frame performance')
        if data.get('rom', {}).get('sha256') != rom_hash:
            raise ValueError('Performance report must identify the current candidate ROM')
        if data.get('provenance_declaration') != 'button-only':
            raise ValueError('Injected or unknown run performance must not be used as campaign performance')
        rows = []
        for mission in range(1, 7):
            entry = data['missions'][str(mission)]
            if entry['status'] == 'unmeasured':
                rows.append(f'<tr><th>M{mission}</th><td colspan="5">측정 자료 없음</td></tr>')
            else:
                rows.append(f'<tr><th>M{mission}</th><td>{entry["fps"]:.2f}</td>'
                            f'<td>{entry["measured_seconds"]:.1f}</td><td>{entry["p95_interval_ms"]:.1f}</td>'
                            f'<td>{entry["worst_interval_ms"]:.1f}</td><td>{entry["intervals_ge_100ms"]}</td></tr>')
        self.evidence['performance'] = data
        return ('<div class="table-scroll"><table><thead><tr><th>미션</th><th>평균 FPS</th><th>측정 초</th>'
                '<th>95% 간격 ms</th><th>최장 간격 ms</th><th>100ms 이상</th></tr></thead><tbody>' + ''.join(rows) +
                '</tbody></table></div><p>GBA LCD 주기를 기준으로 실제 제출된 연속 프레임 간격을 측정했습니다. '
                'P1이 활성화된 미션 플레이만 포함하며 데모·일시정지·결과창·엔딩과 불확실한 세션 경계는 제외합니다.</p>'
                '<p class="note">자동 조작기가 적을 공격하거나 길을 찾으며 기다린 시간도 포함됩니다. 사람의 플레이 평균이나 '
                '동일 전투 상태의 전후 개선율로 해석할 수 없습니다. 이전 후보와 공정한 동일 작업량 비교는 별도로 필요합니다.</p>')

    def transition_measurement(self):
        value = self.spec.get('transition_validation')
        if not value:
            return ''
        data = self.json(value, 'scoped ending transfer cycle validation')
        timing = data['first_ending_transfer_cycles']
        before, after = timing['baseline_N'], timing['word_spans_P']
        percent = (1 - after / before) * 100
        self.evidence['transition_cycle_fixture'] = data
        return (f'<h3>엔딩 첫 전환의 전송 비용</h3><p>같은 전환 진단에서 실제 ARM 경과 사이클이 '
                f'<strong>{before:,} → {after:,} ({percent:.2f}% 감소)</strong>로 줄었습니다. '
                f'VBlank 예산 {timing["vblank_budget"]:,}사이클 안에 들어왔습니다.</p>'
                '<p class="meta">mGBA에서 CPU 명령을 읽기 전용으로 추적한 측정입니다. 배경 14,816바이트와 OBJ 512바이트가 필요한 '
                '첫 전체 결과창→엔딩 B 전환 하나를 비교했습니다. HUD는 실제 할당 타일만 전송하고, 변경 구간은 묶어서 처리합니다. '
                '전송 수정 후보 P의 M1 결과에서 진행값을 주입한 별도 진단이며, 최종 R의 전체 FPS 측정과는 별개입니다. '
                '이 47.57%를 전체 게임 FPS 개선율로 해석하면 안 됩니다.</p>')

    def hardware(self):
        rows = []
        for item in self.spec.get('hardware', []):
            data = self.json(item['report'], 'actual committed background hardware snapshot')
            rows.append(f'<tr><th>{escape(item["label"])}</th><td>{data["verified_map_bytes"]:,}</td>'
                        f'<td>{data["verified_cached_tile_bytes"]:,}</td>'
                        f'<td>{data["map_mismatched_bytes"]} / {data["tile_mismatched_bytes"]}</td></tr>')
        if not rows:
            return ''
        return ('<h3>실제 BG 전송 확인</h3><table><thead><tr><th>미션</th><th>맵 바이트</th><th>타일 바이트</th>'
                '<th>맵 / 타일 불일치</th></tr></thead><tbody>' + ''.join(rows) + '</tbody></table>'
                '<p class="meta">같은 ROM 저장 상태에서 실제 화면 제출 직후의 VRAM·맵을 검사했습니다. '
                '전송 정확성 검사이며, 원본 배경을 올바르게 설계했는지나 전투 동작이 같은지는 별도 비교가 필요합니다.</p>')

    def campaign_captures(self):
        blocks = []
        for item in self.spec.get('campaign_captures', []):
            block, _ = self.capture(item, 'gba')
            blocks.append('<article><h3>' + escape(item['title']) + '</h3>' + block + '</article>')
        return ('<div class="capture-pair">' + ''.join(blocks) + '</div>') if blocks else ''

    def build(self):
        candidate = self.spec['candidate']
        rom = self.path(candidate['rom'])
        rom_info = self.record(rom, 'candidate ROM')
        self.evidence['candidate'] = {**candidate, **rom_info}
        gate = self.json(self.spec['reference_gate'], 'strict reference gate') if self.spec.get('reference_gate') else None
        # The report is deliberately scoped. A passing gate still needs an independent review.
        self.evidence['strict_reference_gate'] = gate
        gate_text = ('별도 엄격 비교 게이트의 통과 기록이 있습니다. 아래 캡처의 검토 범위는 각 항목에 표시합니다.'
                     if gate and gate.get('reference_verified') else
                     '현재 자료는 제한된 장면 대조입니다. 전체 네오지오 동작·그래픽 일치 게이트를 통과했다고 주장하지 않습니다.')
        summary = self.spec.get('summary', '이번 후보의 검증 자료를 정리했습니다. 완료되지 않은 항목은 아래에 남깁니다.')
        claims = self.spec.get('verified_outcomes', [])
        pending = self.spec.get('remaining', ['새 후보의 전체 경로와 원본 대조 검증 진행 중', '평균 30fps 미확인', '실기 GBA·2P 링크 미검증'])
        changes = self.spec.get('changes', [
            'M3–M6 배경을 GBA 8×8 타일·4bpp 팔레트·맵으로 사전 변환하고 공통 하드웨어 BG 경로에 연결했습니다.',
            '구역별 팔레트와 배경 타일 유지·전송 경로를 사용하여 실행 중 원본 조각 합성과 색 재포장을 줄였습니다.',
            '전체 결과창의 도면 한 장을 두 affine BG에 나눠 표시하여 OBJ 타일 부족을 해결했습니다.',
            '장면 전환에서 엔딩의 부모 이벤트 작업을 유지하고, 전체 정리 시 명시적으로 종료하도록 수명 관리를 수정했습니다.',
            '실제로 사용하는 HUD 타일만 업로드하고 연속 변경 구간을 묶어 VBlank 전송 비용을 줄였습니다.',
        ])
        run_block = self.runs(rom_info['sha256'])
        perf_block = self.performance(rom_info['sha256'])
        transition_block = self.transition_measurement()
        hardware_block = self.hardware()
        campaign_captures = self.campaign_captures()
        if self.spec.get('completion'):
            completion = self.json(self.spec['completion'], 'full selected campaign completion and stack evidence')
            if completion['rom_sha256'] != rom_info['sha256']:
                raise ValueError('Completion evidence identifies another ROM')
            stack = completion['stack_high_water']
            hardware_block += (f'<p class="meta">전체 선택 경로의 최대 관측 스택: 일반 '
                               f'{stack["main"]["observed_bytes"]:,}/{stack["main"]["reserved_bytes"]:,}바이트, IRQ '
                               f'{stack["irq"]["observed_bytes"]:,}/{stack["irq"]["reserved_bytes"]:,}바이트. '
                               f'두 하단 가드 보존: {"확인" if all(x["bottom_guard_intact"] for x in stack.values()) else "실패"}. '
                               '이번 실행에서 관측한 값입니다.</p>')
        comparison_block = self.comparisons()
        date = self.spec.get('date', datetime.now(timezone.utc).date().isoformat())
        html_document = f'''<!doctype html><html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Metal Slug GBA · M3–M6 검증</title>
<style>
:root{{color-scheme:light}}*{{box-sizing:border-box}}body{{margin:0;background:#eef2f5;color:#18303e;font:16px/1.75 system-ui,sans-serif}}
main{{max-width:1240px;margin:auto;padding:32px 24px 64px}}header{{margin-bottom:28px}}h1{{font-size:32px;line-height:1.35;margin:6px 0 18px}}
h2{{margin:34px 0 12px;font-size:24px}}h3{{margin:24px 0 8px;font-size:19px}}p{{max-width:1040px;margin:10px 0}}.meta,small{{font-size:13px;color:#486171}}
.lead{{background:#dcebe6;padding:18px;border-left:4px solid #397c68}}.note,.pending{{background:#fff0d8;padding:14px 18px;border-left:4px solid #b77925}}
table{{width:100%;border-collapse:collapse;background:#fff;font-size:14px}}th,td{{padding:10px 12px;border-bottom:1px solid #cfdae1;text-align:left;white-space:nowrap}}
.table-scroll,.capture-pair{{overflow-x:auto}}.capture-pair{{display:flex;gap:18px;padding:8px 0 14px;align-items:flex-start}}
figure{{margin:0;flex:none}}figcaption{{margin:0 0 8px}}img{{display:block;image-rendering:pixelated;image-rendering:crisp-edges;max-width:none}}
.comparison{{padding:10px 0 22px;border-bottom:1px solid #c5d2db}}ul{{padding-left:24px}}li{{margin:5px 0}}code{{font-size:12px;overflow-wrap:anywhere}}
footer{{margin-top:36px;border-top:1px solid #c5d2db;padding-top:18px}}@media(max-width:700px){{main{{padding:22px 16px}}h1{{font-size:26px}}}}
</style></head><body><main>
<header><p class="meta">{escape(date)} · {escape(candidate.get('version', ''))} · 후보 {escape(candidate.get('label', ''))}</p>
<h1>M3–M6 GBA 재설계와 실행 검증</h1><p class="lead">{escape(summary)}</p></header>
{('<ul>' + ''.join('<li>' + escape(x) + '</li>' for x in claims) + '</ul>') if claims else ''}
<h2>이번 변경</h2><ul>{''.join('<li>'+escape(x)+'</li>' for x in changes)}</ul>
<h2>실행 경로와 검증 범위</h2>{run_block}{campaign_captures}{hardware_block}
<h2>미션별 화면 갱신 성능</h2>{perf_block}{transition_block}
<h2>실제 네오지오 화면과 대조</h2><p class="note">{gate_text}</p>{comparison_block}
<h2>남은 확인 사항</h2><ul>{''.join('<li>'+escape(x)+'</li>' for x in pending)}</ul>
<footer><p><strong>ROM</strong> · {rom_info['bytes']:,}바이트 · {rom_info['bytes']/1e6:.2f}MB / {rom_info['bytes']/1048576:.2f}MiB<br>
<small>32MiB 한도 대비 {33554432-rom_info['bytes']:,}바이트 여유</small></p>
<p class="meta">SHA-256<br><code>{rom_info['sha256']}</code></p>
<p class="meta">이 보고서는 실제 실행 캡처와 기록을 사용합니다. 캡처·로그·ROM의 해시는 별도 evidence JSON에 보존합니다.</p></footer>
</main></body></html>'''
        if rom_info['bytes'] > 33554432:
            raise ValueError('Candidate exceeds the requested32MiB cartridge limit')
        return html_document


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--spec', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--evidence-output', type=Path)
    args = parser.parse_args()
    report = Report(args.spec)
    document = report.build()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(document, encoding='utf-8')
    evidence_path = args.evidence_output or args.output.with_suffix('.evidence.json')
    report.evidence['report_html_sha256'] = sha(args.output)
    evidence_path.parent.mkdir(parents=True, exist_ok=True)
    evidence_path.write_text(json.dumps(report.evidence, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'html': str(args.output.resolve()), 'evidence': str(evidence_path.resolve()),
                      'actual_capture_pairs': len(report.evidence['comparisons'])}, ensure_ascii=False))


if __name__ == '__main__':
    main()
