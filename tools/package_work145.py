#!/usr/bin/env python3
"""Package the selected completed v145 campaign and its reproducible source."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--measurement', type=Path, default=ROOT/'docs/work145/candidateB145.json')
    parser.add_argument('--output', type=Path, default=ROOT.parents[1]/'output')
    args = parser.parse_args()
    version = (ROOT/'VERSION').read_text().strip()
    assert version == '0.5.145-wip'
    base = json.loads((ROOT/'docs/work145/baseline144.json').read_text())
    final = json.loads(args.measurement.read_text())
    build = json.loads((ROOT/'generated/native-build.json').read_text())
    rom = ROOT/'dist'/f'metal-slug-gba-native-v{version}.gba'
    assert sha(rom) == build['rom_sha256']
    assert final['return_to_title_lcd'] is not None and len(final['missions']) == 6
    assert not max(final['faults'].values()) and final['execution']['exit_code'] == 0
    fps0, fps1 = base['aggregate_fps'], final['aggregate_fps']
    lines = [
        f'Metal Slug GBA {version} — 게임 동작/FPS 우선 최적화', '',
        f'활성 구간 전체 평균: {fps0:.4f} -> {fps1:.4f} FPS ({(fps1/fps0-1)*100:+.2f}%).',
        '30 FPS 목표는 미달. GBA 하드웨어 최적화의 극한 또는 완전한 원본 재현을 주장하지 않음.', '',
        '측정: v0.5.144 -> 최종 v0.5.145',
    ]
    for mission, row in final['missions'].items():
        old = base['missions'][mission]
        lines.append(f"{mission}: {old['fps']:.4f} -> {row['fps']:.4f} FPS; 최장 {old['worst_ms']:.2f} -> {row['worst_ms']:.2f}ms; 100ms 이상 {old['over_100ms']} -> {row['over_100ms']}회")
    lines += [
        '', '적용한 변경',
        '- 기존 6kHz/4효과음/음악 재생률은 유지. 효과음 PCM 캐시를 16비트에서 8비트로 줄이고 M5/M6/보스 음악(0x25/0x26/0x28)을 미리 디코딩.',
        '- 객체 그리기 캡처의 간접 호출과 숨긴 sparse 장식의 버려질 임시 정보 생성 제거. 원본 strip 예산, 객체 갱신, 난수/생성 순서는 유지.',
        '- 변하지 않은 배경 팔레트 재사용. 객체 팔레트 스냅샷의 중복 초기화/RGB 복사를 제거.',
        '- OBJ 타일/OAM 부족 시 sparse 객체가 없는 큐에서 타일/OAM 부족을 해소하지 못하는 sparse-only 준비 재시도를 생략. 기존 dense/final crop 정책은 유지.',
        '', '품질 절충과 검증 범위',
        '- 추가 음질 손실: PCM8 양자화. 20,473,434 샘플 시험에서 signed8 출력 차이 최대1, clipping 양쪽0, 457 응답 시점과 논리 상태/재생 타이밍 일치. ASan/UBSan 통과.',
        '- 표시 해상도/75% 스케일/게임 판정은 변경하지 않음. 재시도 생략으로 과거 팔레트 배치에 따라 비보호 객체의 근사 색상이 달라질 수 있음.',
        '- 실제 M6 큐에 합성한 512개 팔레트 과거 상태 시험: 26개에서 색 차이, 최대1,357픽셀/화면 및 RGB5 채널 차이7/31. 형상/정체성과 보호된 플레이어/HUD/탄환 색상 차이0. 실제 캠페인512프레임 시험은 아님.',
        '- 최종B의 실제 커밋128/256/1000은 v144 대비 배경/팔레트/주소를 해석한 OBJ 픽셀과 속성 일치. A의 실제 M6 압박 프레임도 일치. 전체 캠페인 픽셀 동일성 주장은 하지 않음.',
        '- 원본 Neo Geo 실제 실행의 M1 도입부는 제한적 화면 대조만 수행. 원본 전체 동작/모든 장면 대조와 실기 GBA/2P 링크는 미검증.',
        '', '통합 플레이 확인',
        f"- 새 게임부터 같은 상태 대응 버튼 조작기(mode8,M3_NATIVE=1)로 230000 LCD 실행. M1~M6 결과/엔딩/타이틀 복귀(LCD{final['return_to_title_lcd']}) 통과.",
        f"- 본편 오류0, 전송 초과{final['late_max']}; 전체 실행 흰 화면{final['execution'].get('all_white_game_frames','기록 참조')}.",
        '- 타이틀 복귀 이후 M2 데모의 기존 전송 지연1은 미해결. 본편 결과와 분리해서 기록.',
        '- FPS는 생존/활성 상태의 연속 게임틱 간격을 실제 LCD 주파수로 계산. 메뉴/결과/일시정지/사망 구간 제외. 전투 상태가 실행별로 달라질 수 있어 완전히 동일한 작업량 비교는 아님.',
        '- 평균 외 p50/p95/p99/100ms 이상 횟수/최장 지연은 docs/work145의 JSON 참고. 일부 장면의 긴 멈춤은 남음.',
        '', '남은 주요 병목',
        '- M5 최장 지연의 큰 비중은 HUD/FIX 재구성. 기준본 동일 이벤트에서 native_fixed_refresh 약51.6ms로 관측.',
        '- 전체 그리기 사전컴파일, 독립 파티클 풀, 전미션 BG HUD는 미완료.',
        '', '재현 및 자료',
        '- python tools/build_native.py --incremental (ARM_CC 또는 arm-none-eabi-gcc 필요)',
        '- python tools/performance_regressions.py --only source-visit145 native-bg-palette object-retry145 palette-worklist capture audio-pcm8 audio-catalog',
        '- python tests/run_object_retry_palette145.py',
        '- docs/work145/profiling-method.md, execution-evidence.zip, visual-validation.json, audio-validation.json 참고.',
        '- 이전 ROM의 에뮬레이터 save state를 새 ROM에 불러오지 말고 새 게임으로 시작.',
        '', f'ROM {rom.stat().st_size} bytes', f'SHA256 {sha(rom)}',
        f"관측 스택: {final.get('observed_stack_bytes')}",
    ]
    readme = ROOT/f'README-v{version}.ko.txt'
    readme.write_text('\n'.join(lines)+'\n')
    args.output.mkdir(parents=True, exist_ok=True)
    outputs = [args.output/rom.name, args.output/f'metal-slug-gba-v{version}-source.zip', args.output/readme.name]
    shutil.copy2(rom, outputs[0]); shutil.copy2(readme, outputs[2])
    allowed = {'art','docs','generated','m1_native','src','tests','tools'}
    with zipfile.ZipFile(outputs[1], 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(ROOT.rglob('*')):
            if not path.is_file(): continue
            rel = path.relative_to(ROOT)
            if any(part.startswith('.') or part=='__pycache__' for part in rel.parts): continue
            if len(rel.parts)>1 and rel.parts[0] not in allowed:
                if rel.parts[0]!='dist' or path.name not in (rom.name, 'metal-slug-gba-native-v0.5.144-wip.gba'): continue
            archive.write(path, 'metal-slug-native/'+str(rel))
    with zipfile.ZipFile(outputs[1]) as archive:
        assert archive.testzip() is None
    print(json.dumps({'artifacts':[{'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)} for p in outputs]},indent=2))


if __name__ == '__main__':
    main()
