#!/usr/bin/env python3
"""Build the M1 village fault evidence from immutable ARM execution runs."""
import base64, copy, csv, hashlib, html, json, subprocess
from pathlib import Path
from PIL import Image, ImageDraw
import numpy as np
R=Path(__file__).resolve().parents[1]; B=R/'build'; O=R/'docs/m1-0.5.114'; O.mkdir(exist_ok=True)
def read(p): return json.loads(p.read_text())
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def trace(p): return [json.loads(s) for s in (p/'trace.jsonl').read_text().splitlines()]
def hashes(p): return {int(r[1]):r[3] for r in csv.reader((p/'frame-hashes.csv').open())}
def metrics(p):
 rows=list(csv.DictReader((p/'commits.csv').open()))
 a=[r for r in rows if r['playing']=='1' and r['presentation']=='0' and r['demo']=='0' and r['scene']=='0']
 a=a[next(i for i,r in enumerate(a) if int(r['live0'])):]
 def interval(q):
  first,last=q[0],q[-1];lcd=int(last['lcd'])-int(first['lcd']);n=int(last['commits'])-int(first['commits'])
  return dict(first_lcd=int(first['lcd']),last_lcd=int(last['lcd']),commits=n,fps=n/(lcd*280896/16777216))
 gaps=np.diff([int(r['lcd']) for r in a]);ms=gaps*280896/16777216*1000
 white=[list(map(int,r)) for r in csv.reader((p/'white-frames.csv').open())] if (p/'white-frames.csv').exists() else []
 return dict(m1=interval(a),village_2649_2999=interval([r for r in a if 2649<=int(r['camera_x'])<3000]),
  p95_ms=float(np.percentile(ms,95)),p99_ms=float(np.percentile(ms,99)),max_ms=float(max(ms)),
  m1_white=sum(r[2]==0 for r in white),m2_white=sum(r[2]==1 for r in white),m1_late=max(int(r['late']) for r in a))
base=B/'crash114-final'; normal=B/'crash114-final-baseline'; burst=B/'village114-final-9'; old=B/'village113-reproduce'
build=read(base/'build.json'); assert sha(base/'rom.gba')==build['rom_sha256']
runs=read(base/'runs.json'); final=[r for r in runs if r['rom_sha256']==build['rom_sha256']]
assert len(final)==11 and all(r['exit_code']==0 and r['error']=='00000000' for r in final)
oldhash=hashes(B/'perf113-memory'); newhash=hashes(normal); shared=oldhash.keys()&newhash.keys()
assert len(shared)==5664 and all(oldhash[k]==newhash[k] for k in shared) and not oldhash.keys()-newhash.keys()
oldburst=hashes(old); newburst=hashes(burst); sharedburst=oldburst.keys()&newburst.keys()
assert len(sharedburst)==2870 and all(oldburst[k]==newburst[k] for k in sharedburst)
fault=trace(old)[-1]; assert fault['app.error']==0x90001803
second=trace(B/'village114-stress-9')[-1]; assert second['app.error']==0x90001804
pressure=[list(map(int,r)) for r in csv.reader((burst/'pressure.csv').open())]
assert any(r[3] for r in pressure) and any(r[6] for r in pressure)
sections={}
for line in build['section_sizes'].splitlines():
 q=line.split()
 if len(q)==3 and q[0].startswith('.') and q[1].isdigit(): sections[q[0]]=(int(q[1]),int(q[2]))
sentinel=(0xa55a3cc3).to_bytes(4,'little')
def stack(p,lo,hi):
 data=(p/'iwram.bin').read_bytes();return hi-next((i for i in range(lo,hi,4) if data[i:i+4]!=sentinel),hi)
memory=dict(ewram_free=262144-sections['.data'][0]-sections['.bss'][0],iwram_before_stack_free=0x03006700-sum(sections['.iwram']),
 main_stack_max=max(stack(B/r['name'],0x6700,0x7700) for r in final),main_stack_reserved=4096,
 irq_stack_max=max(stack(B/r['name'],0x7700,0x7fa0) for r in final),irq_stack_reserved=2208)
summary=dict(version=build['version'],rom_sha256=build['rom_sha256'],rom_bytes=build['rom_bytes'],
 primary_fault=dict(code='90001803',lcd=fault['lcd_frame'],tick=fault['world.ticks'],camera_x=fault['world.weapons.camera_x'],palette_count=16,locked_mask='7fff',actor_count=fault['world.entities.active_count'],actor_peak=fault['world.entities.peak_count']),
 secondary_fault=dict(code='90001804',rom_sha256=read(B/'crash114/build.json')['rom_sha256'],lcd=second['lcd_frame'],tick=second['world.ticks'],camera_x=second['world.weapons.camera_x'],tile_count=second['video.objects.frame.tile_count'],tile_budget=1024),
 before=metrics(B/'perf113-memory'),after=metrics(normal),normal_rgb_commits_compared=len(shared),normal_rgb_differences=0,
 stress_prefix_commits_compared=len(sharedburst),stress_prefix_differences=0,
 final_runs=final,pressure_rows=pressure,pressure_columns=['lcd','tick','commit','palette_spills','camera_x','cosmetic_omitted','tile_compactions'],
 actual_cosmetic_budget_retries=sum(r['budget_retries'] for r in final),memory=memory,
 tests=['ASan/UBSan: original palette merge differential20000 +1000x128 requests, protected colors/tolerance',
        'ASan/UBSan: exact tile compaction1000 randomized bursts, OAM/pixel retention and real exhaustion',
        'ASan/UBSan: capture1200 frames/108000 draws; cosmetic-only retry preserves all noncosmetic maps and source strip admission'],
 limitations=['10 attack policies are finite button-only stress routes; not all policies clear M1.',
 'Existing camera2880 left hut destruction mismatch against Neo Geo remains unresolved.',
 'M2 transition white2/late2, full campaign, physical GBA and physical2P remain unverified.',
 'Not a30FPS performance release. Errors with other hexadecimal codes may have other causes.'])
(O/'validation.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')

# Compare fresh candidate captures with the actual Neo Geo captures from the
# previous checkpoint. Preserve the explicitly BLOCKED destruction case.
reference=copy.deepcopy(read(R/'docs/m1-0.5.113/reference-comparison.json'))
reference['gba_rom_sha256']=build['rom_sha256']
refs={int(r[1]):r for r in csv.reader((normal/'reference-captures.csv').open()) if int(r[3])==1}
tr=trace(normal)
def result_frame(tick):
 return min((r for r in tr if r['world.weapons.scene_number']==0 and r['world.weapons.camera_x']==3848 and (normal/f'frame-{r["lcd_frame"]:06}.ppm').exists()),key=lambda r:abs(r['world.ticks']-tick))['lcd_frame']
sheet=Image.new('RGB',(980,10*360),'#131a24');draw=ImageDraw.Draw(sheet)
for i,case in enumerate(reference['cases']):
 n=case['neogeo'];g=case['gba'];cx=g['state']['camera_x']
 src=B/n['capture_file']; previous=R/'docs/m1-0.5.113'/n['image']; assert sha(previous)==n['sha256']
 assert Image.open(src).convert('RGB').tobytes()==Image.open(previous).convert('RGB').tobytes()
 Image.open(src).convert('RGB').save(O/n['image']);n['sha256']=sha(O/n['image'])
 lcd=300 if i==0 else result_frame(5442) if i==8 else 14400 if i==9 else int(refs[cx][0])
 candidate=Image.open(normal/f'frame-{lcd:06}.ppm').convert('RGB');candidate.save(O/g['image'])
 g['lcd_frame']=lcd;g['sha256']=sha(O/g['image'])
 case['approved_adaptations']=['75% assets; center crop4+4 GBA pixels from224-line original','Existing background/HUD quantization and pressure palette sharing; no input/AI changes in114']
 case['review_scope']='Limited background/terrain/event-display comparison against actual Neo Geo capture. Candidate normal-route RGB hashes also match all5664 commits of113; this is not full behavior equivalence.'
 original=Image.open(src).convert('RGB').resize((240,168),Image.Resampling.NEAREST).crop((0,4,240,164))
 draw.text((8,i*360+6),f'{i+1:02} {case["phase"]} | camera {cx},{g["state"]["camera_y"]} | {case["review"]}',fill='white')
 sheet.paste(original.resize((480,320),Image.Resampling.NEAREST),(0,i*360+30));sheet.paste(candidate.resize((480,320),Image.Resampling.NEAREST),(500,i*360+30))
sheet.save(O/'reference-contact-sheet.png')
(O/'reference-comparison.json').write_text(json.dumps(reference,ensure_ascii=False,indent=2)+'\n')
check=subprocess.run(['python3',str(R/'tools/check_reference_comparison.py'),'--candidate',str(base/'rom.gba'),'--reference','/workspace/scratch/779e6ec69435/neogeo-reference/mslug.zip','--manifest',str(O/'reference-comparison.json'),'--missions','0'],capture_output=True,text=True)
(O/'reference-gate.json').write_text(check.stdout); assert check.returncode==1

# Fresh Neo Geo burst capture: same camera, different damage/RNG timing.
ng=B/'neogeo-village114-stress';script=R/'tests/neogeo_village_pressure.lua'
provenance=read(ng/'provenance.json');provenance['script_sha256']=sha(script);(ng/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n');(ng/'capture.lua').write_bytes(script.read_bytes())
original=Image.open(ng/'matched-005220.png').convert('RGB');original.save(O/'burst-neogeo.png')
candidate=Image.open(burst/'frame-007003.ppm').convert('RGB');candidate.save(O/'burst-gba.png')
pair=Image.new('RGB',(980,350),'#131a24');d=ImageDraw.Draw(pair)
d.text((8,8),'Neo Geo | camera2880,144',fill='white');d.text((508,8),'GBA114 | camera2880,144',fill='white')
pair.paste(original.resize((240,168),Image.Resampling.NEAREST).crop((0,4,240,164)).resize((480,320),Image.Resampling.NEAREST),(0,28));pair.paste(candidate.resize((480,320),Image.Resampling.NEAREST),(500,28));pair.save(O/'burst-comparison.png')
Image.open(old/'frame-006804.ppm').save(O/'error-90001803.png')
Image.open(burst/'frame-007076.ppm').save(O/'fixed-village.png')
Image.open(normal/'frame-014400.ppm').save(O/'mission-complete.png')
def image_uri(name):return 'data:image/png;base64,'+base64.b64encode((O/name).read_bytes()).decode()
def figure(name,label):return f'<figure><img src="{image_uri(name)}" alt="{html.escape(label)}"><figcaption>{html.escape(label)}</figcaption></figure>'
before=summary['before'];after=summary['after'];testrows=''.join(f'<tr><td>{r["variant"] or "기준 경로"}</td><td>{r["lcd"]+1:,}</td><td>{r["error"]}</td><td>M{r["scene"]+1} · X{r["camera_x"]}</td></tr>' for r in sorted(final,key=lambda r:r['variant']))
page=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>M1 마을 중단 오류 · v0.5.114-m1</title><style>
body{{font:16px/1.75 system-ui,sans-serif;background:#f3f6fa;color:#182432;margin:0}}main{{max-width:1050px;margin:auto;padding:36px 24px}}h1{{font-size:30px;line-height:1.3}}h2{{margin-top:36px;font-size:22px}}.lead{{background:#e0f1ed;border-left:5px solid #267965;padding:20px}}.pair{{display:grid;grid-template-columns:1fr 1fr;gap:20px}}figure{{margin:10px 0}}img{{width:100%;image-rendering:pixelated;background:#111}}figcaption,small{{font-size:13px;color:#506173}}table{{border-collapse:collapse;width:100%;background:white}}td,th{{border-bottom:1px solid #dce4ee;padding:10px;text-align:left}}code{{overflow-wrap:anywhere}}.note{{background:#fff1dc;padding:18px}}@media(max-width:650px){{.pair{{grid-template-columns:1fr}}}}</style><main>
<small>2026-11-03 · M1 안정성 수정 · 입력 버튼만 사용하는 실제 ARM 롬 실행</small><h1>마을 연쇄 파괴 중 오류창이 뜨는 원인과 수정</h1>
<p class="lead"><b>배포판113에서 오류90001803을 재현했습니다.</b> 수정판114는 같은 공격 패턴으로 해당 지점을 통과하고 M1 보스 종료·결과 화면·M2 진입까지 진행했습니다. 재현 이후에 드러난 타일 용량 오류90001804도 함께 처리했습니다.</p>
<div class="pair">{figure('error-90001803.png','이전 롬: 오류 진입 직후. DETAIL90001803 / B: RETURN TO TITLE')}{figure('fixed-village.png','수정 롬: 같은 연쇄 파괴 경로에서 후속 타일 압박 구간 통과')}</div>
<h2>원인</h2><table><tr><th>중단</th><th>실제 상태</th><th>코드 경로</th></tr><tr><td>90001803<br>X2727 · tick2869</td><td>OBJ 팔레트16/16, 보호된 팔레트15개. 둘 이상의 비보호 팔레트를 요구하는 병합이 실패.</td><td>object_video.palette → NOBJ_PALETTES → NOS_HARDWARE → 오류 화면</td></tr><tr><td>90001804<br>X2880 · tick2916</td><td>팔레트만 수정한 후보에서 후속 재현. OBJ 타일999/1024 상태에서 다음 그래픽을 배치하지 못함.</td><td>object_video.tile_data → NOBJ_TILES → 오류 화면</td></tr></table>
<p>첫 오류에서 활성 엔티티82개, 관측 최고101개로 풀256개를 넘지 않았습니다. 게임 로직·충돌·오디오 오류는0이었습니다. B는 오류 화면에서 타이틀로 돌아가는 메뉴 입력이며 기기의 자동 재부팅이 아닙니다. 사용자 화면의 실제 오류 번호는 아직 전달받지 않았으므로, 여기서 확정한 원인은 배포판에서 재현한 이 오류 경로입니다.</p>
<h2>수정 범위</h2><ol><li>팔레트가 가득 찬 경우, 이미 보호된 색은 유지하면서 비보호 효과의 색을 공유합니다. 기존 RGB555 허용 오차를 넘기지 않고, 이미 합친 색을 다시 근사하지 않습니다. 플레이어·탄환·HUD의 정확한 색 요청은 그대로 보호합니다.</li><li>타일 용량 부족 시 실제 픽셀 바이트가 같은 그래픽을 합칩니다. OAM 참조도 함께 갱신하므로 그림을 지우지 않습니다.</li><li>실제 용량 초과가 남을 때만 장식으로 명시된 연기·파편을 제외하고 화면 구성을 한 번 재시도합니다. 시뮬레이션·RNG·충돌·원본 스프라이트 예산은 유지합니다. 이번11개 최종 실행에서는 이 장식 감축 재시도는 <b>{summary['actual_cosmetic_budget_retries']}회</b>였습니다.</li></ol>
<p>재현 경로에서 팔레트 공유는 tick2869·2870, 중복 타일 회수는 tick2916·2917에 관측됐습니다. 일반 M1 경로는 이전 배포판과 출력 화면5664개가 모두 동일한 RGB 해시였고, 오류 발생 전 재현 경로2870개 출력도 동일했습니다. 원본과의 동작 일치 판정과는 별개의 회귀 검사입니다.</p>
<h2>실행 결과</h2><table><tr><th>입력 패턴</th><th>GBA LCD 프레임</th><th>오류</th><th>종료 시점</th></tr>{testrows}</table>
<p>기준 경로와 패턴9는 M1 보스 종료 및 M2 진입까지 확인했습니다. 일부 스트레스 입력은 특정 위치에서 사격을 유지하는 패턴이므로 그 실행이 미션 클리어 검증은 아닙니다.</p>
<div class="pair">{figure('mission-complete.png','수정판 MISSION1 COMPLETE')}{figure('burst-comparison.png','실제 네오지오 / 수정GBA · 카메라2880,144. 입력 주기·피격·파괴 시점은 동일하지 않음.')}</div>
<h2>원본 대조와 남은 차이</h2><p>실제 네오지오 ROM(MAME0.264, euro-s1)을 새로 실행해 마을 연사·수류탄 구간을 촬영했습니다. 기존10개 좌표·이벤트 기준에도 수정판 화면을 다시 대조했습니다. 9개는 제한된 배경·지형·이벤트 표시 범위에서 유지되었지만, X2880의 왼쪽 오두막 파괴 상태 차이는 남아 있습니다. 따라서 원본 대조 전체 통과 판정은 하지 않았습니다. 입력·AI·애니메이션의 완전 동등성 검증도 아닙니다.</p>
<h2>성능과 메모리</h2><table><tr><th>기준 M1 경로</th><th>113</th><th>114</th></tr><tr><td>평균FPS</td><td>{before['m1']['fps']:.3f}</td><td>{after['m1']['fps']:.3f}</td></tr><tr><td>마을X2649..2999 평균FPS</td><td>{before['village_2649_2999']['fps']:.3f}</td><td>{after['village_2649_2999']['fps']:.3f}</td></tr><tr><td>M1 흰 화면 / 늦은 전송</td><td>{before['m1_white']} / {before['m1_late']}</td><td>{after['m1_white']} / {after['m1_late']}</td></tr><tr><td>최대 프레임 간격</td><td>{before['max_ms']:.1f}ms</td><td>{after['max_ms']:.1f}ms</td></tr></table>
<p>이번 수정은 중단 방지용이며30FPS 목표는 미달입니다. EWRAM 여유{memory['ewram_free']}B, IWRAM 스택 앞 여유{memory['iwram_before_stack_free']}B. 최종11개 실행에서 관측한 주 스택 최대{memory['main_stack_max']}/{memory['main_stack_reserved']}B, IRQ 스택{memory['irq_stack_max']}/{memory['irq_stack_reserved']}B입니다.</p>
<p>ASan/UBSan: 팔레트 기존 구현 대조20000건·128팔레트 압박1000회, 타일/OAM 보존 압박1000회, 캡처1200프레임·108000그리기 및 장식 제외 시 나머지 배치 보존 검사를 통과했습니다.</p>
<p class="note">기존 M2 전환의 흰 화면2회, 오두막 파괴 상태 차이, 실물GBA·실물2P·전체 미션 검증은 남아 있습니다. 다른 오류 번호까지 모두 해결했다는 뜻은 아닙니다.</p>
<h2>배포 파일 식별</h2><p><b>metal-slug-gba-native-v0.5.114-m1.gba</b><br>{build['rom_bytes']:,}bytes · {build['rom_bytes']/1e6:.2f}MB · {build['rom_bytes']/1048576:.2f}MiB<br>SHA256 <code>{build['rom_sha256']}</code></p><p><small>버튼 입력·CPU/RAM 읽기만 사용. 카메라·체력·진행 상태 주입 없음. 실패한 중간 빌드를 최종 검증 결과에 포함하지 않았습니다.</small></p></main></html>'''
(O/'M1-village-crash-v0.5.114.html').write_text(page)
print(json.dumps({k:summary[k] for k in ('version','rom_bytes','rom_sha256','before','after','memory','actual_cosmetic_budget_retries')},indent=2))
