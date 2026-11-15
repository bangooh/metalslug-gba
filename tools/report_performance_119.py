#!/usr/bin/env python3
"""Package the measured v119 result and scoped original-ROM comparisons."""
import base64, hashlib, json, shutil, subprocess
from pathlib import Path
from PIL import Image

R=Path(__file__).resolve().parents[1]
B=R/'build/perf119'
O=R/'docs/m1-0.5.119'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p): return json.loads(p.read_text())
report=read(O/'validation.json')
before=report['runs']['base-normal']; after=report['runs']['c-normal']
assert sha(B/'final/rom.gba')==report['rom_sha256']
cases=[('heli',1536,2096,1672,'scroll','metal prop damaged','metal prop destroyed'),
       ('village',2336,4445,5607,'transition','intact village huts and platforms','intact village huts and platforms'),
       ('destruction',2880,5220,7383,'destruction','right hut destruction in progress','right hut intact; different attack history')]
manifest={'gba_rom_sha256':report['rom_sha256'],
          'neogeo_rom_sha256':sha(R.parents[1]/'reference118/mslug.zip'),
          'scope':'M1 camera-matched visual review only; no full behavioral equivalence claim',
          'cases':[]}
for tag,camera,ng,gba,phase,ng_event,gba_event in cases:
    item={'mission':0,'phase':phase,'reference_kind':'neogeo_rom_capture',
          'review':'scoped','approved_adaptations':['approved75% scale and4+4px crop','existing BG quantization, HUD/font and effect adaptations'],
          'unresolved_differences':['Actors, attack history and timing are not synchronized; full-state equivalence is not established.']}
    for side,file,event in [('neogeo',B/'neogeo'/f'matched-{ng:06}.png',ng_event),
                            ('gba',B/'final-reference'/f'frame-{gba:06}.ppm',gba_event)]:
        name=f'{tag}-{side}.png';Image.open(file).convert('RGB').save(O/name)
        item[side]={'image':name,'sha256':sha(O/name),
                    'state':{'scene':0,'camera_x':camera,'camera_y':144,'event_state':event}}
    manifest['cases'].append(item)
(O/'reference-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
gate=subprocess.run(['python',str(R/'tools/check_reference_comparison.py'),
 '--candidate',str(B/'final/rom.gba'),'--reference',str(R.parents[1]/'reference118/mslug.zip'),
 '--manifest',str(O/'reference-manifest.json'),'--missions','0'],capture_output=True,text=True)
assert gate.returncode==1 and not json.loads(gate.stdout)['reference_verified']
(O/'reference-gate.json').write_text(gate.stdout)
for src,dst in [('reference-normal-contact.png','reference-comparison.png'),('results-contact.png','results-sequence.png')]:shutil.copy2(B/src,O/dst)
def img(name):return 'data:image/png;base64,'+base64.b64encode((O/name).read_bytes()).decode()
gain=(after['m1_fps']/before['m1_fps']-1)*100
rows=[]
for label,key in [('M1 전체 평균 · 결과 화면 포함','m1_fps'),('100ms 이상 갱신 지연 횟수','intervals_ge_100ms'),('99백분위 갱신 시간 (ms)','p99_interval_ms'),('최장 갱신 시간 (ms)','max_interval_ms')]:
    rows.append(f'<tr><td>{label}</td><td>{before[key]:.2f}</td><td>{after[key]:.2f}</td></tr>')
for label,key in [('마을 진입 · 카메라 X2336–2648','2336..2648'),('마을 파괴 · X2649–2999','2649..2999')]:
    rows.append(f'<tr><td>{label} (fps)</td><td>{before["camera_fps"][key]:.2f}</td><td>{after["camera_fps"][key]:.2f}</td></tr>')
html=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>M1 성능 개선 v0.5.119</title><style>body{{margin:0;background:#f0f3f5;color:#20303e;font:16px/1.75 system-ui,sans-serif}}main{{max-width:980px;margin:auto;padding:32px 20px}}h1{{line-height:1.3}}h2{{font-size:23px;margin-top:32px}}table{{border-collapse:collapse;width:100%;background:white}}td,th{{border-bottom:1px solid #d3dce1;padding:10px;text-align:left}}.lead{{padding:18px;background:#dceee5}}.note{{padding:16px;background:#fff0d7}}img{{max-width:100%;image-rendering:pixelated}}code{{overflow-wrap:anywhere;font-size:12px}}.muted{{color:#506574;font-size:14px}}</style>
<main><p class="muted">2026-11-03 · M1 성능 후보 · mGBA 0.10.2 / MAME 0.264</p>
<h1>배경의 중복 처리와 불필요한 전체 검사를 줄였습니다</h1>
<p class="lead">M1 평균 <b>{before['m1_fps']:.2f} → {after['m1_fps']:.2f}fps (+{gain:.2f}%)</b>.
마을 파괴 구간은 11.79 → 12.96fps, 100ms 이상 갱신 지연은 138 → 53회입니다. 30fps 목표에는 아직 도달하지 못했습니다.</p>
<table><thead><tr><th>같은 자동 플레이 경로</th><th>v118</th><th>v119</th></tr></thead><tbody>{''.join(rows)}</tbody></table>
<p class="muted">실제 플레이어가 활성화된 시점부터 M1 마지막 화면 갱신까지 측정했습니다. 결과 화면을 포함하고 M2는 제외합니다. 카메라별 수치는 양쪽 끝이 해당 구간 안에 있는 연속 갱신으로 계산합니다. 에뮬레이터의 호스트 실행 속도가 아닌 GBA LCD 주기 기준입니다. 초기 설정까지 포함한 평균은 22.81 → 23.64fps입니다.</p>
<h2>반영한 변경</h2>
<ol><li>파괴된 배경 셀만 기존 캐시에 적용하여 과거 지형 변경의 전체 재계산을 줄였습니다.</li>
<li>배경 타일 수용 여부를 검사하면서 모은 고유 타일 목록을 VRAM 배치에 재사용합니다.</li>
<li>스크롤 중 배경 전체를 검사하는 대신 새로 드러나는 가장자리만 갱신합니다.</li>
<li>스프라이트 표시 범위 계산에 ROM 표를 사용하고 그림 구성 정보 캐시를 32 → 64개로 늘렸습니다.</li>
<li>M1에서 쓰지 않는 HUD 함수가 빠른 메모리를 차지하지 않도록 배치를 정리했습니다.</li></ol>
<p>스프라이트 업로드 정렬 방식도 시험했지만 선택한 후보보다 느려 제외했습니다. 그래픽·음악·효과음 데이터와 게임 규칙은 이번 버전에서 바꾸지 않았습니다. 팔레트의 사전 배정 확대, 장식 생성량 조정, 결과 화면의 전용 BG 전환은 이번 완료 항목이 아닙니다.</p>
<h2>검증 결과와 한계</h2>
<ul><li>일반 진행 21,000 LCD 프레임, 집중 파괴 24,000프레임, 슬러그 탑승 21,000프레임 모두 M1 보스·결과 화면을 지나 M2에 진입했습니다.</li>
<li>이 세 경로의 게임 오류·오디오 오류·M1 흰 화면·M1 전송 초과는 모두 0건입니다. M2 전환의 흰 화면 2프레임은 남아 있습니다.</li>
<li>기준 롬과 M1의 같은 화면 갱신 번호 5,566개를 대조한 RGB 해시는 모두 같습니다. 진행 틱은 화면 제출과 시점이 달라 비교 키로 사용하지 않았습니다.</li>
<li>초반·수중 구간·마을에서 실제 전송 직후 캡처한 BG 타일/맵, OBJ 타일/OAM/팔레트는 모두 0바이트 불일치입니다.</li>
<li>지형·스크롤 976,500셀 대조, 타일 배치 5,000회 대조, 표시 범위 488,332건, 그림 구성 108,000건, 팔레트 압력 및 HUD 메모리 검사를 통과했습니다.</li>
<li>별도의 포로 99명·생존 슬러그 진단에서 결과 문자 버퍼가 13,632바이트까지 커져 이전 오류를 막는 제자리 변환 분기를 실제로 거쳤고 M2에 진입했습니다. 체력·포로 수·보스 패배를 주입한 검사로, 입력만 사용하는 플레이 증거와 구분합니다.</li></ul>
<p class="note">최장 끊김 234.4ms는 그대로입니다. 기존의 장식 생략을 동반한 용량 재시도도 일반 경로에서 8회로 같습니다. 실기 GBA·2인 링크·전체 미션 검증은 이번 범위에 포함되지 않습니다. 사용자가 보고한 결과 화면 이전 오류는 동일 오류 코드로 확정 재현되지 않았으므로, 이번 성능 수정으로 해결됐다고 단정하지 않습니다.</p>
<h2>실제 네오지오 실행 화면과 대조</h2>
<p>원본 ROM을 MAME에서 입력만으로 실행했습니다. 카메라 X/Y를 맞춘 헬기 구역·마을 진입·파괴 구간을 아래에 보존했습니다. 마을 입구의 지형과 발판을 확인했습니다. 공격 이력이 달라 장식물 및 오른쪽 집의 파괴 상태, 적·플레이어 위치와 효과 타이밍은 일치하지 않습니다. 이 자료는 전체 동작의 동일성 증명이 아니며 원본 비교 게이트는 미통과 상태입니다.</p>
<img alt="실제 네오지오와 GBA의 같은 카메라 비교" src="{img('reference-comparison.png')}">
<p class="muted">왼쪽은 실제 네오지오 320×224 캡처를 승인된 75% 크기와 상하 4픽셀씩 자르기로 정규화한 보기입니다. 원본 PNG도 검증 자료에 포함합니다. 이 보기의 확대는 비교를 위한 최근접 보간입니다.</p>
<h2>배포 파일</h2><p>metal-slug-gba-native-v0.5.119-m1.gba · {report['rom_bytes']:,}바이트 · 약 32.1MB / 30.62MiB</p>
<p>SHA-256<br><code>{report['rom_sha256']}</code></p>
<p class="muted">다음 우선순위는 마을의 실제 오브젝트·팔레트 구성 비용과 가장 긴 처리 지연입니다. 이번에 확인한 개선 폭을 30fps 달성으로 해석하면 안 됩니다.</p></main></html>'''
(O/'M1-performance-v0.5.119.html').write_text(html)
print(json.dumps({'report':str(O/'M1-performance-v0.5.119.html'),'reference_gate':False,'rom':report['rom_sha256']}))
