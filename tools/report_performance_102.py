#!/usr/bin/env python3
"""Package the measured 0.5.102 candidate; never substitute an exploratory ROM."""
import base64
import csv
import difflib
import hashlib
import html
import io
import json
import shutil
import struct
import zipfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build/perf102'
DOC = ROOT/'docs/performance-0.5.102'
ROM_SHA = '3b010d95b09ea7ce9720fb13651a36ffa791a6fd02aa31ce157802429c109fca'
HZ, LCD = 16777216, 280896


def read_json(z, name):
    return json.loads(z.read(name))


def interval(rows):
    first, last = rows[0], rows[-1]
    seconds = (int(last['lcd'])-int(first['lcd']))*LCD/HZ
    count = int(last['commits'])-int(first['commits'])
    return dict(lcd_start=int(first['lcd']), lcd_end=int(last['lcd']),
                commits=count, seconds=seconds, fps=count/seconds)


def main():
    build = json.loads((ROOT/'generated/native-build.json').read_text())
    assert (ROOT/'VERSION').read_text().strip() == build['version'] == '0.5.102'
    assert hashlib.sha256(Path(build['rom']).read_bytes()).hexdigest() == build['rom_sha256'] == ROM_SHA
    assert hashlib.sha256(Path(build['elf']).read_bytes()).hexdigest() == build['elf_sha256']
    DOC.mkdir(parents=True, exist_ok=True)
    z = zipfile.ZipFile(BUILD/'release-play.zip')
    summary = read_json(z, 'summary.json')
    assert summary['rom_sha256'] == ROM_SHA and summary['exit_code'] == 0
    commits = list(csv.DictReader(io.StringIO(z.read('commits.csv').decode())))
    active = [r for r in commits if r['playing']=='1' and r['presentation']=='0' and r['demo']=='0']
    active = active[next(i for i,r in enumerate(active) if int(r['live0'])):]
    scenes = {i:interval([r for r in active if int(r['scene'])==i]) for i in (0,1)}
    trace = [json.loads(s) for s in z.read('trace.jsonl').decode().splitlines()]
    transitions = [r for i,r in enumerate(trace) if i and any(r[k]!=trace[i-1][k] for k in
                   ['session.state','world.results.progress','world.weapons.scene_number'])]
    transition_fields = ['lcd_frame','world.ticks','world.results.progress','world.weapons.scene_number','session.state']
    transitions = [{k:r[k] for k in transition_fields} for r in transitions]
    assert any(r['session.state']==2 and r['world.results.progress']==0 for r in transitions)
    assert any(r['world.results.progress']==1 and r['world.weapons.scene_number']==1 for r in transitions)
    frontend_z = zipfile.ZipFile(BUILD/'release-frontend.zip')
    assert read_json(frontend_z,'build.json')['rom_sha256'] == ROM_SHA
    frontend = read_json(frontend_z,'summary.json')
    snapshots_z = zipfile.ZipFile(BUILD/'release-snapshots.zip')
    snapshots = read_json(snapshots_z,'summary.json')
    assert all(v['exit_code']==0 for v in snapshots.values())
    fixed_bytes = struct.unpack('<22I',snapshots_z.read('4800/offsets.bin'))[-1]
    with zipfile.ZipFile(ROOT/'build/heli101/final-play.zip') as old:
        baseline = read_json(old,'summary.json')
    (DOC/'baseline-0.5.101-summary.json').write_text(json.dumps(baseline,indent=2)+'\n')

    profile_z = zipfile.ZipFile(BUILD/'profile-release.zip')
    groups = list(csv.DictReader(io.StringIO(profile_z.read('groups.csv').decode())))
    calls = list(csv.DictReader(io.StringIO(profile_z.read('calls.csv').decode())))
    profiles = []
    for window,start in enumerate((7000,11500,25000)):
        selected = [r for r in groups if int(r['window'])==window]
        total = sum(int(r['cycles']) for r in selected)
        timing = {}
        for name in ['native_session_frame','native_video_prepare','native_background_prepare','native_object_scene_append']:
            values = [int(r['non_interrupt_cycles'])*1000/HZ for r in calls
                      if int(r['window'])==window and r['function']==name]
            timing[name] = dict(calls=len(values),mean_non_interrupt_ms=sum(values)/len(values))
        profiles.append(dict(lcd_start=start,lcd_end=start+120,
                             group_percent={r['group']:int(r['cycles'])*100/total for r in selected},timing=timing))

    tradeoffs = [
        '최근접 활성 배경 최대 3층. 용량·팔레트 압박 시 2층까지 줄이며 해당 world 연결 동안 유지한다. 마지막 후보는 1층 축소를 사용하지 않는다.',
        '배경 자동 텍스처 애니메이션을 정지한다. 카메라 스크롤과 명시적인 지형 변경·파괴 쓰기는 계속한다.',
        '수면 반사 그림을 제거하고, 지정한 탄피·피·연기·물보라·잔해 등은 객체 번호 기준 4개 중 1개만 그린다.',
        '151번 계열의 비충돌 잔상 자식 생성은 팔레트 주기 3회 중 1회로 줄인다. 실제 발사체의 궤적·공격은 유지한다. 이 변경은 장식 객체 할당과 수명을 줄이므로 전체 객체 목록 동일성을 주장하지 않는다.',
        'HUD 탄약·수류탄 숫자의 E/F 팔레트 점멸을 없앤다. 숫자와 갱신 시점은 유지한다.',
        'OBJ 16팔레트 초과 때만 보호하지 않은 배우 팔레트 쌍을 RGB555 채널당 최대 5/31 오차로 병합한다. 병합 결과 재병합은 금지한다. HUD·플레이어/팔·PROJECTILE·Slug·아이템·상호작용 객체는 보호한다. ENEMY 종류의 적 탄환까지 모두 보호된다는 뜻은 아니다.',
    ]
    palette_policy = 'Exact retained BG/HUD RGB; on OBJ16-bank overflow only, unprotected actor pair merge with RGB555 per-channel error <=5, no cascading merges; protected player/arm/projectile/Slug/item/interaction palettes'
    scope = 'Input-only mGBA0.10.2 HLE: 36000 LCD frames, M1 results/clear then M2 entry and partial M2 play; not M2 clear, full campaign, helicopter defeat or physical2P verification'
    frontend_paths = ['logo/title','first demo and ranking before the known second-demo failure',
                      'tutorial/game','ranking/title','pause/resume','no-peer link cancel',
                      'M1 results/clear to M2 entry in continued play']
    report = dict(version='0.5.102',rom_sha256=ROM_SHA,elf_sha256=build['elf_sha256'],
                  verified_campaign_scope=scope,complete_port=False,m1_clear_verified=True,
                  target_fps=30,target_met=False,fixed_frame_bytes=fixed_bytes,
                  input_method='Automated setKeys only; read-only telemetry assists aim. No game RAM, camera, HP, progression or speed injection. Auto fire/jump/grenades/continue; not manual human play.',
                  performance=dict(overall=summary['active_play'],m1_active=scenes[0],m2_observed=scenes[1],
                                   ranges=summary['ranges'],white_frames=14,game_lcd_frames=35859,late_transfers=15,
                                   bg_backend='Up to3nearest source planes, persistent2plane pressure fallback; exact retained-layer pixels, static auto tiles, dynamic tile capacity, common-pan scroll cache and IWRAM coverage hotloops'),
                  lcd_clock=dict(cpu_hz=HZ,cycles_per_lcd=LCD,lcd_hz=HZ/LCD,nominal30_ceiling=HZ/LCD/2),
                  visual_tradeoffs=tradeoffs,object_palette_policy=palette_policy,
                  catalog_decode=dict(assets=192,decoded_samples=18871232,pcm_fnv32='730777df',faults=0),
                  audible_output=dict(samples_stereo=658350,rate=32768,seconds=658350/32768,peak=10368,audio_fault=0,
                                      limitation='Captured waveform, not a complete campaign listening audit'),
                  pcm_cache=dict(assets=168,bytes=4465596,sha256='344aece2305f66c339f6e8d10a0828a93928cb52f7d6c4326c58b480b416ea68',
                                 exact_differential_samples=1056000,pcm_fnv32='fdcda934'),
                  memory=dict(rom_bytes=build['rom_bytes'],ewram_used=262084,ewram_free=60,iwram_before_stack_used=26264,
                              iwram_before_reserved_stack_free=104,observed_main_stack=2624,main_stack_reserved=4096,
                              observed_irq_stack=288,irq_stack_reserved=2208,limitation='Observed maxima in this run, not a universal bound'),
                  snapshots=snapshots,scene_transitions=transitions,profiles=profiles,frontend_verified_paths=frontend_paths,
                  frontend_results=[dict(name=r['name'],exit_code=r['exit_code'],tick=r['last']['world.ticks'],
                                         error=f"0x{r['last']['app.error']:08x}") for r in frontend],
                  priority='Reach nominal30FPS: reduce capture/strip/palette CPU in dense M1 and background occlusion cost in M2; remaining14white frames, known second-demo error, fullcampaign and physical2P validation',
                  evidence='docs/performance-0.5.102/')
    (ROOT/'generated/play-validation-0.5.102.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    (DOC/'measurement.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')

    for name in ['release-play.zip','release-snapshots.zip','release-frontend.zip','profile-release.zip']:
        shutil.copy2(BUILD/name,DOC/name)
    for rel in ['regressions-cullfast.log','build-release.log','probe.log',
                'release-sanitize/renderer.log','release-sanitize/capacity.log',
                'sanitize-final/pressure.log','sanitize-final/integration.log']:
        target=DOC/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(BUILD/rel,target)
    shutil.copy2(ROOT/'generated/native-build.json',DOC/'build.json')
    (DOC/'fixtures').mkdir(exist_ok=True)
    for name in ['post-helicopter-100.bin','post-helicopter-100.json']:
        shutil.copy2(ROOT/'tests/fixtures'/name,DOC/'fixtures'/name)
    driver=(BUILD/'diagnosis-release.c').read_text()
    driver=driver.replace('unmodified v0.5.99 ROM','unmodified v0.5.102 ROM')
    (DOC/'diagnosis.c').write_text(driver)
    diff=[]
    with zipfile.ZipFile(BUILD/'baseline-code.zip') as old:
        names=set(old.namelist())
        for folder in ['src','tools']:
            names.update(str(p.relative_to(ROOT)) for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ('.c','.h','.S','.py'))
        for name in sorted(names):
            if name.startswith(('src/','tools/')) or name=='VERSION':
                before=old.read(name).decode(errors='replace').splitlines(True) if name in old.namelist() else []
                after=(ROOT/name).read_text(errors='replace').splitlines(True) if (ROOT/name).is_file() else []
                if before!=after:diff.extend(difflib.unified_diff(before,after,fromfile='0.5.101/'+name,tofile='0.5.102/'+name))
    (DOC/'changes.diff').write_text(''.join(diff))

    ranges = [('0–1535','camera_0_1536'),('1552–1659','camera_1552_1660'),('1660–1799','camera_1660_1800'),
              ('1800–1999','camera_1800_2000'),('2000–2399','camera_2000_2400'),('2400–2648','camera_2400_2649')]
    md_table='| M1 카메라 X | 0.5.101 | 0.5.102 |\n|---|---:|---:|\n'
    table_rows=''
    for label,key in ranges:
        a,b=baseline['ranges'][key]['fps'],summary['ranges'][key]['fps']
        md_table+=f'| {label} | {a:.2f}fps | {b:.2f}fps |\n'
        table_rows+=f'<tr><td>{label}</td><td>{a:.2f}</td><td>{b:.2f}</td><td>+{(b/a-1)*100:.0f}%</td></tr>'
    measure_note='같은 카메라 범위의 비교이며 전투·컨티뉴 체류 길이는 같지 않을 수 있다. 특히 2400–2648은 이전 ROM이 팔레트 오류로 중단된 구간이다. 이전 전체 평균 16.95fps는 더 짧은 M1 실행이므로 새 10분 평균과 같은 workload로 비교하지 않는다.'
    common = f'''# Metal Slug GBA Native 0.5.102 — 30fps 목표 성능 개선

목표는 게임 전반의 평균 명목 30fps이다. **이번 후보의 10분 자동 입력 플레이 평균은 {summary['active_play']['fps']:.2f}fps로, 목표는 아직 미달성이다.** M1 결과/클리어와 M2 진입을 확인했다. 전 캠페인 완료판은 아니다.

{md_table}
{measure_note}

36,000 LCD프레임 실행, 활성 구간 600.779초·12,930커밋. M1 관측 구간 {scenes[0]['fps']:.2f}fps, M2 관측 구간 {scenes[1]['fps']:.2f}fps. FPS는 커밋 증가량을 에뮬레이트된 LCD 시간으로 나눈 값이며 호스트 PC 속도가 아니다. GBA LCD {HZ/LCD:.5f}Hz의 반속 상한은 {HZ/LCD/2:.5f}fps이다. 프레임 복제나 에뮬레이터 배속으로 수치를 올리지 않았다.

자동 발사·점프·수류탄·컨티뉴를 사용한 입력-only mGBA 0.10.2 HLE 검증이다. 조준 판단에 읽기 전용 상태를 사용하며 setKeys 외 게임 메모리·HP·카메라·진행 상태는 바꾸지 않았다. 사람이 직접 조작한 실기 플레이와 구분한다. M1의 LCD15240/tick5295에서 RESULTS, LCD16140/tick5692에서 progress1/scene1/PLAY를 관측했다. 이 결과는 M1 헬기를 격파했다는 근거가 아니며 양의 HP 상태의 철수도 기록된다.

## 성능을 위해 바꾼 표현

'''+''.join('- '+x+'\n' for x in tradeoffs)+f'''
75% 배율·위 4/아래 4픽셀 크롭, 승인 player.tiles SHA는 유지한다. 원본 전체 화면 동일성은 주장하지 않는다. 1층만 남기는 시험안은 지형 가독성 손실 때문에 최종 후보에서 제외했다.

## 구현과 검증

- 배경의 같은 타일 원점/공통 스크롤 재사용, 완전 가림 8×8 검사와 내부 메모리 배치, FIX 유휴 메모리를 사용하는 배경 workspace를 적용했다. 화면 가장자리에서 새로 드러나는 픽셀을 건너뛰지 않는다.
- HUD의 정확한 색 재매핑·변경 글자 패킹·접미부 캐시, 빈 HUD 객체 생략과 OBJ 정렬을 개선했다. 숫자 갱신과 같은색 분리 후 페이드도 검사했다.
- 168개 짧은 효과음을 정확한 PCM16으로 미리 풀어 ROM에 4,465,596바이트를 추가했다. 음악·긴 소리·루프는 기존 IMA 경로를 사용한다. 음원 샘플은 손실을 더하지 않았다.
- 실제 LCD4800/7000/11500/25000의 유지한 BG와 HUD를 참조·VRAM·OAM·팔레트와 대조해 차이 0을 확인했다. 삭제한 배경 층이나 근사 병합한 배우 색까지 원본과 같다는 뜻은 아니다.
- 호스트: BG220/FIX90/팔레트1200프레임, strip488,332사례·9,391맵, 용량400사례×3모드, OBJ 압박·캐시·정렬·공통 충돌 경로 통과. ASan/UBSan 통과(컨테이너 ptrace 제약으로 누수 탐지는 비활성).
- 음향: 1,056,000샘플 차등 비교 일치, 전체192자산·18,871,232샘플 디코드 오류 0. 실제 출력20.09초·peak10368·audio.fault0. 전 캠페인 청취 검증은 아니다.
- 튜토리얼·랭킹·일시정지/복귀·상대 없는 링크 취소는 종료 코드0. 첫 데모 후 두 번째 데모 scene4/tick112의 기존 0x0b1e0000은 여전히 발생한다.
- 10분 경로 app.error0, 흰 LCD프레임14/35,859와 늦은 전송15는 남는다. M2 클리어 이후·다른 모든 보스·엔딩·실기2P는 미검증이다.

## 남은 CPU 비용

읽기 전용 명령 계측의 120 LCD프레임 창을 비교했다. 함수 시간은 IRQ를 뺀 CPU 시간이며 서로 포함되는 항목은 합산하면 안 된다. 그룹에서 actor-first HUD suffix는 video_other에 포함된다. DMA 전용 watch는 꺼져 있어 DMA 상세 비용을 주장하지 않는다.

| 창 | 세션/프레임 | 비디오 준비/프레임 | BG 준비/프레임 | 오디오 IRQ 비율 |
|---|---:|---:|---:|---:|
'''+''.join(f"| LCD{p['lcd_start']}–{p['lcd_end']-1} | {p['timing']['native_session_frame']['mean_non_interrupt_ms']:.2f}ms | {p['timing']['native_video_prepare']['mean_non_interrupt_ms']:.2f}ms | {p['timing']['native_background_prepare']['mean_non_interrupt_ms']:.2f}ms | {p['group_percent']['audio_irq']:.1f}% |\n" for p in profiles)+f'''
M1 혼잡 구간은 그림 수집·source strip 계산·객체 팔레트 매핑과 세션 처리가 다음 개선 대상이다. M2 계측 창에서는 배경 계산이 전체 사이클의 {profiles[2]['group_percent']['background_composition']:.1f}%를 차지했다. 장식 축소만으로 목표를 달성하지 못했으므로 반복 계산과 데이터 배치에 추가 개선이 필요하다.

## 빌드와 메모리

- ROM {build['rom_bytes']:,}바이트, SHA-256 `{ROM_SHA}`.
- ELF SHA-256 `{build['elf_sha256']}`.
- EWRAM262,084바이트(60 여유), IWRAM26,264바이트(예약 스택 전104 여유).
- 이번 경로 관측 스택 main2624/4096, IRQ288/2208. 다른 장면·2P의 최대 사용량 보장은 아니다.
- 승인 player.tiles SHA-256 `ca7febc19eeda7cf62af36f036cdb91f290bf8be8ac279344e03bf269c74be32`.

측정: `generated/play-validation-0.5.102.json`, 원자료/재현: `docs/performance-0.5.102/`, 남은 작업: `docs/remaining-work-0.5.102.ko.md`.
'''
    (ROOT/'docs/checkpoint-0.5.102.ko.md').write_text(common)
    backlog='''# 0.5.102 남은 작업

사용자 목표는 **게임 전반 평균 명목30fps**이다. 중요도가 낮은 그래픽·기능의 희생을 허용했다. 이번 10분 자동 입력 평균21.52fps이며 아직 목표에 못 미친다. M1 클리어·M2 진입은 확인했지만 전 캠페인 완료는 아니다.

| 우선순위 | 남은 작업 | 완료 기준 |
|---|---|---|
| 1 | M1 혼잡 구간의 그림 수집·source strip·팔레트 매핑·세션 비용 | LCD7000 창의 세션25.46ms·비디오17.37ms CPU를 축소. 실제 전투/입력/판정을 유지하며 같은 경로를 재측정 |
| 2 | M2 BG 가림·반복 타일 계산, 전반30fps | LCD25000 창 BG20.20ms·전체 사이클42.4% 개선. 평균 및 혼잡 구간을 별도 제시하고 실제 M2 이후 경로 확대 |
| 3 | 흰 화면/전송 지연 | 14/35,859 흰 LCD프레임·late15 원인과 VRAM 전송 예산 개선 |
| 4 | 두 번째 자동 데모 scene4/tick112 | 기존world11/weapons30/error0x0b1e0000 해결 후 반복 데모 검증 |
| 5 | M2 클리어 이후 전 캠페인·보스·엔딩 | 입력만으로 M2~나머지 미션·보스·Slug·결과·1P/2P 엔딩 검증. M1 헬기 격파는 별도 확인 |
| 6 | 보스·2P 수용량·메모리 | OAM/타일/팔레트/행렬·메인/IRQ 스택 검증. 현재 EWRAM60바이트 여유, IWRAM 예약 스택 전104바이트 |
| 7 | 원본48슬롯 팔레트·과밀strip 의미 | 공유/이력/반납/재사용/실패·부분 열·이전SCB. 현재 성능용 색 병합과 구분해 표현·전투 영향을 검증 |
| 8 | 추가 승인 그래픽·축소 차이 | combat.tiles 몸/팔 바인딩과 원본 축소 마스크 비교 |
| 9 | 잔류 원본 예외3건 | 59D1·120D5·318A3/A4 의미 입증. 연결된 네이티브 인자 보정과 구분 |
| 10 | GBA2P 실기 | 두 기기 핸드셰이크·동기·전체 진행·분리/복구·음향/IRQ 검증. 취소 화면 통과는 실기2P 증거가 아님 |
| 11 | 음향 정밀도·추가 기능 | 개별YM채널·음높이 유지 템포·랜덤 음색, 전 구간 청취/응답, 영구 랭킹·언어/설정 |

이번 완료: 2648의 OBJ 팔레트 오류를 넘겨 M1 결과와 M2 진입, 배경3층/압박시2층·자동 텍스처 정지·반사 제거·일부 장식1/4·비충돌 잔상 생성1/3·HUD 숫자 점멸 제거, HUD 정확 색 캐시, 보호되지 않은 OBJ 색의 제한적 병합, 168SFX 정확 PCM 캐시, 가림·스크롤·strip·OBJ정렬 최적화. 75%와 위4/아래4 크롭·승인 player.tiles 유지.

같은 M1 카메라1552–1659는21.99→25.42fps, 2400–2648은4.45→15.21fps이다. 후자는 이전 오류 중단 때문에 관측 체류 길이가 다르다. 이전 전체16.95fps와 이번21.52fps는 길이·미션 구성이 다른 실행이다.

현재 자료: docs/checkpoint-0.5.102.ko.md, docs/performance-0.5.102.ko.html, generated/play-validation-0.5.102.json, docs/performance-0.5.102/. 실제 화면4곳과 호스트 renderer/palette/audio/strip/capacity/ASan 검증을 통과했다. 두 번째 데모 오류는 남는다. 1층 배경 시험안과 그25.94fps는 폐기했으며 현재 ROM 성능으로 인용하지 않는다.

구현·ROM 생성·입력-only 플레이·회귀·성능 검증은 승인되어 있으므로 다시 진행 허가를 받지 않는다. 원본 데이터는 장면 공통으로 변환하고 특정 플레이 경로를 배경 데이터로 굽지 않는다. 합성/호스트·자동 입력 에뮬레이터·실기2P를 구분한다. 표현을 희생하면 목록과 화면 근거를 남기고 핵심 전투·입력·충돌과 지형 가독성을 우선한다. 이 지침은 과거의 일괄적인 화면 삭제/색 근사 금지보다 우선한다.
'''
    (ROOT/'docs/remaining-work-0.5.102.ko.md').write_text(backlog)
    images=[]
    for frame,label in [(4800,'M1 · 헬기 이후'),(6900,'M1 · 혼잡 구간'),(12000,'M1 · 보스 전투'),
                        (16200,'M2 · 진입'),(18000,'M2 · 교량'),(24900,'M2 · 배경 가림 구간')]:
        img=Image.open(io.BytesIO(z.read(f'frame-{frame:06d}.ppm'))).convert('RGB')
        path=DOC/f'frame-{frame:06d}.png';img.save(path)
        data=base64.b64encode(path.read_bytes()).decode()
        images.append(f'<figure><img alt="{label}" src="data:image/png;base64,{data}"><figcaption>{label} · LCD {frame}</figcaption></figure>')
    trade_html=''.join('<li>'+html.escape(t)+'</li>' for t in tradeoffs)
    profile_html=''.join(f"<tr><td>{p['lcd_start']}–{p['lcd_end']-1}</td><td>{p['timing']['native_session_frame']['mean_non_interrupt_ms']:.2f}</td><td>{p['timing']['native_video_prepare']['mean_non_interrupt_ms']:.2f}</td><td>{p['timing']['native_background_prepare']['mean_non_interrupt_ms']:.2f}</td><td>{p['group_percent']['audio_irq']:.1f}%</td></tr>" for p in profiles)
    page=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Metal Slug Native 0.5.102 성능 검증</title>
<style>body{{margin:0;background:#10151d;color:#e9edf3;font:16px/1.75 system-ui,sans-serif}}main{{max-width:1060px;margin:auto;padding:52px 24px 72px}}h1{{font-size:clamp(28px,4vw,44px);line-height:1.3;letter-spacing:-1px}}h2{{font-size:24px;margin-top:46px}}p,li{{color:#c5ceda}}strong{{color:#fff}}.eyebrow{{color:#67dec4;letter-spacing:.12em;font-size:13px}}.badge{{display:inline-block;background:#422e1c;color:#ffd090;padding:5px 14px;border-radius:20px}}.metrics{{display:grid;grid-template-columns:repeat(3,1fr);gap:14px;margin:28px 0}}.metric{{background:#1a2330;padding:20px;border-radius:12px}}.metric b{{display:block;font-size:32px;color:#76e0c7}}.metric small{{color:#bac6d5}}table{{border-collapse:collapse;width:100%;font-variant-numeric:tabular-nums}}th,td{{padding:12px 15px;text-align:left;border-bottom:1px solid #344152}}th{{background:#202b3a}}td:not(:first-child),th:not(:first-child){{text-align:right}}.scroll{{overflow:auto}}.note{{font-size:14px;color:#a4b1c3}}.screens{{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:20px}}figure{{margin:0}}img{{display:block;width:100%;image-rendering:pixelated;border:1px solid #354458;box-sizing:border-box}}figcaption{{color:#a9bacb;font-size:14px;padding-top:6px}}code{{overflow-wrap:anywhere;font-size:13px;color:#b3dcd4}}ul{{padding-left:22px}}li{{margin:10px 0}}.warn{{border-left:3px solid #efb365;padding-left:18px}}@media(max-width:650px){{.metrics,.screens{{grid-template-columns:1fr}}main{{padding:28px 18px}}th,td{{padding:9px}}}}</style>
<main><div class="eyebrow">METAL SLUG · GBA NATIVE · 0.5.102</div><h1>혼잡 구간을 개선하고<br>M1 클리어까지 검증했습니다.</h1><div class="badge">평균30fps 목표는 미달성</div>
<p>중요도가 낮은 표현을 줄이고 배경·HUD·객체·효과음의 반복 계산을 개선했습니다. 아래는 최종 ROM의 측정값입니다. M1과 일부 M2를 포함하며, 전 캠페인 성능을 뜻하지 않습니다.</p>
<div class="metrics"><div class="metric"><b>21.52 fps</b><small>자동 입력 600.779초 · 12,930커밋</small></div><div class="metric"><b>15.21 fps</b><small>M1 X2400–2648 · 이전4.45</small></div><div class="metric"><b>M2 진입</b><small>M1 결과/클리어 확인 · M2 일부 진행</small></div></div>
<h2>같은 M1 카메라 범위 비교</h2><div class="scroll"><table><thead><tr><th>카메라 X</th><th>0.5.101 fps</th><th>0.5.102 fps</th><th>변화</th></tr></thead><tbody>{table_rows}</tbody></table></div><p class="note">{measure_note}</p>
<p>M1 관측 평균21.25fps, M2 관측 평균21.79fps. 16,777,216Hz / 280,896cycle LCD 기준으로 커밋 간격을 측정했습니다. 실제 반속 상한은 약29.86fps입니다. 프레임 복제·호스트 실행 속도를 FPS로 세지 않았습니다.</p>
<h2>최종 후보의 실제 화면</h2><div class="screens">{''.join(images)}</div><p class="note">mGBA 캡처 240×160. 75% 배율·위4/아래4픽셀 크롭 유지. 배경1층 시험안은 지형 가독성 손실로 제외하고 최소2층을 유지했습니다.</p>
<h2>성능을 위해 줄인 표현</h2><ul>{trade_html}</ul><p>적과 플레이어의 실제 공격·이동·충돌을 단순화하려는 변경은 넣지 않았습니다. 일부 장식 생성이 줄어 전체 객체 목록이나 원본 화면의 완전한 동일성은 주장하지 않습니다.</p>
<h2>검증 결과와 한계</h2><ul><li><strong>M1 결과/클리어 → M2 진입:</strong> LCD15240에서 RESULTS, LCD16140에서 progress1/scene1/PLAY. 양의 HP로 철수하는 헬기도 있어 헬기 격파 완료와 구분합니다.</li><li><strong>실제 화면4곳:</strong> LCD4800/7000/11500/25000에서 유지한 BG/HUD RGB·우선순위와 VRAM/OAM/팔레트 차이0. 삭제된 층·근사 배우 팔레트는 원본 동일성 검증 대상이 아닙니다.</li><li><strong>호스트 검사:</strong> BG220/FIX90/팔레트1200프레임, strip488,332사례, 용량400사례×3모드, OBJ 병합·캐시·정렬·충돌 경로와 ASan/UBSan 통과. 누수 탐지는 컨테이너 제약으로 비활성입니다.</li><li><strong>음향:</strong> 168개 짧은 효과음의 정확 PCM 사전 디코드, 1,056,000샘플 차등 일치. 전체192음원18,871,232샘플 디코드 오류0. 실제20.09초 출력 peak10368. 전 캠페인 청취 검증은 남습니다.</li><li><strong>화면 흐름:</strong> 튜토리얼·랭킹·일시정지/복귀·링크 취소 통과. 두 번째 데모 scene4/tick112의 기존0x0b1e0000은 남습니다.</li></ul>
<p class="warn">10분 경로의 app.error는0이지만 흰 LCD프레임14/35,859와 늦은 전송15가 남았습니다. M2 클리어 이후, 모든 보스와 엔딩, 실제 두 GBA의 링크 플레이는 아직 검증하지 않았습니다.</p>
<h2>30fps까지 남은 병목</h2><div class="scroll"><table><thead><tr><th>LCD 계측 창</th><th>세션 ms</th><th>비디오 ms</th><th>BG ms</th><th>오디오 IRQ</th></tr></thead><tbody>{profile_html}</tbody></table></div><p class="note">함수당 평균 IRQ 제외 CPU 시간. BG는 비디오에 포함되어 합산하면 안 됩니다. 오디오 IRQ는 전체 계측 사이클의 비율입니다. actor-first HUD는 별도 FIX 그룹이 아닌 video_other에 잡힙니다. DMA 상세 watch는 비활성입니다.</p>
<p>M1에서는 그림 수집·strip 처리·팔레트 매핑과 세션 처리, M2에서는 배경 가림 계산(해당 창 전체42.4%)을 더 줄여야 합니다. 이어서 흰 전송 프레임, 두 번째 데모 오류, 나머지 캠페인·실기2P를 검증해야 합니다.</p>
<h2>재현 가능한 빌드</h2><p>최종 ROM31,663,236바이트. EWRAM262,084바이트(60 여유), IWRAM26,264바이트(예약 스택 전104 여유). 이 경로의 스택 관측값은 메인2624/4096, IRQ288/2208이며 모든 장면의 상한 보장은 아닙니다.</p><p class="note">자동 setKeys 플레이와 읽기 전용 텔레메트리. 게임 RAM·HP·진행 상태 주입 없음. 사람의 수동 플레이·실기 검증과 구분합니다. 원자료: release-play.zip, release-snapshots.zip, release-frontend.zip, profile-release.zip. 전체 소스의 docs/performance-0.5.102/에 재현 명령과 검사 로그를 포함했습니다.</p><p>ROM SHA-256<br><code>{ROM_SHA}</code></p><p>승인 player.tiles SHA-256<br><code>ca7febc19eeda7cf62af36f036cdb91f290bf8be8ac279344e03bf269c74be32</code></p></main></html>'''
    (ROOT/'docs/performance-0.5.102.ko.html').write_text(page)
    repro='''# 0.5.102 최종 후보 증거

최종 ROM SHA는 measurement.json/build.json에 있다. release-play.zip만 최종 10분 측정이다. 다른 탐색 후보의 결과와 혼합하지 않는다. 최종 목표30fps는 미달성이다.

## 재현

프로젝트 루트에서 ARM GCC와 mGBA0.10.2 개발 파일을 준비한다. 경로를 환경에 맞춰 지정한다.

```sh
python3 tools/build_native.py --cc /path/to/arm-none-eabi-gcc --incremental
python3 tools/native_probe.py --cc /path/to/arm-none-eabi-gcc --elf build/native/native.elf
python3 tools/prepare_followthrough_probe.py --cc /path/to/arm-none-eabi-gcc --output build/repro102/probe
python3 tools/validate_heli_performance.py --output build/repro102/play --probe build/repro102/probe/probe.txt --mgba-prefix /path/to/mgba/usr --frames 36000 --mode 8
python3 tools/performance_regressions.py
python3 tools/verify_native_snapshot.py --cc /path/to/arm-none-eabi-gcc --mgba-prefix /path/to/mgba/usr --rom dist/metal-slug-gba-native-v0.5.102.gba --inputs build/repro102/play/inputs.csv --probe build/repro102/probe/probe.txt --frame 25000 --output build/repro102/snapshot25000
```

화면 비교는4800/7000/11500/25000에서 시행했다. 빌드/주소/헤더가 바뀌면 probe와 화면 fixture를 다시 만든다. 다른 빌드의 ELF offset을 사용하지 않는다.

읽기 전용 profiler: docs/performance-0.5.102/diagnosis.c를 호스트cc -O2, mGBA include/lib와 -lmgba로 컴파일한다. 인자는 ROM, probe.txt, symbols.txt, 기록 inputs.csv, 출력 디렉터리, 25120 순서다. lib/x86_64-linux-gnu를 LD_LIBRARY_PATH로 지정한다. LCD7000/11500/25000의120프레임 창을 계측한다. DMA 전용 watch는 비활성이다. 게임 메모리 수정은 하지 않는다.

ASan/UBSan은 performance_regressions.py의 renderer/object-pressure/integration 구성과 tests/background_capacity.c를 -O1 -g -fsanitize=address,undefined로 빌드했다. ASAN_OPTIONS=detect_leaks=0으로 실행했다. capacity는 renderer 구성의 tests/render_cache_equivalence.c 대신 tests/background_capacity.c를 사용하고, 실행 인자로 tests/fixtures/post-helicopter-100.bin을 준다. 이 합성 검사의 원본 캡처·형식·SHA는 같은 디렉터리의 JSON에 있다. 증거 묶음의 fixtures/에도 복사했다. 원검사 로그도 포함했다.

## 묶음

- release-play.zip: 36,000 LCD프레임의 inputs/commits/trace/entities/RAM/스크린샷/오디오/요약. ROM/ELF는 별도 배포와 source 빌드를 사용한다.
- release-snapshots.zip: 실제4화면의 BG/HUD 참조 비교와 committed VRAM/OAM/palette/RAM.
- release-frontend.zip: 첫 데모·두 번째 데모 오류·튜토리얼·랭킹·일시정지·링크 취소 입력과 상태.
- profile-release.zip: 함수·그룹·명령·프레임 계측. BG/비디오와 IRQ 시간은 구분한다.
- changes.diff: 이전0.5.101의 src/tools/VERSION에 대한 최종 코드 변경.
- regressions-cullfast.log, release-sanitize/, sanitize-final/: 호스트 검증. 회귀 검사 후 추가 변경은 이미 테스트한 coverage 쓰기 함수의 IWRAM 배치이며 최종 ARM ROM 실제4화면 비교를 통과했다.

한계: 자동 입력/합성 테스트와 실제 두 GBA의 실행은 다르다. 완전한 원본 시뮬레이션·이미지 동일성, M2 이후 클리어, 전 캠페인30fps를 입증하지 않는다.
'''
    (DOC/'README.ko.md').write_text(repro)
    readme=ROOT/'README.ko.md'
    marker='이하 기록은 이전 구현 이력이다.'
    history=readme.read_text();history=history[history.index(marker):]
    readme.write_text('''# Metal Slug GBA Native 0.5.102 — 평균30fps 목표 성능 개선

최종 10분 자동 입력 평균 **21.52fps**, M1 클리어와 M2 진입 확인. **30fps 목표는 아직 미달성**이다. 같은 M1 X1552–1659는21.99→25.42fps, X2400–2648은4.45→15.21fps. 전 캠페인 평균이나 완료판으로 표시하지 않는다.

배경3층/압박시2층·자동 텍스처 정지·반사 제거·일부 장식 축소·제한적 OBJ 색 병합을 적용했고 HUD/객체/가림/효과음 처리를 최적화했다. 75%·위4/아래4픽셀 크롭·승인 player.tiles 유지. 실제4화면의 유지한 BG/HUD·VRAM/OAM/팔레트 차이0, 호스트 회귀 검사 통과. 흰 프레임14/35,859·두 번째 데모 오류·M2 이후 전 캠페인·실기2P는 남는다.

최신 문서: docs/checkpoint-0.5.102.ko.md, docs/remaining-work-0.5.102.ko.md, docs/performance-0.5.102.ko.html, generated/play-validation-0.5.102.json. 재현 명령과 원자료는 docs/performance-0.5.102/README.ko.md에 있다.

'''+history)
    agents=ROOT/'AGENTS.md'
    text=agents.read_text()
    checkpoint='- Latest checkpoint2026-11-02 v0.5.102: nominal30FPS target NOTmet. Final selected ROM3b010d95b09ea7ce9720fb13651a36ffa791a6fd02aa31ce157802429c109fca input-only36000LCD/600.779active seconds averages21.522FPS, M1clear/results thenM2entry verified. M1interval21.253,M2observed21.790; camera1552..1659=25.416,2400..2648=15.209 versus10121.986/4.449. Different run lengths; no fullcampaign claim. Three nearestBG planes, persistent2plane pressure fallback (not1), frozen decorative auto tiles, no reflections, markedcosmetics1/4draw, noncolliding151trail1/3spawn, stableHUDcounterpalette. On OBJoverflow unprotectedactorpalette merge<=5/31perRGB555channel, protectedHUD/player etc and no cascading. Exact168SFX PCM cache4.466MB, retainedBG/HUD exact snapshot diffs0 atLCD4800/7000/11500/25000. Host/ASan regressions pass. Error0on10minroute; white14/35859late15 remain. Knownseconddemo scene4tick1120x0b1e0000 persists. M2clear/fullcampaign/helicopterdefeat/physical2P NOTverified. EWRAM262084(60free),IWRAM26264(104before reservedstack),observedmain2624/4096IRQ288/2208. Preserve75percent4+4crop/playerSHA. Next optimize capture/strip/palette/logic in denseM1 and culling inM2; do not cite rejected one-layer25.94FPS. Current docs/checkpoint-0.5.102.ko.md, docs/remaining-work-0.5.102.ko.md, docs/performance-0.5.102/. Previous restrictions on visual removal/approximation are superseded by user performance authorization; disclose tradeoffs and protect core combat/readability. Testing/ROM generation already authorized.\n\n'
    if checkpoint not in text:agents.write_text(text.replace('# Source-based native port\n\n','# Source-based native port\n\n'+checkpoint,1))
    manifest={str(p.relative_to(DOC)):hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(DOC.rglob('*')) if p.is_file() and p.name!='manifest.json'}
    (DOC/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    release=ROOT.parent/'release';release.mkdir(exist_ok=True)
    archive=release/'metal-slug-gba-native-v0.5.102-evidence.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as out:
        for path in sorted(DOC.rglob('*')):
            if path.is_file():out.write(path,'performance-0.5.102/'+str(path.relative_to(DOC)))
        for path in [ROOT/'docs/performance-0.5.102.ko.html',ROOT/'docs/checkpoint-0.5.102.ko.md',ROOT/'docs/remaining-work-0.5.102.ko.md']:
            out.write(path,path.name)
    print(json.dumps(dict(rom_sha256=ROM_SHA,m1=scenes[0],m2=scenes[1],fixed_frame_bytes=fixed_bytes,
                          report=str(ROOT/'docs/performance-0.5.102.ko.html'),evidence=str(archive),evidence_bytes=archive.stat().st_size),ensure_ascii=False))


if __name__=='__main__':
    main()
