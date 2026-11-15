#!/usr/bin/env python3
"""Scoped M1 visibility release: actual ROM captures, telemetry and limitations."""
import ast,base64,csv,hashlib,json,subprocess
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parents[1];B=R/'build';O=R/'docs/m1-0.5.117'
O.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def trace(n):return [json.loads(s) for s in (B/n/'trace.jsonl').read_text().splitlines()]
def metrics(n):
 d=B/n;t=trace(n);rows=list(csv.DictReader((d/'commits.csv').open()))
 a=[x for x in rows if x['playing']=='1' and x['presentation']=='0' and x['demo']=='0' and x['scene']=='0']
 a=a[next(i for i,x in enumerate(a) if int(x['live0'])):]
 def fps(q):return (int(q[-1]['commits'])-int(q[0]['commits']))/((int(q[-1]['lcd'])-int(q[0]['lcd']))*280896/16777216) if len(q)>1 else None
 white=[list(map(int,x)) for x in csv.reader((d/'white-frames.csv').open())] if (d/'white-frames.csv').exists() else []
 pressure=[list(map(int,x)) for x in csv.reader((d/'oam-pressure.csv').open())]
 return dict(lcd_frames=t[-1]['lcd_frame']+1,final_scene=t[-1]['world.weapons.scene_number'],m1_fps=fps(a),village_fps=fps([x for x in a if 2649<=int(x['camera_x'])<3000]),errors=max(x['app.error'] for x in t),audio_errors=max(x['audio.fault'] for x in t),m1_white=sum(x[2]==0 for x in white),m2_white=sum(x[2]==1 for x in white),m1_late=max(int(x['late']) for x in a),capacity_retries=max(x['video.objects.budget_retries'] for x in t),max_strip_drops=max(x.get('video.objects.strips.record_drops',0) for x in t),max_key_drops=max(x.get('video.objects.strips.key_drops',0) for x in t),oam_cycle_upper_bound=max(x[-1] for x in pressure),prestage_frames=max(x.get('m1.staged_frames',0) for x in t),prestage_bytes=max(x.get('m1.staged_bytes',0) for x in t))
build=read(B/'bugs117-final/build.json');assert sha(B/'bugs117-final/rom.gba')==build['rom_sha256']
runs={n:metrics('bugs117-final-'+n) for n in ['normal','burst9','stress3','mount','water']}
for n,v in runs.items():
 assert v['errors']==v['audio_errors']==v['m1_white']==v['m1_late']==0
 if n!='water':assert v['final_scene']==1
gpu={n:{k:read(B/('bugs117-gpu-'+n)/(f+'.json')) for k,f in [('objects','object-hardware'),('background','m1-hardware')]} for n in ['opening','water','village','prestage','dense']}
for case in gpu.values():
 for part in case.values():assert all(v==0 for k,v in part.items() if 'mismatched' in k)
summary=dict(version=build['version'],rom_bytes=build['rom_bytes'],rom_sha256=build['rom_sha256'],rom_remaining_bytes=33554432-build['rom_bytes'],runs=runs,before=metrics('bugs116-verified-normal'),gpu=gpu,assets=read(B/'bugs117-final/asset-integrity.json'),memory=dict(ewram_free=812,iwram_free_before_reserved_stacks=16,m1_overlay_bytes=13592,overlay_capacity=13640),full_reference_verified=False,limitations=['Scoped visibility review, not complete Neo Geo behavior equivalence.','The underwater actor matches scene/camera and final rising pose, but its jump timing/height and other combat actors differ from Neo Geo.','30 FPS is not reached; previously missing objects now add real rendering load.','On final OBJ capacity retry, decorative explosion drawings may be omitted; simulation and gameplay objects remain.','M2 transition white frames, physical GBA/link, all branches and full campaign remain unverified or unresolved.'])
(O/'validation.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
specs=[
 ('rock','첫 HMG 상자 뒤 바위 발판',429,'opening','rock remains visible after animation END',[
  'neogeo-visibility117/burst-001460.png','bugs117-before-normal/frame-000768.ppm','bugs117-final-normal/frame-000771.ppm']),
 ('water','수중에서 뛰어오르는 적',1805,'scroll','underwater soldier rising, last pose held, damage timer zero',[
  'neogeo-visibility117-water/burst-003988.png','bugs117-before-water/frame-004610.ppm','bugs117-final-water/frame-004632.ppm']),
 ('village','마을 입구 발판·구조물',2336,'transition','intact village entry platforms before demolition',[
  'neogeo-visibility117/burst-004445.png','bugs117-before-normal/frame-005689.ppm','bugs117-final-normal/frame-005791.ppm'])]
captures={}
for tag,label,camera,phase,event,paths in specs:
 for side,src in zip(['ng','before','after'],paths):
  name=tag+'-'+side;Image.open(B/src).convert('RGB').save(O/(name+'.png'))
  captures[name]=dict(image=name+'.png',sha256=sha(O/(name+'.png')),capture=src,camera_x=camera,camera_y=144,event_state=event)
(O/'captures.json').write_text(json.dumps(captures,ensure_ascii=False,indent=2)+'\n')
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',19)
small=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',15)
canvas=Image.new('RGB',(1496,1240),'#101c29');draw=ImageDraw.Draw(canvas)
draw.text((20,16),'M1 visibility repair | v0.5.117-m1',font=font,fill='white')
draw.text((20,47),'Actual Neo Geo capture / same camera. Yellow boxes mark the reported missing objects.',font=small,fill='#b6c9d9')
boxes={'rock':[(83,87,138,129)]*3,'water':[(144,43,174,84),(148,53,178,100),(148,53,178,100)],'village':[(154,56,185,94)]*3}
for row,(tag,label,camera,phase,event,paths) in enumerate(specs):
 y=118+row*362
 for col,side in enumerate(['ng','before','after']):
  x=20+col*492;im=Image.open(O/(tag+'-'+side+'.png')).convert('RGB')
  if side=='ng':im=im.resize((480,336),Image.Resampling.NEAREST).resize((240,168),Image.Resampling.NEAREST).crop((0,4,240,164))
  im=im.resize((480,320),Image.Resampling.NEAREST);canvas.paste(im,(x,y))
  rect=boxes[tag][col];draw.rectangle((x+rect[0]*2,y+rect[1]*2,x+rect[2]*2,y+rect[3]*2),outline='#ffda62',width=3)
  draw.text((x,y-28),f'{["Neo Geo (75% preview)","Before: v0.5.116","After: v0.5.117"][col]} | X={camera}',font=small,fill='white')
draw.text((20,1212),'75% preview uses the approved scaling/crop. Combat timing and underwater actor height differ; see report.',font=small,fill='#b6c9d9')
canvas.save(O/'M1-visibility-v0.5.117.png')
def uri(name):return 'data:image/png;base64,'+base64.b64encode((O/(name+'.png')).read_bytes()).decode()
def trio(tag):return '<div class="trio">'+''.join(f'<figure><img src="{uri(tag+"-"+s)}"><figcaption>{l} · X={captures[tag+"-"+s]["camera_x"]}</figcaption></figure>' for s,l in [('ng','실제 네오지오 / 원본 320×224'),('before','수정 전 v0.5.116 / 240×160'),('after','수정 후 v0.5.117 / 240×160')])+'</div>'
labels={'normal':'일반 진행','burst9':'집중 파괴','stress3':'파괴·가솔린 경로','mount':'슬러그 탑승','water':'수중 적 정지 관찰'}
rows=''.join(f'<tr><td>{labels[n]}</td><td>{v["lcd_frames"]:,}</td><td>{v["m1_fps"]:.2f}</td><td>{v["errors"]}</td><td>{v["m1_white"]}</td><td>{"M2 진입" if n!="water" else "재현 구간 관찰"}</td></tr>' for n,v in runs.items())
page=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>M1 발판·수중 적 표시 수정 — v0.5.117</title><style>
body{{margin:0;background:#eff3f6;color:#1d2c39;font:16px/1.75 system-ui,sans-serif}}main{{max-width:1160px;margin:auto;padding:30px 24px}}h1{{font-size:30px;line-height:1.4}}h2{{font-size:23px;margin-top:32px}}.lead{{background:#dbeee5;border-left:5px solid #277659;padding:18px}}.note{{background:#fff0d8;padding:16px}}.trio{{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}}figure{{margin:10px 0}}img{{width:100%;image-rendering:pixelated}}figcaption,small{{font-size:13px;color:#526878}}table{{width:100%;border-collapse:collapse;background:white}}td,th{{padding:9px;border-bottom:1px solid #d4dee5;text-align:left}}code{{overflow-wrap:anywhere}}@media(max-width:700px){{.trio{{grid-template-columns:1fr}}main{{padding:20px 14px}}}}</style><main>
<small>2026-11-03 · M1 한정 수정 · mGBA 0.10.2 / MAME 0.264</small><h1>첫 상자 뒤 발판과 수중 적의 표시 누락 수정</h1>
<p class="lead">애니메이션이 끝난 뒤 마지막 그림을 다시 제출하지 않던 공통 처리 오류를 수정했습니다. 첫 HMG 상자 뒤 바위, 수중 적의 점프 마지막 자세, 마을 입구의 발판·구조물을 실제 네오지오 실행 화면과 대조했습니다. 네 가지 입력 전용 경로에서 M1 보스 종료·결과 화면·M2 진입을 확인했고, M1 오류와 흰 화면은 0건입니다.</p>
<h2>1. 첫 HMG 상자 뒤 바위 발판</h2>{trio('rock')}
<p>충돌체는 남아 있었지만 그림이 사라졌습니다. 원본 <code>SIGN_A $2949AE</code>는 마지막 그림 뒤의 대기 명령을 거쳐 종료 명령에 도달합니다. 네오지오는 종료 시 마지막 그림을 계속 표시합니다. 수정 전 GBA는 이 시점부터 그리기 목록에 넣지 않았습니다.</p>
<h2>2. 물속에서 뛰어오르는 적</h2>{trio('water')}
<p>원본 <code>BARREL_RISE $2C7626</code>의 마지막 그림 <code>$2C764E</code>에서도 같은 문제가 발생했습니다. GBA 월드 틱 2,091, 카메라 X=1,805에서 수정 전에는 적이 보이지 않고 수정 후에는 점프 자세가 유지됩니다. 해당 시점의 피격 타이머는 양쪽 모두 0이므로 피격 점멸과 구분됩니다.</p>
<p class="note">네오지오와 GBA는 카메라와 상승 마지막 자세를 맞춰 비교했습니다. 적의 세로 위치와 점프 시작 시점은 다릅니다. 이 비교는 표시 유지 동작에 대한 검증이며 이동 궤적·AI·프레임 전체의 동일성 검증은 아닙니다. 수정 전후 GBA는 같은 월드 틱·좌표·상태를 사용합니다.</p>
<h2>3. 마을 입구 발판과 구조물</h2>{trio('village')}
<p>정지된 발판·지지 구조물에도 같은 종료 처리 경로가 적용됩니다. 카메라 X=2,336의 파괴 전 구간에서 수정 전 빠져 있던 구조가 복원됩니다. 주변 적·포로·공격 이력은 원본과 달라 별도의 동등성 판정을 내리지 않았습니다.</p>
<h2>공통 원인과 GBA 자원 처리</h2>
<p>네오지오 <code>$029334</code>는 마지막 그리기 명령을 다시 읽고 일반 그리기 <code>$028DFE</code>로 들어갑니다. GBA의 <code>AN_END</code>도 같은 표시 유지 규칙을 따르도록 변경했습니다. 기존 미션 배너의 개별 우회 처리는 제거해 중복 제출을 방지했습니다. 121회 연속 그리기, 특수→일반 그리기 전환, 배너 복귀, 빈 그림 유지 회귀 검사를 통과했습니다.</p>
<p>누락됐던 구조물이 복원되자 마을의 OBJ 타일 메모리가 실제로 부족해졌습니다. 큰 그림을 빈 공간이 적은 GBA 타일 묶음으로 사전 배치하고, 추가로 134개 그림에 용량 부족 시 사용할 조밀한 배치를 ROM에 저장했습니다. 기본 배치의 OAM 수를 늘리지 않으며, 조밀한 배치는 남는 OAM을 더 사용하는 대신 타일 공간을 절약합니다. 전체 9,391개 그림의 픽셀 해시는 기존과 같고 배경·폰트·승인된 플레이어 그림을 유지합니다.</p>
<p class="note">마지막 용량 초과 재시도에서는 장식용 폭발 그림 일부를 생략할 수 있습니다. 연산·수명·피해·파괴 처리는 계속 실행하며 적·탄환·아이템·발판은 이 생략 대상에 넣지 않습니다. 집중 파괴 경로에서 해당 재시도와 조밀한 배치를 실제 사용했습니다. 이는 기존 연기·잔해 생략에 추가되는 제한적 시각적 타협입니다.</p>
<p>마을 파괴 프레임의 분산 DMA는 약 28KB 전송에서도 VBlank를 넘길 수 있었습니다. 사용 중인 배경과 다음 배경 모두 쓰지 않는 VRAM 구역에 OBJ를 미리 준비하는 기준을 32KB에서 24KB로 조정했습니다. 수정 중 발견된 M1 흰 화면 1프레임을 제거했고, 실제 사전 준비 19,872바이트가 포함된 캡처에서도 전송 데이터 불일치가 없었습니다.</p>
<h2>실행 결과와 성능</h2><table><tr><th>경로</th><th>LCD 프레임</th><th>M1 평균 FPS</th><th>오류</th><th>M1 흰 화면</th><th>도달 지점</th></tr>{rows}</table>
<p>일반 경로는 v0.5.116의 {summary['before']['m1_fps']:.2f} → {runs['normal']['m1_fps']:.2f} FPS, 마을 X=2,649..2,999는 {summary['before']['village_fps']:.2f} → {runs['normal']['village_fps']:.2f} FPS입니다. 이번 수정은 보이지 않던 객체를 복원하므로 그리기 부하가 늘었습니다. <strong>30 FPS 목표는 달성하지 못했습니다.</strong> 같은 입력 정책이지만 전투·프레임 진행 차이가 있어 완전히 같은 작업량 비교는 아닙니다. 수중 적 관찰은 M1 전체 평균과 직접 비교할 수 없습니다.</p>
<p>입력은 버튼만 사용했으며 RAM·카메라·체력·진행을 강제로 변경하지 않았습니다. 다섯 커밋 직후 캡처의 BG 맵·타일, OBJ 타일·팔레트·OAM은 실제 VRAM과 불일치 0바이트였습니다. 애니메이션, 캡처, 팔레트 및 타일 압박 회귀 검사는 ASan/UBSan을 통과했습니다. 승인된 75% 축소와 상하 4+4픽셀 크롭은 유지합니다.</p>
<h2>배포 정보와 남은 범위</h2><p><strong>metal-slug-gba-native-v0.5.117-m1.gba</strong><br>{build['rom_bytes']:,} bytes · {build['rom_bytes']/1e6:.2f} MB · {build['rom_bytes']/1048576:.2f} MiB<br>SHA-256: <code>{build['rom_sha256']}</code></p>
<p>EWRAM 여유 812바이트, 예약 스택을 제외한 IWRAM 여유 16바이트입니다. 메모리 배치가 바뀌었으므로 이전 빌드의 세이브스테이트 대신 새로 부팅해 확인해야 합니다.</p>
<p>이번 원본 대조는 제보된 세 구간에 한정됩니다. 전체 M1 원본 동등성 게이트는 미통과이며, 모든 분기·전 미션·실물 GBA·실물 2P 링크는 검증하지 않았습니다. M2 전환에서는 기존 흰 화면 1~2프레임이 남습니다. 입력·원본 캡처·ROM 해시·검사 결과는 별도 검증 자료에 보존했습니다.</p></main></html>'''
(O/'M1-visibility-v0.5.117.html').write_text(page)
reference=R.parents[1]/'neogeo-reference/mslug.zip'
manifest=dict(gba_rom_sha256=build['rom_sha256'],neogeo_rom_sha256=sha(reference),scope='Three M1 visibility cases; full equivalence not claimed',cases=[])
for tag,label,camera,phase,event,paths in specs:
 c=dict(mission=0,phase=phase,reference_kind='neogeo_rom_capture',review='passed',approved_adaptations=['75% approved scaling and 4+4 pixel crop','Existing BG palette and approved font adaptations'],unresolved_differences=['Surrounding combat state and timing differ; visibility review only']+(['Underwater actor height and jump timing differ'] if tag=='water' else []))
 for key,side in [('neogeo','ng'),('gba','after')]:
  v=captures[tag+'-'+side];c[key]=dict(image=v['image'],sha256=v['sha256'],state={k:v[k] for k in ['camera_x','camera_y','event_state']}|{'scene':0})
 manifest['cases'].append(c)
(O/'reference-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
result=subprocess.run(['python3',str(R/'tools/check_reference_comparison.py'),'--candidate',str(B/'bugs117-final/rom.gba'),'--reference',str(reference),'--manifest',str(O/'reference-manifest.json'),'--missions','0'],capture_output=True,text=True)
assert result.returncode==1 and not json.loads(result.stdout)['reference_verified'];(O/'reference-gate.json').write_text(result.stdout)
print(json.dumps({'rom':build['rom_sha256'],'normal_fps':runs['normal']['m1_fps'],'m1_errors':0,'m1_white':0,'reference_gate':False},indent=2))
