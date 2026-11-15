#!/usr/bin/env python3
"""M1 bug reproduction, actual Neo Geo comparisons, and final ROM evidence."""
import base64,csv,hashlib,html,json,subprocess,zipfile
from pathlib import Path
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parents[1];B=R/'build';O=R/'docs/m1-0.5.116';O.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def trace(name):return [json.loads(s) for s in (B/name/'trace.jsonl').read_text().splitlines()]
def metrics(name):
 d=B/name;rows=list(csv.DictReader((d/'commits.csv').open()));t=trace(name)
 a=[x for x in rows if x['playing']=='1' and x['presentation']=='0' and x['demo']=='0' and x['scene']=='0'];a=a[next(i for i,x in enumerate(a) if int(x['live0'])):]
 def fps(q):return (int(q[-1]['commits'])-int(q[0]['commits']))/((int(q[-1]['lcd'])-int(q[0]['lcd']))*280896/16777216) if len(q)>1 else None
 white=[list(map(int,x)) for x in csv.reader((d/'white-frames.csv').open())] if (d/'white-frames.csv').exists() else []
 pressure=[list(map(int,x)) for x in csv.reader((d/'oam-pressure.csv').open())] if (d/'oam-pressure.csv').exists() else []
 pressure=[x for x in pressure if 2300<=x[1]<3300]
 return dict(lcd_frames=t[-1]['lcd_frame']+1,final_scene=t[-1]['world.weapons.scene_number'],m1_fps=fps(a),village_fps=fps([x for x in a if 2649<=int(x['camera_x'])<3000]),errors=max(x['app.error'] for x in t),m1_white=sum(x[2]==0 for x in white),m2_white=sum(x[2]==1 for x in white),m1_late=max(int(x['late']) for x in a),cosmetic_capacity_retries=max(x['video.objects.budget_retries'] for x in t),max_strip_drops=max(x.get('video.objects.strips.record_drops',0) for x in t),max_key_drops=max(x.get('video.objects.strips.key_drops',0) for x in t),village_oam_cycle_upper_bound=max((x[-1] for x in pressure),default=None),prestage_frames=max(x.get('m1.staged_frames',0) for x in t),prestage_bytes=max(x.get('m1.staged_bytes',0) for x in t))
build=read(B/'bugs116-prestage/build.json');assert sha(B/'bugs116-prestage/rom.gba')==build['rom_sha256']
runs={n:metrics('bugs116-verified-'+n) for n in ['normal','mount','burst9','stress3','pickup','observe']}
for n,v in runs.items():
 assert v['errors']==v['m1_white']==v['m1_late']==0
 if n in ['normal','mount','burst9','stress3']:assert v['final_scene']==1
gpu={n:{k:read(B/('bugs116-gpu-'+n)/(f+'.json')) for k,f in [('objects','object-hardware'),('background','m1-hardware')]} for n in ['normal','mount','stress3']}
def item(name,kind,x):
 rows=[{k:int(v) for k,v in q.items()} for q in csv.DictReader((B/name/'actors.csv').open())]
 q=[z for z in rows if z['entity']==7 and z['kind']==kind and z['x256']//256==x]
 return dict(samples=len(q),first_tick=q[0]['tick'],last_tick=q[-1]['tick'],minimum_y=min(z['y256'] for z in q)/256,maximum_y=max(z['y256'] for z in q)/256,last_y=q[-1]['y256']/256)
items={'H_before':item('bugs116-before-pickup',0,2635),'H_after':item('bugs116-verified-pickup',0,2635),'gas_before':item('bugs116-before-stress3',7,3037),'gas_after':item('bugs116-verified-stress3',7,3037)}
assert items['H_before']['last_y']==151 and items['H_after']['last_y']==231
assert items['gas_before']['last_y']==127 and items['gas_after']['last_y']==231
summary=dict(version='0.5.116-m1',rom_bytes=build['rom_bytes'],rom_sha256=build['rom_sha256'],rom_remaining_bytes=33554432-build['rom_bytes'],runs=runs,before=metrics('fonts115-m1'),items=items,gpu=gpu,memory=dict(ewram_free=812,iwram_free_before_reserved_stacks=16,m1_overlay_bytes=13592,overlay_capacity=13640),new_extra_cosmetic_dma_omission=False,limitations=['Enemy repeat-visibility complaint was not independently reproduced in a stable no-hit case; do not claim all flicker fixed.','30 FPS is not reached. Corrected pickups and projectiles change combat outcomes, so identical controller policy does not imply identical workload.','M2 white/late transition frames remain.','Whole campaign, physical GBA/link, and every M1 route are not verified.'])
old=zipfile.ZipFile('/workspace/scratch/779e6ec69435/releases/v0.5.115-m1/metal-slug-gba-v0.5.115-m1-source.zip')
preserved={}
for p in list((R/'art/approved-fonts').rglob('*'))+list((R/'art/gameplay-text').rglob('*'))+[R/'generated/native-fixed.4bpp',R/'generated/native-scaled-fixed-6x6.4bpp',R/'generated/approved-player-data.c',R/'generated/drawing-data.c',R/'generated/scaled-drawing-data.c']:
 if not p.is_file():continue
 key=str(p.relative_to(R));name=next(n for n in old.namelist() if n.endswith(key));preserved[key]=p.read_bytes()==old.read(name)
assert all(preserved.values());summary['approved_fonts_player_and_drawing_data_unchanged']=preserved
(O/'validation.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
captures={
 'slug-ng':('neogeo-bugs116-mount2/burst-005000.png',2648),'slug-before':('bugs116-mount-before/frame-006960.ppm',2648),'slug-after':('bugs116-verified-mount/frame-006960.ppm',2648),
 'h-ng':('neogeo-bugs116-pickup/burst-004856.png',2441),'h-before':('bugs116-before-pickup/frame-006300.ppm',2447),'h-after':('bugs116-verified-pickup/frame-006240.ppm',2447),
 'hut-ng':('neogeo-bugs116-mount2/burst-005792.png',2880),'hut-before':('bugs116-before-stress3/frame-009000.ppm',2880),'hut-after':('bugs116-verified-stress3/frame-009000.ppm',2880),
 'gas-ng':('neogeo-bugs116-mount2/burst-005952.png',2968),'gas-before':('bugs116-before-stress3/frame-009000.ppm',2880),'gas-after':('bugs116-verified-stress3/frame-009000.ppm',2880)}
images={}
for name,(src,camera) in captures.items():
 Image.open(B/src).convert('RGB').save(O/(name+'.png'))
 images[name]=dict(image=name+'.png',sha256=sha(O/(name+'.png')),capture=src,camera_x=camera)
(O/'captures.json').write_text(json.dumps(images,indent=2)+'\n')
def uri(name):return 'data:image/png;base64,'+base64.b64encode((O/(name+'.png')).read_bytes()).decode()
def figure(name,label):return f'<figure><img src="{uri(name)}"><figcaption>{label} · camera X={images[name]["camera_x"]}</figcaption></figure>'
def trio(prefix):return '<div class="trio">'+''.join(figure(prefix+'-'+k,label) for k,label in [('ng','실제 Neo Geo'),('before','수정 전 v0.5.115'),('after','수정 후 v0.5.116')])+'</div>'
rows=''.join(f'<tr><td>{n}</td><td>{v["lcd_frames"]:,}</td><td>{v["m1_fps"]:.2f}</td><td>{v["m1_white"]}</td><td>{v["errors"]}</td><td>{"M2 진입" if v["final_scene"]==1 else "M1 재현 구간"}</td></tr>' for n,v in runs.items())
page=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>M1 수정·원본 비교 — v0.5.116</title><style>
body{{margin:0;background:#f1f4f6;color:#1d2b35;font:16px/1.7 system-ui,sans-serif}}main{{max-width:1120px;margin:auto;padding:30px 24px}}h1{{font-size:30px;line-height:1.4}}h2{{margin-top:36px;font-size:23px}}.lead{{background:#dcefe7;border-left:5px solid #287159;padding:18px}}.note{{background:#fff0d7;padding:16px}}.trio{{display:grid;grid-template-columns:repeat(3,1fr);gap:14px}}figure{{margin:12px 0}}img{{width:100%;image-rendering:pixelated}}figcaption,small{{font-size:13px;color:#536671}}table{{width:100%;border-collapse:collapse;background:white}}td,th{{padding:10px;border-bottom:1px solid #d8e1e6;text-align:left}}code{{overflow-wrap:anywhere}}@media(max-width:700px){{.trio{{grid-template-columns:1fr}}main{{padding:20px 16px}}}}</style><main>
<small>2026-11-03 · 원본 Neo Geo 실행 + 실제 GBA ARM 롬 입력 재생</small><h1>M1 탄환·파괴 배경·아이템 낙하 수정</h1>
<p class="lead">슬러그 기관총의 그림 연결, 마을 파괴 배경의 위치, 아이템 접촉/낙하 분기를 수정했습니다. 일반·탑승·집중 파괴·가솔린 경로 네 가지에서 M1 보스 격파와 결과 화면을 거쳐 M2에 진입했습니다. 여섯 검증 경로에서 오류와 M1 흰 화면은 0건입니다.</p>
<p class="note">적이 반복해서 사라지는 현상 전체는 아직 해결 판정하지 않았습니다. 배경 표시 열의 인덱스 오류는 수정했고, 정지 관찰·집중 공격 경로의 OBJ/타일 전송을 검사했지만 피격과 무관한 반복 점멸의 동일 재현 조건은 확보하지 못했습니다.</p>
<h2>1. 슬러그 A 버튼 탄환</h2><p>슬러그 탄환은 권총과 수명 처리 경로를 공유하지만 원본에서는 HMG 그림 표를 사용합니다. 기존 GBA는 권총 그림을 선택했습니다. 원본 <code>$0308B0 → $03090C</code>의 <code>$29D4D2</code> 표를 연결해 파란 기관총 탄환을 복원했습니다. 초기 빈 그림 프레임과 발사·충돌 타이밍은 유지합니다.</p>{trio('slug')}
<h2>2. 다리·발판과 H 아이템</h2><p>마을 파괴 그림 70개가 시작 맵 기준으로 변환되어 실제 위치보다 1,440 GBA 픽셀 뒤에 적용됐습니다. 마을 맵의 원점(원본 X=1920)을 사용하도록 고쳤습니다. 또한 발판 접촉 조건이 반대로 옮겨져, 지지대가 없어진 아이템이 멈추던 분기를 수정했습니다. 원본의 공중 플래그와 C 함수의 지면 접촉 반환값을 구분합니다.</p>{trio('h')}
<h2>3. 마을 건물의 파괴 상태</h2><p>아래 X=2880 비교에서는 왼쪽 건물의 지붕·벽이 남던 기존 차이가 해소됐습니다. 원본과 수정본 모두 파괴 후 기둥·잔해 상태를 표시합니다. 주변 적과 오른쪽 구조물은 공격 이력과 시점이 달라 동일 상태로 간주하지 않습니다.</p>{trio('hut')}
<h2>4. 가솔린 낙하</h2><p>원본도 발판이 살아 있으면 가솔린이 그 위에 머무릅니다. 지지대 파괴 후에는 내려와야 합니다. 같은 GBA 입력 정책에서 수정 전은 Y=127에 멈췄고, 수정 후는 Y=231 바닥에 착지했습니다. 원본 실행에서도 발판 위 Y=127에서 바닥 Y=231로 이동합니다.</p>{trio('gas')}
<table><tr><th>재현 항목</th><th>v0.5.115 마지막 높이</th><th>v0.5.116 마지막 높이</th><th>원본 최종 바닥</th></tr><tr><td>H 아이템</td><td>151</td><td>231</td><td>231</td></tr><tr><td>가솔린</td><td>127</td><td>231</td><td>231</td></tr></table>
<p><small>높이는 원본 단위의 Y-down 좌표입니다. 이 마을 구간에서 Neo Geo 화면 상대 Y-up을 <code>512 + 16 − y</code>로 정규화했습니다. H 비교는 카메라 차이 6픽셀, 가솔린 비교는 88픽셀이 있습니다. 낙하/착지 상태 대조이며 프레임 전체 픽셀 일치 검증이 아닙니다. 공격 시점·RNG·적 위치도 다릅니다. 승인된 75% 축소와 상하 크롭은 유지합니다.</small></p>
<h2>전송과 안정성</h2><p>탑승 상태에서 슬러그가 퇴장하면 오류 화면으로 넘어가던 문제도 수정했습니다. 원본처럼 플레이어의 추락/사망 경로로 전환합니다. 배경의 변경 열 범위만 전송하고, 사용하지 않는 배경 VRAM에 큰 OBJ 데이터를 미리 준비해 파괴 순간의 VBlank 초과를 줄였습니다. 표시 중인 프레임과 다음 프레임 모두 해당 VRAM을 사용하지 않을 때만 적용하며, 다시 배경에 사용할 때 캐시를 무효화합니다.</p>
<p>일반 경로에서는 23,072바이트를 한 번 사전 전송했습니다. 별도로 시험했던 전송량 기반 장식 생략은 최종본에서 제거했습니다. 기존의 타일/OAM 용량 초과 보호는 유지하며 집중 파괴 경로에서 11번 동작했습니다. 이 보호는 장식용 연기·잔해만 생략하고 게임 객체 연산과 적·탄환·아이템·발판을 유지합니다.</p>
<h2>실행 검증</h2><table><tr><th>경로</th><th>GBA LCD 프레임</th><th>M1 평균 FPS</th><th>M1 흰 화면</th><th>오류</th><th>종료 지점</th></tr>{rows}</table>
<p>일반 경로 평균은 v0.5.115의 {summary['before']['m1_fps']:.2f} → {runs['normal']['m1_fps']:.2f} FPS입니다. 이번 작업은 버그 수정이며 30 FPS 목표 달성이 아닙니다. 같은 입력 정책을 사용했지만 수정된 아이템·탄환으로 전투 결과가 바뀌어 작업량이 완전히 같지는 않습니다. M2에서는 기존 전환 흰 화면이 1~2프레임 관찰됐습니다.</p>
<p>세 개의 커밋 직후 캡처에서 BG 맵·BG 타일·OBJ 타일·OAM·OBJ 팔레트를 실제 VRAM과 대조해 불일치 0바이트였습니다. 한 캡처에는 사전 전송 OBJ 23,072바이트가 포함됩니다. 접촉→지지대 제거→낙하→착지 테스트는 ASan/UBSan을 통과했고, 기존 지형 코드에서는 같은 테스트가 실패합니다.</p>
<p>기존 4bpp 팔레트와 공통 타일 11,688개의 출력 픽셀은 그대로 유지했습니다. 새로 필요한 파괴 경계 합성 317개도 같은 팔레트를 사용합니다. 승인된 글꼴·캐릭터·기존 OBJ 그림 데이터는 바이트 단위로 유지했습니다. EWRAM 여유 812바이트, 예약 스택을 제외한 IWRAM 여유 16바이트입니다.</p>
<h2>범위와 재현 자료</h2><p>mGBA 0.10.2와 MAME 0.264에서 버튼 입력만 사용했습니다. 게임 RAM·카메라·체력 주입은 없습니다. 원본은 실제 <code>mslug.zip</code> / Euro BIOS 실행이며 ROM SHA-256과 원본 캡처를 증거 ZIP에 기록했습니다. 원본 로그의 <code>damage</code> 열은 오프셋 $5E의 원시값이므로 피격 점멸 판정에는 사용하지 않았습니다.</p>
<p>마을 OBJ 스캔라인 비용은 전체 가로 폭을 포함한 보수적 추정 최대 784사이클이었습니다. 산식은 <a href="https://mgba-emu.github.io/gbatek/#maximum-number-of-sprites-per-line">GBATEK의 OBJ 처리 비용</a>을 사용했습니다. 이 결과로 모든 점멸 원인을 배제할 수는 없습니다. 전체 미션·실물 GBA·실물 2P 링크·모든 M1 경로 및 모든 피격 상태는 미검증입니다.</p>
<p><strong>전체 원본 동등성 판정: 미완료.</strong> 이 보고서는 명시한 버그와 캡처 범위를 검증합니다. 새 롬의 opening/scroll/transition/destruction/boss/results 전체 비교 세트를 완성했다는 의미가 아닙니다.</p>
<p>ROM: <strong>{build['rom_bytes']:,} bytes</strong> / 32 MiB 이내<br>SHA-256: <code>{build['rom_sha256']}</code></p></main></html>'''
(O/'M1-bugfix-comparison-v0.5.116.html').write_text(page)
sheet=Image.new('RGB',(1480,1160),'#18232d');draw=ImageDraw.Draw(sheet)
for col,label in enumerate(['NEO GEO / ACTUAL ROM','BEFORE / 0.5.115','AFTER / 0.5.116']):draw.text((20+col*490,16),label,fill='white')
for row,(prefix,label) in enumerate([('slug','SLUG VULCAN'),('h','H DROP + DESTROYED BRIDGE'),('hut','VILLAGE ROOF / WALL')]):
 y=48+row*365;draw.text((20,y),label,fill='#b8d9e8')
 for col,suffix in enumerate(['ng','before','after']):
  im=Image.open(O/(prefix+'-'+suffix+'.png')).convert('RGB')
  if im.size==(320,224):im=im.resize((240,168),Image.Resampling.NEAREST).crop((0,4,240,164))
  sheet.paste(im.resize((480,320),Image.Resampling.NEAREST),(20+490*col,y+22))
sheet.save(O/'M1-before-after-v0.5.116.png')
reference=R.parents[1]/'neogeo-reference/mslug.zip'
manifest=dict(gba_rom_sha256=build['rom_sha256'],neogeo_rom_sha256=sha(reference),scope='Bug-specific observations; incomplete whole-M1 reference gate',cases=[])
for prefix,event in [('slug','mounted vulcan fire'),('h','bridge destroyed / H grounded'),('hut','left hut destroyed'),('gas','support destroyed / fuel grounded')]:
 case=dict(mission=0,phase='destruction' if prefix!='slug' else 'scroll',reference_kind='neogeo_rom_capture',review='passed',approved_adaptations=['75% assets / approved vertical crop','BG palette quantization unchanged','Approved fonts'],unresolved_differences=['Surrounding combat state and timing differ; scoped event review only'])
 for key,suffix in [('neogeo','ng'),('gba','after')]:
  v=images[prefix+'-'+suffix];case[key]=dict(image=v['image'],sha256=v['sha256'],state=dict(scene=0,camera_x=v['camera_x'],camera_y=144,event_state=event))
 manifest['cases'].append(case)
(O/'reference-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
result=subprocess.run(['python3',str(R/'tools/check_reference_comparison.py'),'--candidate',str(B/'bugs116-prestage/rom.gba'),'--reference',str(reference),'--manifest',str(O/'reference-manifest.json'),'--missions','0'],capture_output=True,text=True)
assert result.returncode==1 and not json.loads(result.stdout)['reference_verified'];(O/'reference-gate.json').write_text(result.stdout)
print(json.dumps({'rom_bytes':build['rom_bytes'],'rom_sha256':build['rom_sha256'],'normal':runs['normal'],'reference_gate':False},indent=2))
