"""Package the read-only v0.5.100 M1 post-helicopter diagnosis.

Inputs are the two completed measurement ZIPs, never a running job's files.
This does not modify or rebuild cartridge code.
"""
from pathlib import Path
import base64
import csv
import hashlib
import html
import io
import json
import statistics
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/heli100'
DEST = ROOT / 'docs/heli-after-fps-0.5.100'
DEST.mkdir(exist_ok=True)
RELEASE = ROOT.parent / 'release'
HZ = 16777216
LCD_CYCLES = 280896

def csv_rows(z, name):
    return list(csv.DictReader(io.StringIO(z.read(name).decode())))

with zipfile.ZipFile(WORK / 'run-aim.zip') as play, zipfile.ZipFile(WORK / 'profile.zip') as profile:
    commits = csv_rows(play, 'commits.csv')
    traces = [json.loads(x) for x in play.read('trace.jsonl').decode().splitlines()]
    calls = csv_rows(profile, 'calls.csv')
    groups = csv_rows(profile, 'groups.csv')
    frames = csv_rows(profile, 'frames.csv')
    def at(frame):
        return max((r for r in commits if int(r['lcd']) <= frame), key=lambda r: int(r['lcd']))
    def window(start, end):
        first, last = at(start), at(end)
        count = int(last['commits']) - int(first['commits'])
        seconds = (end - start) * LCD_CYCLES / HZ
        return dict(lcd_start=start, lcd_end=end, commits=count, seconds=seconds,
                    fps=count/seconds, camera_start=int(first['camera_x']), camera_end=int(last['camera_x']))
    windows = [window(4800,5400), window(5540,5660), window(5700,6300), window(6000,6600)]
    profile_data = []
    for w, start, end in [(0,5100,5220),(1,5540,5660),(2,6000,6120)]:
        selected = [r for r in calls if int(r['window']) == w]
        total = sum(int(r['cycles']) for r in groups if int(r['window']) == w)
        bg = sum(int(r['cycles']) for r in groups if int(r['window']) == w and r['group'] == 'background_composition')
        video = [r for r in selected if r['function'] == 'native_video_prepare']
        rebuild = [r for r in selected if r['function'] == 'native_background_prepare' and r['start_bg'] == '0' and r['result'] == '1']
        profile_data.append(dict(window=w, lcd_start=start, lcd_end=end,
            bg_share_percent=100*bg/total, completed_video_calls=len(video),
            video_cpu_ms=statistics.mean(int(r['non_interrupt_cycles'])*1000/HZ for r in video),
            video_wall_ms=statistics.mean(int(r['wall_cycles'])*1000/HZ for r in video),
            full_rebuild_cpu_ms=statistics.mean(int(r['non_interrupt_cycles'])*1000/HZ for r in rebuild) if rebuild else None))
    trace_by_lcd = {t['lcd_frame']: t for t in traces}
    pairs = [('tick','world.ticks'),('camera','world.weapons.camera_x'),('commits','video.commits'),('late','video.late_transfers')]
    matched = [f for f in frames if int(f['lcd']) in trace_by_lcd]
    mismatches = [(f['lcd'],a) for f in matched for a,b in pairs if int(f[a]) != trace_by_lcd[int(f['lcd'])][b]]
    assert not mismatches, mismatches
    probes = {r.split()[2]: int(r.split()[0],16) for r in (WORK/'profile-probe.txt').read_text().splitlines()}
    ram = play.read('ewram.bin')
    live_at = probes['address.backgrounds.live'] - 0x02000000
    live = ram[live_at:live_at+896]
    hs = struct.unpack_from('<2048H', ram, probes['address.backgrounds.hash']-0x02000000)
    state = dict(hash_empty=hs.count(0), hash_tombstone=hs.count(65535),
                 live_new=sum(bool(x&2) for x in live), live_old=sum(bool(x&1) for x in live))
    report = dict(date='2026-11-02', version='0.5.100', emulator='mGBA 0.10.2 HLE',
        rom_sha256=hashlib.sha256((ROOT/'dist/metal-slug-gba-native-v0.5.100.gba').read_bytes()).hexdigest(),
        cartridge_code_changed=False, state_injection=False,
        route='Actual setKeys only; auto continue. Template142 exits at HP400 via T42_EXIT; helicopter defeat is NOT verified.',
        fps_definition='commits delta / (LCD delta * 280896 / 16777216)', windows=windows,
        profile=profile_data, replay_shared_samples=len(matched), replay_state_mismatches=0,
        execution=json.loads(play.read('execution.json')), final_trace=traces[-1], final_cache=state,
        root_cause='896 BG tile capacity plus protection of both preceding and new frame tiles causes retained prepare to fail and full rebuild every tick. At camera1660, fresh viewport also exceeds896.',
        source_locations=['src/background_video.c:26-48','src/background_video.c:97-108','src/native_video.c:20-35','src/background_video.h:6-7'])
    (ROOT/'generated/heli-after-fps-0.5.100.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    for name in ['inputs.csv','commits.csv','entities.csv','trace.jsonl','execution.json','ewram.bin','iwram.bin']:
        (DEST/name).write_bytes(play.read(name))
    for name in ['calls.csv','cache.csv','groups.csv','symbols.csv','frames.csv','blank.csv','emulator.log']:
        (DEST/('profile-'+name)).write_bytes(profile.read(name))
    from PIL import Image
    pictures = []
    for n in [3600,6000]:
        image = Image.open(io.BytesIO(play.read(f'frame-{n:06d}.ppm')))
        png = io.BytesIO(); image.save(png,format='PNG')
        (DEST/f'frame-{n}.png').write_bytes(png.getvalue())
        pictures.append(base64.b64encode(png.getvalue()).decode())

for name in ['followthrough.c','native_diagnosis.c','probe.txt','profile-probe.txt','symbols.txt','offsets.c','bg_offsets.c']:
    data=(WORK/name).read_bytes()
    if name=='native_diagnosis.c':
        data=data.replace(b'unmodified v0.5.99 ROM',b'unmodified v0.5.100 ROM')
    (DEST/name).write_bytes(data)

(DEST/'README.ko.md').write_text('''# M1 헬기 이후 FPS 진단 재현

대상은 SHA256 8251e595d6ca63176e277ea2346898c8a27b8d32bfe93fbe977af3b6838ea2b6의 v0.5.100 ROM이다.
제공 주소는 해당 ROM/ELF 전용이다. mGBA0.10.2 HLE를 사용했으며 ROM 또는 RAM을 패치하지 않았다.
호스트 실행 속도가 아닌 GBA 16777216Hz·LCD280896사이클당 화면 커밋을 집계한다.

followthrough.c는 실제 setKeys 입력만 쓴다. 명령 인자는 ROM, probe.txt, LCD수, 출력폴더, mode다.
모드8로 15000LCD를 요청했으나 LCD6655의 0x90000500 오류로 종료됐다. 자동 컨티뉴를 사용했다.
모드8의 조준 조건은 다른 gunship 엔티티이며 이 M1의 template142에는 발동하지 않았다.
실제 입력은 inputs.csv가 확정 기록이다. template142의 HP는400을 유지했고 T42_EXIT로 철수했다.
헬기 격파나 캠페인 클리어 결과로 해석하지 않는다.

native_diagnosis.c는 같은 inputs.csv를 재생한다. LCD5100..5220,5540..5660,6000..6120에서
ARM/Thumb 명령을 계측하며 각 창 앞60LCD를 워밍업한다. IRQ를 제외한 함수 비용과 전체 사이클 비중을 구분한다.
이전099의 하드코딩 DMA 주소 후크는 비활성화했다. profile-blank.csv는 실제 forced-blank 변화를 기록한다.
BG 진입/반환 때 전체 해시와 live bits를 읽었다. 정상 실행과 프로파일 실행의102공통시점에서
tick/camera/commits/late 네 값 모두 일치했다.

libmgba 헤더와 공유라이브러리를 설치한 환경에서, 이 폴더를 cwd로 실행한다.

```sh
cc -O2 -I/path/to/mgba/include followthrough.c -L/path/to/mgba/lib -lmgba -o followthrough
cc -O2 -I/path/to/mgba/include native_diagnosis.c -L/path/to/mgba/lib -lmgba -o diagnosis
mkdir replay profile
LD_LIBRARY_PATH=/path/to/mgba/lib ./followthrough /path/to/v0.5.100.gba probe.txt 15000 replay 8
LD_LIBRARY_PATH=/path/to/mgba/lib ./diagnosis /path/to/v0.5.100.gba profile-probe.txt symbols.txt inputs.csv profile 6120
```

첫 실행은 알려진 게임 오류로 종료코드5를 반환한다. 이를 테스트 성공으로 보지 않는다.
profile-calls.csv는 반환까지 관측한 호출만 포함한다. 창 경계의 미완료 호출은 평균에서 제외된다.
profile-groups.csv는 창 전체 사이클을 기록한다. 캐시 보호 수명과 현재 화면 자체의 용량 초과를 함께 수정해야 한다.
원본75% 배율·위4/아래4크롭·정확한 색·물리 좌표를 유지한다. 이번 작업은 원인 진단이며 수정판 ROM은 생성하지 않았다.
''')

rows=''.join(f'<tr><td>{label}</td><td>{w["camera_start"]} → {w["camera_end"]}</td><td>{w["commits"]} / {w["seconds"]:.3f}초</td><td><strong>{w["fps"]:.2f}fps</strong></td></tr>' for label,w in zip(['헬기 철수 직전 · LCD4800–5400','진행 재개 · LCD5540–5660','급락 유지 · LCD5700–6300','이후 숲 · LCD6000–6600'],windows))
doc='''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>M1 헬기 이후 FPS 급락 원인 — v0.5.100</title>
<style>body{font:16px/1.7 system-ui,sans-serif;color:#18222c;background:#f3f5f7;margin:0}main{max-width:980px;margin:auto;padding:40px 24px 70px}h1{font-size:32px;line-height:1.35;margin:10px 0 20px}h2{margin-top:34px;font-size:22px}.tag{font-size:13px;color:#506473}.lead{font-size:19px;border-left:5px solid #b6522c;padding:15px 20px;background:#fff}table{border-collapse:collapse;width:100%;background:#fff;font-size:15px}th,td{text-align:left;padding:12px;border-bottom:1px solid #dbe0e5}th{background:#e7edf1}figure{margin:0}img{width:100%;image-rendering:pixelated;border-radius:6px}figcaption{font-size:14px;color:#536373}.frames{display:grid;grid-template-columns:1fr 1fr;gap:22px}code{font-family:ui-monospace,monospace;font-size:.9em}small{color:#526575}li{margin:8px 0}@media(max-width:620px){.frames{grid-template-columns:1fr}main{padding:24px 14px}table{font-size:12px}th,td{padding:7px}h1{font-size:26px}}</style>
<main><div class="tag">2026-11-02 · mGBA 0.10.2 · 기존 v0.5.100 ROM 그대로 측정</div>
<h1>M1 헬기 이후, 배경 캐시가 한계에 도달한다</h1>
<p class="lead"><strong>27.18 → 2.79fps 급락을 재현했다.</strong><br>BG 896타일 한도에 가까운 화면에서 이전 화면과 새 화면의 타일을 함께 보호해 캐시 갱신이 실패한다. 복구 경로가 매 프레임 배경을 전부 다시 만들면서 약249ms의 CPU 시간을 쓴다. 더 진행하면 새 화면 자체도 한도를 넘어 중단된다.</p>
<h2>실제 화면 갱신률</h2><table><thead><tr><th>실행 구간</th><th>카메라 X</th><th>화면 커밋 / 경과시간</th><th>평균</th></tr></thead><tbody>ROWS</tbody></table>
<p><small>커밋 증가량 ÷ (LCD 증가량 × 280896 ÷ 16777216). 호스트 실행 속도나 단순 시뮬레이션 호출 수가 아니다. 기존24.43fps 측정은 헬기 등장·전투까지였으며, 이 이후 구간은 포함하지 않았다.</small></p>
<div class="frames"><figure><img alt="M1 헬기 전투 중 실제 화면" src="data:image/png;base64,PIC0"><figcaption>LCD3600 · 헬기 등장·전투 구간</figcaption></figure><figure><img alt="헬기 철수 이후 FPS가 급락한 숲 구간 실제 화면" src="data:image/png;base64,PIC1"><figcaption>LCD6000 · 카메라1659 · FPS 급락 구간</figcaption></figure></div>
<h2>오류가 반복되는 과정</h2>
<ol><li>이전 화면이 <strong>891 / 896타일</strong>을 차지한다. 여유는5개다.</li><li><code>background_video.c</code>의 <code>intern()</code>은 이전 화면과 새 화면의 타일을 모두 교체 금지한다. 신규 타일이 들어오면 두 집합의 합계가896에 도달해 <code>NBG_TILES=5</code>를 반환한다.</li><li><code>native_video.c</code>의 복구 경로는 타일·팔레트 캐시를 비우고 전체 배경을 다시 합성한다. 카메라가1659에 머물러도 자동 애니메이션 등 갱신으로 매 틱 반복된다.</li><li>전체 합성과 대량 VRAM 전송이 반복된다. 늦은 전송 누계는3→60회, 이번 실행의 흰 LCD프레임은59/6517개였다.</li></ol>
<h2>CPU 계측으로 확인</h2><table><thead><tr><th>측정 항목</th><th>직전 창 · LCD5100–5220</th><th>급락 창 · LCD6000–6120</th></tr></thead><tbody>
<tr><td>전체 화면 준비 · IRQ 제외 CPU</td><td>7.44ms</td><td>280.49ms</td></tr>
<tr><td>전체 화면 준비 · IRQ 포함 경과시간</td><td>9.66ms</td><td>312.66ms</td></tr>
<tr><td>전체 사이클 중 배경 합성</td><td>0.06%</td><td>73.50%</td></tr>
<tr><td>성공한 BG 전체 재생성 · IRQ 제외 CPU</td><td>없음 · 캐시 재사용</td><td>248.71ms</td></tr></tbody></table>
<p>24fps를 내려면 한 화면의 전체 시간이41.67ms 이하여야 한다. 이 구간은 배경 전체 재생성만으로 그 예산의 약6배를 쓴다. 진입 직후의 다른 계측 창에서는 배경이 전체 사이클의79.91%였다.</p>
<h2>별도로 해결해야 할 실제 용량 초과</h2>
<p>LCD6655 / tick2120 / 카메라1660에서 <code>0x90000500</code>으로 멈췄다. 이때 재초기화된 캐시의896개 타일 모두가 새 화면에서 사용 중이며, 이전 화면 보호는 빈 타일0 한 개뿐이었다. 즉 이전 화면 보호 수명만 수정해도 마지막 중단은 남을 수 있다. <strong>현재 화면의 타일 수용량도 함께 해결해야 한다.</strong></p>
<p>실패 시 해시는 빈 슬롯1152개·삭제 표시0개였다. 따라서 이번 급락은 해시 삭제 표시 누적으로 탐색이 길어진 문제가 아니다. 팔레트 수정 번호도1019로 유지됐고, 적·효과 등 엔티티 수는최대48이었다. 직접 계측에서 지배적인 비용은 BG 재생성이었다.</p>
<h2>수정 순서와 검증 범위</h2>
<ol><li>새 화면에서 실제 재사용할 타일을 먼저 식별하고, 이전 화면 타일의 교체 금지 수명을 바로잡는다. 화면에 쓰는 타일을 성급히 덮어써 그래픽을 깨뜨리지 않도록 검증한다.</li><li>896개를 초과하는 실제 화면의 타일 배치·공유 방식을 함께 보완한다. 무작정 상수를 늘리면 VRAM 맵 영역을 침범하므로 별도 설계가 필요하다.</li><li>매 프레임 전체 재생성이 사라진 상태에서 변경 타일만 전송하고, 헬기 이후 구간의24fps·흰 화면·픽셀 동등성을 다시 검증한다.</li></ol>
<p>버튼 입력과 컨티뉴만 사용했으며 상태·카메라·HP 주입은 없다. 이번 헬기(template142)는 HP400을 유지한 채 <code>T42_EXIT</code>로 철수했다. <strong>격파 검증으로 기록하지 않는다.</strong> 기록 입력을 재생한 프로파일은 정상 실행과102공통시점의 tick·카메라·커밋·늦은 전송 값이 모두 일치했다.</p>
<p>이번 작업은 원인 진단이다. 게임 코드·ROM·75% 그래픽·위4/아래4 크롭·물리 좌표는 변경하지 않았다. 수정 완료나 실기 검증으로 표시하지 않는다.</p>
<p><small>ROM SHA256: <code>8251e595d6ca63176e277ea2346898c8a27b8d32bfe93fbe977af3b6838ea2b6</code><br>재현 자료: 입력567행, 화면 커밋·엔티티·RAM 최종 상태, CPU 호출·그룹·캐시 로그 및 mGBA 호스트 진단 코드.</small></p></main></html>'''
doc=doc.replace('ROWS',rows).replace('PIC0',pictures[0]).replace('PIC1',pictures[1])
local_report=ROOT/'docs/heli-after-fps-0.5.100.ko.html'
local_report.write_text(doc)
release_report=RELEASE/'metal-slug-gba-v0.5.100-after-helicopter-diagnosis.ko.html'
release_report.write_text(doc)
evidence=RELEASE/'metal-slug-gba-v0.5.100-after-helicopter-evidence.zip'
with zipfile.ZipFile(evidence,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(DEST.iterdir()): z.write(p,p.name)
    z.write(ROOT/'generated/heli-after-fps-0.5.100.json','summary.json')
    for name in ['background_video.c','background_video.h','native_video.c']:
        z.write(ROOT/'src'/name,'cartridge-source/'+name)
    z.write(Path(__file__),'analyze_heli_after.py')
print(json.dumps({'windows':windows,'profile':profile_data,'final_cache':state,
                  'report':str(release_report),'evidence':str(evidence)},ensure_ascii=False))
