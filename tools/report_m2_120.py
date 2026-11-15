"""Package measured M2 native-catalog evidence without claiming full equivalence."""
import base64,csv,hashlib,html,json,shutil,struct,zipfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parents[1];B=R/'build/m2-120';D=R/'docs/m2-0.5.120';D.mkdir(exist_ok=True)
HZ=16777216/280896
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def save(name,data):(D/name).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
build=read(B/'m/build.json');route=B/'m-route';execution=read(route/'execution.json')
assert execution['exit_code']==0
commits=rows(route/'commits.csv')
probe={x.split()[2]:(int(x.split()[0],16),int(x.split()[1])) for x in (B/'m/probe.txt').read_text().splitlines()}
final_ram=(route/'ewram.bin').read_bytes()
def final_field(k):
 a,w=probe[k];return int.from_bytes(final_ram[a-0x02000000:a-0x02000000+w],'little')
assert final_field('world.weapons.scene_number')==3 and final_field('world.results.progress')==2
trace=[json.loads(x) for x in (route/'trace.jsonl').read_text().splitlines()]
assert not any(x['app.error'] or x['audio.fault'] for x in trace)
def metrics(name):
 allrows=rows(B/name/'commits.csv');r=[x for x in allrows if x['scene']=='1' and x['playing']=='1']
 ds=[int(b['lcd'])-int(a['lcd']) for a,b in zip(r,r[1:])];ordered=sorted(ds)
 after=next((x for x in allrows if x['scene']=='3'),None)
 return dict(fps=(int(r[-1]['commits'])-int(r[0]['commits']))*HZ/(int(r[-1]['lcd'])-int(r[0]['lcd'])),
  first_lcd=int(r[0]['lcd']),last_lcd=int(r[-1]['lcd']),committed_intervals=int(r[-1]['commits'])-int(r[0]['commits']),
  p99_ms=ordered[int((len(ordered)-1)*.99)]*1000/HZ,worst_ms=max(ds)*1000/HZ,
  intervals_ge_100ms=sum(x*1000/HZ>=100 for x in ds),m2_late=max(int(x['late']) for x in r),
  next_mission=after,m2_max_entities=max(int(x['entities']) for x in r))
current=metrics('m-route');legacy=metrics('legacy-k-route')
hashes=lambda n:[x.split(',')[-1] for x in (B/n/'frame-hashes.csv').read_text().splitlines()]
assert hashes('m-route')==hashes('base-route')
gpu={}
for n in [45000,45500,47000,48000]:
 p=route/f'commit-{n}';gpu[str(n)]={k:read(p/f) for k,f in [('state','snapshot-state.json'),('background','m2-hardware.json'),('objects','object-hardware.json')]}
old=zipfile.ZipFile(R.parents[1]/'metal-slug-gba-v0.5.119-m1-source.zip')
asset_audit={}
for p in sorted((R/'generated/m1-cartridge').glob('*.bin')):
 rel=p.relative_to(R).as_posix();same=hashlib.sha256(old.read('metal-slug-native/'+rel)).hexdigest()==sha(p);assert same;asset_audit[rel]=dict(sha256=sha(p),unchanged_from_119=same)
ram=(route/'iwram.bin').read_bytes();stack={}
for name,lo,hi in [('main',0x6700,0x7700),('irq',0x7700,0x7fa0)]:
 first=next((i for i in range(lo,hi,4) if ram[i:i+4]!=struct.pack('<I',0xa55a3cc3)),hi);stack[name]={'used':hi-first,'reserved':hi-lo}
validation=dict(version=build['version'],candidate='m',rom_bytes=build['rom_bytes'],rom_sha256=build['rom_sha256'],
 target_30fps_met=current['fps']>=30,full_reference_verified=False,
 method='mGBA0.10.2 actual LCD cycles, input-only mode8 state-aware buttons, M1 start through M2 boss/results and M3 initialization. The final run stops before the first M3 video commit. FPS counts committed unique frames at16777216/280896 LCD Hz. Death/continue/result time included. No RAM/HP/progression injection. Both renderers use the same corrected gameplay and controller policy, not an exactly synchronized workload.',
 final_state={k:final_field(k) for k in ['world.weapons.scene_number','scene.scene','world.results.progress','scene.command','app.error','video.commits','video.late_transfers']},
 current=current,legacy_renderer=legacy,execution=execution,m1_committed_rgb_frames_unchanged=len(hashes('m-route'))-1,
 source_position_oracle=read(B/'boss2-position-oracle.json'),source_scroll_oracle=read(B/'m2-scroll-latch-oracle.json'),gpu=gpu,
 memory={'ewram_free':424,'iwram_free_before_reserved_stacks':0,'stack_high_water':stack,'rom_free':33554432-build['rom_bytes']},
 limitations=['30fps target not met.','Neo Geo camera/event comparisons are scoped; combat inputs and animation phases differ. The captured final arena camera Y is47 in GBA vs32 in the original reference; not certified equivalent.',
 'Final run stops at M3 initialization before its first video commit. Earlier extended candidate K observed one late transfer at M3 entry; M3 gameplay remains outside scope.','Physical GBA, physical2P and other missions not validated. Stack high-water is this route only.'])
save('validation.json',validation);save('asset-audit.json',asset_audit)
capture_rows=[list(map(int,x.split(','))) for x in (route/'m2-reference-captures.csv').read_text().splitlines()]
def capture_at(x,y,age=1):return next(a[0] for a in capture_rows if a[1:4]==[x,y,age])
def nearest_frame(tick):
 q=min([q for q in trace if (route/f"frame-{q['lcd_frame']:06}.ppm").exists() and q['world.weapons.scene_number']==1],key=lambda q:abs(q['world.ticks']-tick))
 return q['lcd_frame'],q['scene.camera.x']>>16,q['scene.camera.y']>>16
end_n,end_x,end_y=nearest_frame(48030)
boss_n,boss_x,boss_y=nearest_frame(47000)
cases=[
 ('opening','opening','도입부',B/'neogeo3/matched-008990.png',capture_at(0,320,180),(0,320),(0,320),'opening','opening'),
 ('midboss','destruction','중간 포대',B/'neogeo6/matched-018600.png',capture_at(2576,320,60),(2576,320),(2576,320),'cannon active','cannon active'),
 ('stairs','scroll','포대 이후 계단',B/'neogeo7/matched-010291.png',capture_at(2784,320),(2784,320),(2784,320),'after cannon','after cannon'),
 ('rebind','transition','배경 재연결',B/'neogeo7/matched-010557.png',capture_at(2848,256),(2848,256),(2848,256),'map transition','map transition'),
 ('boss','boss','최종 보스 반복 배경',B/'neogeo8/neogeo-015000.png',boss_n,(3990,32),(boss_x,boss_y),'boss combat','boss combat'),
 ('results','results','결과 화면',B/'neogeo8/neogeo-018450.png',end_n,(None,None),(end_x,end_y),'prisoner results','prisoner results')]
manifest={'gba_rom_sha256':build['rom_sha256'],'neogeo_rom_sha256':sha(R.parents[1]/'reference118/mslug.zip'),
 'scope':'Four matched camera comparisons plus unmatched final arena/results inspection. Not full behavioral/reference certification.','cases':[]}
for key,phase,title,ng,n,ns,gs,ne,ge in cases:
 raw=D/f'{key}-neogeo-raw.png';shutil.copy2(ng,raw)
 im=Image.open(ng).convert('RGB');assert im.size==(320,224);im.resize((240,168),Image.Resampling.NEAREST).crop((0,4,240,164)).save(D/f'{key}-neogeo.png')
 Image.open(route/f'frame-{n:06}.ppm').save(D/f'{key}-gba.png')
 sides={}
 for side,xy,event in [('neogeo',ns,ne),('gba',gs,ge)]:
  p=D/f'{key}-{side}.png';sides[side]={'image':p.name,'sha256':sha(p),'state':{'scene':1,'camera_x':xy[0],'camera_y':xy[1],'event_state':event}}
 sides['neogeo'].update(raw_image=raw.name,raw_sha256=sha(raw),source_capture=str(ng.relative_to(B)))
 sides['gba']['source_lcd']=n
 manifest['cases'].append(dict(mission=1,phase=phase,reference_kind='neogeo_rom_capture',review='scoped',
  approved_adaptations=['75% scaling and4+4px crop','GBA4bpp BG palette reduction, existing English HUD/font and cosmetic-effect budget'],
  unresolved_differences=['Player/enemy attack histories and animation phases are not synchronized.']+(['Camera coordinates are not matched.'] if ns!=gs else []),**sides))
save('reference-manifest.json',manifest)
chain=[]
for name in ['neogeo3','neogeo5','neogeo6','neogeo7']:
 p=B/name/'m2-latest.sta';chain.append({'directory':name,'state':p.name,'sha256':sha(p),'used_as_input_only_prefix_for_next_branch':True})
save('reference-chain.json',{'emulator':'MAME0.264 euro-s1 BIOS','branches':chain,'final':'neogeo8 M2 boss/results then MISSION3 START; its later attract-mode save is excluded','excluded':'neogeo4 load was canceled by same-frame save; not reference evidence'})
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',20)
sheet=Image.new('RGB',(1000,len(cases)*365+50),'#111c26');draw=ImageDraw.Draw(sheet);draw.text((16,12),'Neo Geo (75%, crop4+4)                       GBA v0.5.120-m2',font=font,fill='white')
for i,(key,phase,title,ng,n,ns,gs,ne,ge) in enumerate(cases):
 y=50+i*365;draw.text((16,y),f'{key}: Neo Geo {ns if ns[0] is not None else "camera not sampled"} / GBA {gs}',font=font,fill='#b8d8ea')
 for j,side in enumerate(['neogeo','gba']):sheet.paste(Image.open(D/f'{key}-{side}.png').resize((480,320),Image.Resampling.NEAREST),(16+j*492,y+32))
sheet.save(D/'reference-comparison.png')
def data_image(p):return 'data:image/png;base64,'+base64.b64encode(p.read_bytes()).decode()
fmt=lambda n:f'{n:.2f}'
rows_html=''.join(f'<tr><td>{label}</td><td>{fmt(a)}</td><td>{fmt(b)}</td></tr>' for label,a,b in [
 ('전체 M2 평균 FPS',legacy['fps'],current['fps']),('99백분위 프레임 간격(ms)',legacy['p99_ms'],current['p99_ms']),('가장 긴 프레임 간격(ms)',legacy['worst_ms'],current['worst_ms']),('100ms 이상 간격(회)',legacy['intervals_ge_100ms'],current['intervals_ge_100ms'])])
palette=read(R/'generated/m2-cartridge/report4.json');catalog=read(R/'generated/m2-cartridge/report.json')
doc=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>M2 GBA 전용화 v0.5.120</title>
<style>body{{margin:0;background:#eef2f5;color:#162632;font:16px/1.7 system-ui,sans-serif}}main{{max-width:1040px;margin:auto;padding:36px 24px}}h1{{font-size:34px;line-height:1.3}}h2{{font-size:23px;margin-top:36px}}.note{{padding:18px;background:#fff3dc;border-left:4px solid #bd8328}}.ok{{padding:18px;background:#e0eee9;border-left:4px solid #3f7965}}table{{border-collapse:collapse;width:100%;background:white}}td,th{{border:1px solid #cdd7de;padding:10px;text-align:left}}th{{background:#dfE8ef}}img{{max-width:100%;image-rendering:pixelated}}code{{overflow-wrap:anywhere}}.muted{{color:#526774;font-size:14px}}</style><main>
<p class="muted">METAL SLUG NATIVE · 2026-11-03 · v0.5.120-m2</p><h1>M2를 GBA 타일·맵으로 연결하고<br>보스 격파와 결과 화면까지 확인했습니다.</h1>
<div class="ok">최종 ROM을 실제 에뮬레이터에서 버튼 입력만으로 실행했습니다. M1 → M2 → 최종 보스 HP0 → 결과 화면 → 다음 미션 초기화 진입을 확인했습니다. M2 구간의 오류·흰 화면·지연 전송은 0건입니다.</div>
<div class="note">M2 평균 <b>{current['fps']:.2f}fps</b>. 목표 30fps에는 아직 못 미칩니다. 원본과의 대조는 구간별 확인이며, 전투 상태 전체가 일치한다고 판정하지 않았습니다.</div>
<h2>이번에 연결한 구조</h2><ul>
<li>M2의 7개 배경(일반6개 + 보스 반복 맵52)을 완성된 8×8 GBA 타일·맵으로 ROM에 저장합니다. 런타임의 원본 조각 합성과 색 탐색을 제거했습니다.</li>
<li>최종 보스 첫 재연결에서는 이전 배경을 유지하고, 이후에는 사전 생성한 반복 구간을 선택합니다. 파괴 변경40종의 겹침·반복도 사전 변환했습니다.</li>
<li>4bpp 배경,12개 팔레트 뱅크, 타일 상주·변경 범위 전송·안전한 선행 DMA를 사용합니다. 승인된75% 크기와 기존 M1 에셋·HUD·영문 폰트는 유지합니다.</li>
<li>중간 포대 좌표, 적 무리의 카메라 잠금, 퇴장한 수송기 자식의 잠금 잔류, 최종 보스·탄환의 화면 좌표 유지, 자동 스크롤 중 플레이어 이동을 수정했습니다.</li>
<li>이미 제거된 부속 포탑에 해제 명령이 도착할 때 오류 화면이 뜨던 경로를 수정했습니다. 세대가 바뀐 재사용 슬롯에는 쓰지 않습니다.</li></ul>
<h2>성능 측정</h2><table><tr><th>항목</th><th>기존 M2 렌더러 + 동일 게임 수정</th><th>전용 GBA 맵 최종판</th></tr>{rows_html}</table>
<p class="muted">mGBA0.10.2의 실제 GBA LCD 사이클로 계산한 고유 화면 갱신율입니다. 60Hz 화면 반복 횟수를 FPS로 세지 않았습니다. 두 ROM은 같은 상태 기반 조작 정책을 사용했으나 완전히 동기화된 전투 부하는 아닙니다. 사망·컨티뉴·결과 구간을 포함합니다. 기존 렌더러는 가까운2개 배경 제한이 있어 표시 범위도 다릅니다.</p>
<h2>검증 근거</h2><ul>
<li>M1의5,566개 커밋 출력 RGB 해시가 v0.5.119와 모두 동일합니다. M1 타일·맵·팔레트 바이너리도 그대로입니다.</li>
<li>배경 검사: M1 976,500개, M2 1,315,020개 타일 검사 및 보스 맵1,000회 래핑 검사 통과. ASan/UBSan 사용(호스트 환경의 LeakSanitizer는 사용 불가).</li>
<li>제공된 네오지오68000 코드를 실행한 포대 좌표10,003건과 스크롤 잠금10,003건 모두 일치했습니다. 이는 해당 연산의 검사이며 전체 AI 동등성 검사는 아닙니다.</li>
<li>최종 ROM의 네 지점에서 CPU 화면 커밋 순간을 캡처해 BG/OBJ 타일, 맵, OAM, 팔레트 바이트를 검사했습니다. 불일치0건입니다.</li>
<li>실제 네오지오 ROM을 MAME0.264에서 실행해 M2 결과와 다음 미션까지 확인했습니다. 아래는 원본 캡처와 GBA 캡처의 대조입니다.</li></ul>
<h2>원본 화면 대조</h2><p>도입부·포대·계단·재연결은 카메라 좌표를 맞췄습니다. 적·플레이어의 행동과 애니메이션 시점은 다릅니다. 최종 보스의 기준 캡처는 원본 Y32 / GBA Y47로 높이가 달라 추가 대조가 필요합니다. 결과 화면 역시 동작 단계 확인이며 프레임 전체 일치 판정이 아닙니다.</p>
<img alt="Actual Neo Geo and GBA comparison" src="{data_image(D/'reference-comparison.png')}">
<p class="muted">원본320×224 → 240×168 최근접 축소 후 위·아래4픽셀씩 잘라 비교했습니다. 원본 PNG, 좌표, 해시와 미해결 차이는 검증 자료에 포함합니다. full_reference_verified=false.</p>
<h2>에셋·메모리와 배포 파일</h2><p>논리 타일{catalog['tiles']:,}개 / 4bpp 물리 타일{palette['physical_tiles']:,}개. 배경 전체 색281→191. 4bpp 단계의 RGB555 채널 오차99백분위 {palette['p99_channel_error']:.0f}, 최대 {palette['max_channel_error']:.0f}입니다. 기존75% 에셋의 크기는 다시 변경하지 않았습니다.</p>
<p>ROM <b>{build['rom_bytes']:,}바이트</b> ({build['rom_bytes']/1000000:.2f}MB / {build['rom_bytes']/1048576:.2f}MiB),32MiB 한도 내 여유 {33554432-build['rom_bytes']:,}바이트입니다. EWRAM 여유424바이트, 예약 스택 앞 IWRAM 여유0바이트로 메모리 추가 사용에 여유가 작습니다. 관측 스택 main {stack['main']['used']}/4096, IRQ {stack['irq']['used']}/2208바이트.</p>
<p><code>{build['rom_sha256']}</code></p>
<h2>남은 범위</h2><ul><li>30fps 성능 목표와 원본 대비 전투·카메라·애니메이션의 완전 일치 검증.</li><li>M2 마지막 프레임까지 지연 전송0건. 최종 실행은 다음 미션 초기화에서 종료하며 M3 첫 화면까지 표시하지 않았습니다. 이전 후보K의 연장 실행에서 관측한 M3 진입 지연 전송1건과 이후 플레이는 별도 범위입니다.</li><li>실기 GBA·실기2P 링크와 모든 조작 경로는 검증하지 않았습니다.</li></ul></main></html>'''
(D/'M2-native-v0.5.120.html').write_text(doc)
checkpoint=f'''M2 native GBA candidate0.5.120-m2 complete playable scope, 2026-11-03.
Final candidate m, ROM {build['rom_bytes']} bytes SHA256 {build['rom_sha256']}.
Final input-only m-route: M1 -> M2 boss111 HP0 -> prisoner/results -> M3 initialization
(scene3, source campaign order0,1,3,2,4,5), LCD {execution['last_lcd_frame']}; error0,
all-white0, M2 late0. Stops before first M3 video commit. Earlier K extended route
observed M3 entry late1; M3 remains outside scope. No RAM/HP injection.
M2 FPS {current['fps']:.9f}; corrected legacy renderer {legacy['fps']:.9f};30 NOTmet.
No identical workload claim. M1all5566 RGB commits + initial empty frame match119.
M2 maps15..20 plus separate arena52 (essential: earlier H/K missed52 and went blank).
Offline arena inherits image19 through firstrelink4096, then three320px periods;
origin(3072,12) GBA pixels plus generation-capped240/480px offset. No pixel compositor.
Catalog {catalog['tiles']}logical/{palette['physical_tiles']}physical4bpp,40patches,
12banks/191colors, 99percent RGB555 error2 max6. M1 approved bytes unchanged.
Gameplay: boss132 native coordinate conversion, carrier child retirement,
mission op5/8 edge_off consumer; finalboss root/selected ordnance screen carry;
scene1_horizontal_lock consumes source106F5E; released cannon references idempotent.
The old0x0b1e0000 was trapped at m5_emit M5C_70ED6 (released child92/gen149),
not pool capacity. entityfault32 is a generic downstream marker.
Source oracles:10003 boss132 coordinates +10003 scene/terrain latch cases pass.
ASan/UBSan M1runtime976500/M21315020 cells, arena1000 wraps; retired/recycled
attachment callback test and event-camera100 cycles pass. LeakSanitizer unavailable
under host ptrace; ASan uses detect_leaks=0. ARM budgets checked by final build.
Four coherent GPU snapshots45000/45500/47000/48000 BG/OBJ/OAM/palette mismatch0.
Actual Neo Geo MAME0.264 euro-s1: branches neogeo3->5->6->7->8 endM2->M3.
Four matched-camera screenshot cases, boss/results only scoped. Captured arenaY
GBA47 vsNG32, combat histories differ, fullreferencegateFALSE. Do not call exact
NeoGeo behavior verified. neogeo4 invalid canceled-load excluded; ng8 latest
save is later attract mode and not a resume point. Original ROMs not in package.
Source build uses tools/build_m1_cartridge_maps.py --mission2 then
tools/build_m1_4bpp.py --mission2 then tools/build_native.py.
Final run/probes/ELF are build/m2-120/m and m-route. Preserve originals;
scratch source is not Git-backed. Deliver ROM/report/source/evidence together.
'''
(D/'checkpoint.txt').write_text(checkpoint)
print(json.dumps({'current':current,'legacy':legacy,'rom_bytes':build['rom_bytes'],'sha256':build['rom_sha256'],'report':str(D/'M2-native-v0.5.120.html')},ensure_ascii=False))
