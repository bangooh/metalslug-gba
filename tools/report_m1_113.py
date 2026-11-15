#!/usr/bin/env python3
"""Final M1 performance evidence and actual Neo Geo capture comparison."""
import argparse,base64,csv,hashlib,html,json
from pathlib import Path
from PIL import Image,ImageDraw
import numpy as np
R=Path(__file__).resolve().parents[1];B=R/'build';O=R/'docs/m1-0.5.113';O.mkdir(exist_ok=True)
p=argparse.ArgumentParser();p.add_argument('--run',default='perf113-memory');p.add_argument('--reviewed',action='store_true');args=p.parse_args();C=B/args.run
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def traces(p):return [json.loads(s) for s in (p/'trace.jsonl').read_text().splitlines()]
def metrics(p):
 rows=list(csv.DictReader((p/'commits.csv').open()));a=[r for r in rows if r['playing']=='1' and r['presentation']=='0' and r['demo']=='0' and r['scene']=='0']
 a=a[next(i for i,r in enumerate(a) if int(r['live0'])):]
 def interval(a):
  first,last=a[0],a[-1];lcd=int(last['lcd'])-int(first['lcd']);n=int(last['commits'])-int(first['commits'])
  return dict(lcd_start=int(first['lcd']),lcd_end=int(last['lcd']),commits=n,seconds=lcd*280896/16777216,fps=n/(lcd*280896/16777216))
 gaps=np.diff([int(r['lcd']) for r in a]);ms=gaps*280896/16777216*1000
 ranges={f'{lo}-{hi}':interval([r for r in a if lo<=int(r['camera_x'])<hi]) for lo,hi in [(0,1536),(1536,1660),(1660,2336),(2336,2649),(2649,3000),(3000,3848),(3848,4000)]}
 white=[list(map(int,r)) for r in csv.reader((p/'white-frames.csv').open())] if (p/'white-frames.csv').exists() else []
 t=traces(p);m2=next(r['lcd_frame'] for r in t if r['world.weapons.scene_number']==1)
 return dict(m1=interval(a),m1_camera_ranges=ranges,frame_interval_ms=dict(median=float(np.median(ms)),p95=float(np.percentile(ms,95)),p99=float(np.percentile(ms,99)),max=float(max(ms))),
  intervals_at_least_100ms=int(sum(ms>=100)),intervals_at_least_8lcd=int(sum(gaps>=8)),m1_white_frames=sum(r[2]==0 for r in white),m1_late_transfers=max(int(r['late']) for r in a),
  max_game_error=max(r['app.error'] for r in t),max_audio_fault=max(r['audio.fault'] for r in t),max_audio_clipped=max(r['audio.clipped'] for r in t),
  first_observed_m2_lcd=m2,all_white_frames=white,m1_palette_pressure_max_rgb555_channel_error=max(int(r['palette_error']) for r in a))
before=metrics(B/'m1-release');after=metrics(C);build=read(C/'build.json');assert sha(C/'rom.gba')==build['rom_sha256']
def hashes(path):return {int(r[1]):r[3] for r in csv.reader(path.open())}
old_hash=hashes(B/'perf113-baseline-hashes/frame-hashes.csv');new_hash=hashes(C/'frame-hashes.csv');shared=old_hash.keys()&new_hash.keys();diff=sorted(k for k in shared if old_hash[k]!=new_hash[k])
hash_check=dict(algorithm='FNV-1a 64-bit over all240x160 RGB pixels after each output commit',alignment='video.commits; sampled world.ticks can already belong to the next in-progress tick',commits_compared=len(shared),different_commits=diff,missing_baseline_commits=sorted(old_hash.keys()-new_hash.keys()),baseline_rom_sha256=read(B/'m1-release/build.json')['rom_sha256'],candidate_rom_sha256=build['rom_sha256'])
assert not diff and not hash_check['missing_baseline_commits']
iw=(C/'iwram.bin').read_bytes();sentinel=(0xa55a3cc3).to_bytes(4,'little')
def stack_used(lo,hi):return hi-next((i for i in range(lo,hi,4) if iw[i:i+4]!=sentinel),hi)
sections={}
for line in build['section_sizes'].splitlines():
 q=line.split()
 if len(q)==3 and q[0].startswith('.') and q[1].isdigit():sections[q[0]]=(int(q[1]),int(q[2]))
memory=dict(ewram_bytes=sections['.data'][0]+sections['.bss'][0],ewram_free=262144-sections['.data'][0]-sections['.bss'][0],iwram_before_stack_free=0x03006700-sum(sections['.iwram']),main_stack_observed=stack_used(0x6700,0x7700),main_stack_reserved=4096,irq_stack_observed=stack_used(0x7700,0x7fa0),irq_stack_reserved=2208)
out=dict(version=build['version'],run=args.run,baseline=before,candidate=after,target_fps=30,target_met=after['m1']['fps']>=30,improvement_percent=100*(after['m1']['fps']/before['m1']['fps']-1),frame_hash_comparison=hash_check,memory=memory,rom_sha256=build['rom_sha256'],rom_bytes=build['rom_bytes'])
(O/'metrics.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
ng={int(r[1]):r for r in csv.reader((B/'neogeo-m1-matched/reference-captures.csv').open()) if int(r[3])==1}
gba={int(r[1]):r for r in csv.reader((C/'reference-captures.csv').open()) if int(r[3])==1}
trace={r['lcd_frame']:r for r in traces(C)}
def result_frame(tick):return min((r for r in trace.values() if r['world.weapons.scene_number']==0 and r['world.weapons.camera_x']==3848 and (C/f'frame-{r["lcd_frame"]:06}.ppm').exists()),key=lambda r:abs(r['world.ticks']-tick))['lcd_frame']
spec=[('opening','미션 시작',0,128,'mission-start-banner','neogeo-m1-matched/matched-001080.png',300),
 ('destruction','헬기 구간: 무너진 유적 유지',1536,144,'first-helicopter-arena-opening-ruins-destroyed',None,None),
 ('scroll','수상 마을 진입',2336,144,'stilt-house-entry',None,None),('scroll','마을 교전',2648,144,'village-arena',None,None),
 ('transition','마을 끝: 오두막 파괴 상태 미해결',2880,144,'last-stilt-house-water-visible','neogeo-m1-preserved-hut-aim/matched-005168.png',int(gba[2880][0])),('transition','폭포 배경',3328,112,'waterfall-train-wreck',None,None),
 ('transition','보스 앞 절벽',3648,32,'cliff-boss-approach',None,None),('boss','보스 전투 진입',3848,32,'boss-arena-active',None,None),
 ('results','포로 집계 화면',3848,32,'boss-defeated-prisoner-results','neogeo-m1-aim/neogeo-013200.png',result_frame(5442)),
 ('results','MISSION 1 COMPLETE',3848,32,'mission-complete-banner','neogeo-m1-aim/neogeo-013500.png',result_frame(5584))]
cases=[];sheet=Image.new('RGB',(980,len(spec)*368),'#151a22');draw=ImageDraw.Draw(sheet);sections_html=[]
for i,(phase,label,cx,cy,event,source,lcd) in enumerate(spec):
 if source is None:
  row=ng[cx];source='neogeo-m1-matched/matched-'+row[0].zfill(6)+'.png';assert int(row[2])==cy
  row=gba[cx];lcd=int(row[0]);assert int(row[2])==cy
 else:
  n=int(Path(source).stem.split('-')[-1]);root=B/source.split('/')[0]
  if Path(source).stem.startswith('matched-'):
   row=next(x for x in csv.reader((root/'reference-captures.csv').open()) if int(x[0])==n);assert (int(row[1]),int(row[2]))==(cx,cy)
  else:
   rows=list(csv.DictReader((root/'reference-state.csv').open()));row=next(x for x in rows if int(x['frame'])==n);assert (int(row['camera_x']),int(row['camera_y']))==(cx,cy)
  if lcd in trace:
   t=trace[lcd];assert(t['scene.camera.x']>>16,t['scene.camera.y']>>16)==(cx,cy)
  else:
   row=next(x for x in csv.reader((C/'reference-captures.csv').open()) if int(x[0])==lcd);assert (int(row[1]),int(row[2]))==(cx,cy)
 a=Image.open(B/source).convert('RGB');b=Image.open(C/f'frame-{lcd:06}.ppm').convert('RGB');names=[f'{i+1:02}-neogeo.png',f'{i+1:02}-gba.png'];a.save(O/names[0]);b.save(O/names[1])
 state={'scene':0,'camera_x':cx,'camera_y':cy,'event_state':event}
 cases.append(dict(mission=0,phase=phase,title=label,reference_kind='neogeo_rom_capture',review='passed' if args.reviewed else 'pending',review_scope='Static background coverage, terrain alignment and categorical event display only; not exact AI/input/animation equivalence.',unresolved_differences=[],
  approved_adaptations=['75% assets; center crop4+4 GBA pixels from224-line original','Existing background quantization, HUD15colors and enemy palette pressure; no additional reduction in113'],
  excluded_from_match=['Different input routes, scores, ammunition and animation phase','Existing regional language/free-play and bottom HUD safe-row policy'],
  neogeo=dict(image=names[0],sha256=sha(O/names[0]),state=state,capture_file=source,bios='euro-s1',emulator='MAME0.264'),
  gba=dict(image=names[1],sha256=sha(O/names[1]),state=state,lcd_frame=lcd,emulator='mGBA0.10.2')))
 if cx==2880:
  case=cases[-1];case['review']='blocked';case['unresolved_differences']=['Left hut roof/walls remain in GBA; absent in two actual Neo Geo input-only runs. Also present in112, so not introduced by113. Cause/identical damage-state reproduction still pending.']
  case['neogeo']['state']=dict(state,event_state=event+'-left-hut-destroyed');case['gba']['state']=dict(state,event_state=event+'-left-hut-intact')
 av=a.resize((240,168),Image.Resampling.NEAREST).crop((0,4,240,164));draw.text((8,i*368+5),f'{i+1:02} {phase.upper()} | camera {cx},{cy} | Neo Geo75% crop / GBA',fill='white')
 sheet.paste(av.resize((480,320),Image.Resampling.NEAREST),(0,i*368+32));sheet.paste(b.resize((480,320),Image.Resampling.NEAREST),(500,i*368+32))
 uri=lambda p:'data:image/png;base64,'+base64.b64encode(p.read_bytes()).decode()
 sections_html.append(f'<section><h2>{i+1:02}. {html.escape(label)}</h2><p>원본 좌표 {cx}, {cy} · {event}</p><div class="pair"><figure><figcaption>네오지오 원본320×224</figcaption><img src="{uri(O/names[0])}"></figure><figure><figcaption>GBA240×160 · LCD{lcd}</figcaption><img src="{uri(O/names[1])}"></figure></div></section>')
sheet.save(O/'comparison.png')
manifest=dict(version=build['version'],scope='M1 listed backgrounds and event displays only; separate112-vs113 full-commit pixel hash regression',gba_rom_sha256=build['rom_sha256'],neogeo_rom_sha256=sha(Path('/workspace/scratch/779e6ec69435/neogeo-reference/mslug.zip')),cases=cases)
(O/'reference-comparison.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
perf_rows=''.join(f'<tr><td>{k}</td><td>{before["m1_camera_ranges"][k]["fps"]:.2f}</td><td>{v["fps"]:.2f}</td></tr>' for k,v in after['m1_camera_ranges'].items())
stats=[('M1 평균fps',before['m1']['fps'],after['m1']['fps']),('p95 프레임 간격ms',before['frame_interval_ms']['p95'],after['frame_interval_ms']['p95']),('p99 프레임 간격ms',before['frame_interval_ms']['p99'],after['frame_interval_ms']['p99']),('최대 프레임 간격ms',before['frame_interval_ms']['max'],after['frame_interval_ms']['max']),('100ms 이상 간격 횟수',before['intervals_at_least_100ms'],after['intervals_at_least_100ms'])]
stats_rows=''.join(f'<tr><td>{name}</td><td>{x:.2f}</td><td>{y:.2f}</td></tr>' for name,x,y in stats)
body=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>M1 성능 개선 · v0.5.113</title><style>body{{font:16px/1.65 system-ui,sans-serif;background:#111822;color:#e4eaf1;max-width:1100px;margin:40px auto;padding:0 20px}}h1{{font-size:32px}}h2{{font-size:21px}}section{{border-top:1px solid #34404f;padding:18px 0}}.pair{{display:flex;gap:16px;align-items:start;flex-wrap:wrap}}figure{{margin:0;flex:1;min-width:260px}}img{{width:100%;image-rendering:pixelated}}figcaption,p{{color:#b9c6d5}}strong{{color:#ffe19b}}table{{border-collapse:collapse}}td,th{{text-align:left;padding:6px 24px 6px 0;border-bottom:1px solid #34404f}}code{{word-break:break-all}}</style>
<h1>M1 성능 개선 · v0.5.113-m1</h1><p>v0.5.112의 그래픽·게임 규칙·6kHz 오디오 설정을 유지하면서 반복 작업을 줄였습니다. 평균은 <strong>{before['m1']['fps']:.2f} → {after['m1']['fps']:.2f}fps, {out['improvement_percent']:.2f}% 개선</strong>입니다. 목표30fps에는 아직 도달하지 못했습니다.</p>
<p>입력만 사용하는 동일한 자동 조작 정책으로 20,000LCD 프레임을 실행했습니다. M1 시작→헬기→마을→보스 격파→포로 집계→MISSION1 COMPLETE→M2 진입을 확인했습니다. 메모리·체력·카메라 강제 변경은 없습니다. 실제 GBA·2P·다른 조작 경로·전체 미션 완료 검증은 아닙니다.</p>
<h2>측정 결과</h2><table><tr><th>항목</th><th>v0.5.112</th><th>v0.5.113</th></tr>{stats_rows}</table>
<p>M1 첫 실제 플레이 화면부터 마지막 M1 커밋까지의 경과 시간이며 결과 연출을 포함합니다. M2를 섞은 전체 실행 평균과 구분했습니다. 최대 간격{after['frame_interval_ms']['max']:.2f}ms가 남아 있어 모든 끊김을 해결한 상태는 아닙니다.</p>
<h2>화면과 안정성</h2><ul><li>기존 버전과 동일 출력 커밋{len(shared):,}개를 정렬하여240×160 전체RGB 화면 해시 비교: 불일치{len(diff)}. 순간 진행 중인 world.ticks 대신 출력 커밋 번호를 사용했습니다.</li><li>네오지오 실제 실행의 주요10개 장면을 별도 대조했습니다. 9개 장면의 제한된 배경·지형 연결·이벤트 표시를 확인했고, <strong>마을 끝 오두막1개 장면은 파괴 상태가 달라 검증 보류</strong>입니다. 원본 대조 전체 통과로 표시하지 않습니다.</li><li>카메라2880에서 원본은 왼쪽 오두막 지붕·벽이 사라졌고 GBA에는 남아 있습니다. 마을에서 수류탄을 사용하지 않은 두 번째 원본 실행에서도 차이가 확인됐습니다. 기존112에도 있는 차이이며, 원인 및 동일 피격 상태 재현은 아직 미해결입니다. 이전112 보고서의 해당 장면 통과 판정은 이번 보류 판정으로 정정합니다.</li><li>M1 오류{after['max_game_error']}, 흰 화면{after['m1_white_frames']}, 늦은 전송{after['m1_late_transfers']}. M2 진입 이후 기존 흰 화면2프레임·늦은 전송2회는 남아 있습니다.</li><li>오디오 오류{after['max_audio_fault']}, 클리핑{after['max_audio_clipped']}. 6kHz·효과음 최대4개 유지. 이번 작업에서 소리·장식 효과를 추가로 줄이지 않았습니다.</li></ul>
<h2>적용한 변경</h2><ol><li>팔레트 부족 시 같은 결과를 얻기 위해 반복하던 허용 오차별 탐색을 한 번의 최소 오차 탐색으로 변경.</li><li>HUD 중간OAM 구성·중복 초기화 제거. 바뀐 글자 사각형과 색 매핑만 재포장하며, 기존 팔레트 재사용 이력을 보존.</li><li>변경 세대와 실제VRAM 소유권이 일치하는 HUD 타일은 재전송하지 않음.</li><li>OBJ 빈VRAM 검색을 비트 단위 묶음 검색으로 변경. 최저 정렬 위치를 선택하는 기존 배치 규칙 유지.</li><li>BG 스크롤 시 겹치는 화면 타일을 재사용하고 새 가장자리만 계산. 타일 요청 중복 탐색 및 새 타일이 없는 경우의 빈 슬롯 계산 제거.</li><li>ARM IWRAM의 memcpy·memset을 다중 레지스터 전송으로 묶음. 실제ARM 실행29,232개 경우를 호스트libc와 대조.</li></ol>
<p>추가 팔레트 한 건 캐시 실험은 M1 평균 개선이 없어 제외했습니다. HUD 변경 후 헬기 한 프레임의 색상 차이가 발견되어 팔레트 이력 보존을 복구한 뒤 전체 화면 해시가 일치하는 최종 후보를 선택했습니다.</p>
<h2>M1 구간별 평균fps</h2><table><tr><th>원본 카메라X 범위</th><th>v0.5.112</th><th>v0.5.113</th></tr>{perf_rows}</table><p>범위에 들어온 첫 커밋부터 마지막 커밋까지 측정했습니다. 카메라가 되돌아오면 구간별 측정 시간이 겹칠 수 있습니다.</p>
<h2>남은 병목과 다음 순서</h2><p>마을 구간의 CPU 표본에서는 오디오 믹싱, OBJ 팔레트·VRAM 배치, 객체 그리기, 엔티티 갱신 비용이 분산되어 있었습니다. 다음 큰 변경 후보는 GBA용 객체 제출 데이터와 충돌 후보 목록 재구성입니다. 아직 적용하지 않았으며 AI 갱신 빈도·파편·색상·오디오 품질을 더 줄인 결과도 아닙니다. 평균30fps와 일정한 프레임 간격을 함께 확인해야 합니다.</p>
<h2>메모리와 검사</h2><p>EWRAM{memory['ewram_bytes']:,}/262,144바이트, 여유{memory['ewram_free']}바이트. 스택 앞 IWRAM 여유{memory['iwram_before_stack_free']}바이트. 센티널 관찰상 메인 스택{memory['main_stack_observed']}/{memory['main_stack_reserved']}바이트, IRQ{memory['irq_stack_observed']}/{memory['irq_stack_reserved']}바이트. 한 경로의 관찰값이며 최악 경로 보장은 아닙니다.</p><p>ASan/UBSan: 팔레트 탐색20,000개 차등 비교, VRAM 빈 구간20,000개 비교와 캐시 무효화, BG585,900타일·스크롤 검증, 타일 스트림, HUD 글자/색/저장 위치 이동 검증 통과.</p>
{''.join(sections_html)}<section><h2>배포 식별</h2><p>ROM{build['rom_bytes']:,}바이트 ({build['rom_bytes']/1000000:.2f}MB / {build['rom_bytes']/1048576:.2f}MiB), 32MiB 이내.</p><p>SHA-256 <code>{build['rom_sha256']}</code></p><p>원본 참조: MAME0.264 euro-s1 실제 ROM 캡처. GBA 실행: mGBA0.10.2. 기존75% 배율,4+4 GBA 픽셀 중앙 크롭, 색 양자화와 HUD/적 팔레트 제한은 그대로입니다.</p></section></html>'''
(O/'M1-performance-v0.5.113.html').write_text(body)
print(json.dumps(out,ensure_ascii=False,indent=2))
