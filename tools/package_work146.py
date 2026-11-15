#!/usr/bin/env python3
"""Package the measured v146 candidate with its source and verification record."""
import argparse
import hashlib
import json
import re
import shutil
import zipfile
from pathlib import Path
from compile_port import port_units

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_fingerprint():
    inputs = set(port_units()+['src/startup.S', 'src/linker.ld', 'tools/build_native.py', 'VERSION'])
    inputs.update(str(p.relative_to(ROOT)) for folder in ['src', 'generated']
                  for pattern in ['*.h', '*.inc'] for p in (ROOT/folder).glob(pattern)
                  if p.name != 'native-link-build.h')
    for unit in list(inputs):
        if unit.endswith('.S'):
            inputs.update(re.findall(r'\.incbin\s+"([^"]+)"', (ROOT/unit).read_text()))
    digest = hashlib.sha256()
    for unit in sorted(inputs):
        digest.update(unit.encode()+b'\0')
        digest.update((ROOT/unit).read_bytes())
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--measurement', type=Path, default=ROOT/'docs/work146/candidate146.json')
    parser.add_argument('--variant', choices=['C', 'D', 'E'], required=True)
    parser.add_argument('--output', type=Path, default=ROOT.parents[1]/'output')
    args = parser.parse_args()
    version = (ROOT/'VERSION').read_text().strip()
    assert version == '0.5.146-wip'
    base = json.loads((ROOT/'docs/work146/baseline145.json').read_text())
    final = json.loads(args.measurement.read_text())
    build = json.loads((ROOT/'generated/native-build.json').read_text())
    rom = ROOT/'dist'/f'metal-slug-gba-native-v{version}.gba'
    assert sha(rom) == build['rom_sha256']
    assert source_fingerprint() == build['fingerprint']
    assert final['provenance']['rom_sha256'] == build['rom_sha256']
    assert final['provenance']['source_build_fingerprint'] == build['fingerprint']
    assert final['provenance']['candidate'] == args.variant
    assert final['return_to_title_lcd'] is not None and len(final['missions']) == 6
    assert not max(final['faults'].values()) and final['execution']['exit_code'] == 0
    assert final['post_title']['fault_max'] == 0
    old_active, active = base['active_span'], final['active_span']
    before, after = old_active['aggregate_display_fps'], active['aggregate_display_fps']
    lines = [
        f'Metal Slug GBA {version} — HUD 계산 및 화면 전송 개선', '',
        f'활성 구간 전체 표시 평균: {before:.4f} -> {after:.4f} FPS ({(after/before-1)*100:+.2f}%).',
        f"활성 구간 게임틱 처리율: {old_active['aggregate_tick_fps']:.4f} -> {active['aggregate_tick_fps']:.4f} ticks/s.",
        ('평균 FPS는 사실상 유지. 전체 FPS 향상이나 최악 구간 개선으로 해석하지 않음.' if args.variant == 'E' else '평균 FPS 및 지연 분포의 절충은 아래 수치와 선택 기록 참고.'),
        '30 FPS 고정 또는 GBA 최적화의 극한에 도달했다는 의미는 아님.', '',
        '측정: v0.5.145 -> v0.5.146, mGBA의 새 게임부터 시작한 입력 전용 실행',
    ]
    for mission, row in active['missions'].items():
        old = old_active['missions'][mission]
        lines.append(f"{mission}: {old['display_fps']:.4f} -> {row['display_fps']:.4f} FPS; 최장 {old['worst_ms']:.2f} -> {row['worst_ms']:.2f}ms; 100ms 이상 {old['over_100ms']} -> {row['over_100ms']}회; p99 {old['p99_ms']:.2f} -> {row['p99_ms']:.2f}ms")
    lines += [
        '', '적용한 변경',
        ('- HUD의 소규모 변화는 기존 영역을 확장해 재사용. 64칸을 넘는 변화와 저장 공간 한도 초과는 전체 재구성.' if args.variant == 'C' else '- 기존 HUD 사각형 배치·갱신·전체 재구성 정책 유지.'),
        '- HUD의 가장 가까운 색 선택 계산을 정확히 같은 결과의 32KB ROM 조회표로 대체. 불투명 검정과 투명 인덱스 구분 유지.',
        ('- 같은 위치·크기의 HUD 사각형이 색상별로 겹치면 하나로 합성해 OBJ 타일과 OAM 사용량을 줄임. 원본 색상별 픽셀 캐시는 유지.' if args.variant != 'E' else '- 영역 확장·합성 실험은 평균 FPS 저하가 확인되어 최종본에서 제외.'),
        '- HUD 출력 방식 전환 시 이전 방식의 색상표와 픽셀이 잘못 재사용되지 않도록 캐시 갱신.',
        '- HUD 한 칸 쓰기에서 불필요한 범용 사각형 순회 제거. 콜백/좌표 자르기/오류 전파 유지.',
        '- 장면 전환에서 기존 BG3 HUD가 사용하는 VRAM 주소를 보호하고, 빈 영역에 다음 배경을 미리 전송.',
        '- 읽을 수 없는 BLDY 레지스터를 읽어 화면이 가려졌다고 판단하던 경로 제거. 강제 blank 또는 실제 검정 팔레트만 허용.',
        '', '품질 및 검증 범위',
        '- 이번 변경은 추가 화질/음질 저하를 의도하지 않음. 기존 75% 그래픽, v145의 6kHz/4효과음/PCM8 정책 유지.',
        '- 게임 로직, 충돌 판정, 공격/생성/난수 순서 코드는 변경하지 않음.',
        '- HUD 조회표 32768색 및 실제 매퍼 65536입력 일치; 단일 셀 출력 668160비교 일치.',
        '- HUD 갱신·색상 변환, 깜빡임/전체 지움/저장 공간 초과/실제 대규모 변경: 217프레임·16665600픽셀 비교 일치 및 ASan/UBSan 통과.',
        ('- 사각형 합성은 빈 첫 항목, 뒤 항목 제거, 팔레트 변경, 겹친 픽셀 우선순위, suffix/compact 왕복, 제자리 변환을 별도로 검증.' if args.variant != 'E' else '- 원래 사각형 배치, 글자·팔레트 변경, suffix/compact 왕복 시 색상표·픽셀 복구, 제자리 변환을 별도로 검증.'),
        '- 기존 배경 VRAM 보호 2048000비교 및 HUD/팔레트/알 수 없는 화면 모드 시험 통과.',
        '- 원본 Neo Geo 실제 실행 화면과의 대조는 도입부로 제한. 전체 게임 동작/모든 장면 동일성, 실기 GBA 및 2P 링크는 미검증.',
        '', '통합 플레이 확인',
        f"- 새 게임부터 mode8/M3_NATIVE=1로 230000 LCD 실행. M1~M6 결과/엔딩/타이틀 복귀 LCD{final['return_to_title_lcd']}.",
        f"- 본편 오류0, 전송 초과 {base['late_max']} -> {final['late_max']}; 전체 흰 화면 {final['execution'].get('all_white_game_frames','기록 참조')}.",
        f"- 타이틀 복귀 후 데모 전송 초과 {base['post_title']['late_max']} -> {final['post_title']['late_max']}; 데모 오류 {final['post_title']['fault_max']}.",
        '- 주 지표는 동일 미션의 연속된 활성 표시 커밋 수/실제 LCD 경과 시간. 게임틱 처리율도 따로 집계. 메뉴/결과/일시정지/사망 및 상태 경계는 제외.',
        '- 측정 보정: IRQ와 CPU 사이의 관측 시점 때문에 틱 차이가 0 또는 2로 읽히는 정상 표시 간격도 포함. v145/A/B/C/D/E 모두 같은 기준으로 재집계. 과거의 24.9786 FPS 및 188회 수치와 직접 혼합하지 않음.',
        '- 같은 상태 대응 조작기를 사용했으나 성능에 따라 전투와 입력 시점이 달라질 수 있어 완전히 같은 작업량 비교는 아님.',
        f'- 평균 이외 p50/p95/p99/최장 간격/100ms 이상 횟수는 candidate{args.variant}146.json의 active_span에 기록. 일부 긴 멈춤은 남음.',
        ('- M5 문구 등장 구간은 v145와 같은 7 LCD(117.20ms). HUD 색 변환 CPU 비용은 약4.91ms 줄었지만 이 구간의 표시 간격은 줄지 않음.' if args.variant == 'E' else '- M5 문구 등장 구간의 상세 CPU/표시 간격 비교는 source-event-comparison.json 참고.'),
        '', '남은 작업',
        '- M6의 OBJ 타일 용량 부족 시 여러 번 수행하는 그리기 준비, 전체 그리기 사전 변환, 독립 파티클 처리, 전미션 BG HUD는 추가 검토 대상.',
        '- 기존 안정 버전 및 v145는 별도로 보존. 서로 다른 ROM의 에뮬레이터 save state를 섞지 말고 새 게임으로 시작.',
        '', '재현 및 자료',
        '- ARM_CC=<arm-none-eabi-gcc> python tools/build_native.py --incremental',
        '- python tools/performance_regressions.py --only fixed-incremental146 '+('fixed-compact-format146' if args.variant == 'E' else 'fixed-compact-groups146')+' fixed-palette-lut146 native-inherit-hud146',
        '- docs/work146의 HUD 시험, 프로파일, 화면 대조, 캠페인 실행 기록 참고.',
        '', f'ROM {rom.stat().st_size} bytes', f'SHA256 {sha(rom)}',
        f"관측 스택: {final.get('observed_stack_bytes')}",
    ]
    readme = ROOT/f'README-v{version}.ko.txt'
    readme.write_text('\n'.join(lines)+'\n')
    args.output.mkdir(parents=True, exist_ok=True)
    outputs = [args.output/rom.name, args.output/f'metal-slug-gba-v{version}-source.zip', args.output/readme.name]
    shutil.copy2(rom, outputs[0])
    shutil.copy2(readme, outputs[2])
    allowed = {'art', 'docs', 'generated', 'm1_native', 'src', 'tests', 'tools'}
    with zipfile.ZipFile(outputs[1], 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(ROOT.rglob('*')):
            if not path.is_file():
                continue
            rel = path.relative_to(ROOT)
            if any(part.startswith('.') or part == '__pycache__' for part in rel.parts):
                continue
            if len(rel.parts) > 1 and rel.parts[0] not in allowed:
                if rel.parts[0] != 'dist' or path.name not in (rom.name, 'metal-slug-gba-native-v0.5.145-wip.gba'):
                    continue
            archive.write(path, 'metal-slug-native/'+str(rel))
    with zipfile.ZipFile(outputs[1]) as archive:
        assert archive.testzip() is None
    print(json.dumps({'artifacts': [{'path': str(p), 'bytes': p.stat().st_size, 'sha256': sha(p)} for p in outputs]}, indent=2))


if __name__ == '__main__':
    main()
