#!/usr/bin/env python3
"""Package measured renderer/CPU optimizations; keep scope and regressions explicit."""
import base64,csv,difflib,hashlib,html,json,re,shutil,struct,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[1];B=R/'build/perf104';D=R/'docs/performance-0.5.104'
ROM='a6975d1f43770ff72295f22f881582e8c61a5193bb2eef3e39ca25de53fd5d5e'
HZ=16777216;LCD=280896

def read(p):return json.loads(p.read_text())
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def interval(r):
 a,z=r[0],r[-1];sec=(int(z['lcd'])-int(a['lcd']))*LCD/HZ;count=int(z['commits'])-int(a['commits'])
 return dict(lcd_start=int(a['lcd']),lcd_end=int(z['lcd']),commits=count,seconds=sec,fps=count/sec)
def active(p):
 rr=[r for r in rows(p/'commits.csv') if r['playing']=='1' and r['presentation']=='0' and r['demo']=='0'];rr=rr[next(i for i,r in enumerate(rr) if int(r['live0'])):]
 assert not any(int(r['paused']) for r in rr);assert all(int(y['ticks'])>=int(x['ticks']) for x,y in zip(rr,rr[1:]));return rr

def archive(source,target):
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for p in sorted(source.rglob('*')):
   if p.is_file() and p.suffix in ['.json','.jsonl','.csv','.txt','.log','.bin','.c','.h','.py','.png','.ppm']:
    z.write(p,str(p.relative_to(source)))

def main():
 D.mkdir(parents=True,exist_ok=True);build=read(R/'generated/native-build.json');assert build['version']=='0.5.104'
 assert sha(Path(build['rom']))==ROM==sha(B/'all-b/rom.gba')==build['rom_sha256'];assert sha(B/'all-b/native.elf')==build['elf_sha256']
 old=read(R/'generated/play-validation-0.5.103.json');summary=read(B/'all-b/summary.json');assert not summary['invalid_benchmark_reasons'] and summary['exit_code']==0
 assert sha(B/'baseline/rom.gba')==old['rom_sha256']
 rr=active(B/'all-b');before=active(R/'build/audio103/follow')
 perf=dict(overall=summary['active_play'],m1_active=interval([r for r in rr if r['scene']=='0']),m2_observed=interval([r for r in rr if r['scene']=='1']),white_frames=summary['execution']['all_white_game_frames'],game_lcd_frames=summary['execution']['game_lcd_frames'],late_transfers=summary['last']['video.late_transfers'],bg_backend=old['performance']['bg_backend']+'; exact composed8x8 opacity cache and restricted two-plane foreground coverage reuse')
 m1=[r for r in rr if r['scene']=='0'];old_m1=[r for r in before if r['scene']=='0'];ranges=[]
 for lo,hi in [(0,1536),(1552,1660),(1660,1800),(1800,2000),(2000,2400),(2400,2649),(2649,3000)]:
  a=interval([r for r in old_m1 if lo<=int(r['camera_x'])<hi]);z=interval([r for r in m1 if lo<=int(r['camera_x'])<hi]);ranges.append(dict(camera=[lo,hi-1],baseline=a,candidate=z,percent=(z['fps']/a['fps']-1)*100))
 perf['ranges']={f"camera_{v['camera'][0]}_{v['camera'][1]+1}":v['candidate'] for v in ranges}
 trace=[json.loads(l) for l in (B/'all-b/trace.jsonl').read_text().splitlines()];last=trace[-1];assert not last['app.error'] and not last['audio.fault'] and not last['audio.clipped']
 keys=['lcd_frame','world.ticks','world.results.progress','world.weapons.scene_number','session.state']
 transitions=[{k:r[k] for k in keys} for i,r in enumerate(trace) if i and any(r[k]!=trace[i-1][k] for k in keys[2:])]
 assert any(r['session.state']==2 for r in transitions) and any(r['world.results.progress']==1 for r in transitions)
 profiles=[];groups=rows(B/'profile-b/groups.csv');calls=rows(B/'profile-b/calls.csv')
 for w,start in enumerate([7000,11500,25000]):
  group=[r for r in groups if int(r['window'])==w];total=sum(int(r['cycles']) for r in group);timing={}
  for f in ['native_session_frame','native_video_prepare','native_background_prepare','native_object_scene_append']:
   values=[int(r['non_interrupt_cycles'])*1000/HZ for r in calls if int(r['window'])==w and r['function']==f];timing[f]=dict(calls=len(values),mean_non_interrupt_ms=sum(values)/len(values))
  profiles.append(dict(lcd_start=start,lcd_end=start+120,group_percent={r['group']:int(r['cycles'])*100/total for r in group},timing=timing))
 frontend=[]
 for name in ['tutorial','pause','link-cancel','demo']:
  p=B/'frontend'/name;execution=read(p/'execution.json');t=[json.loads(l) for l in(p/'trace.jsonl').read_text().splitlines()];r=t[-1]
  frontend.append(dict(name=name,exit_code=execution['exit_code'],lcd=r['lcd_frame'],tick=r['world.ticks'],error=f"0x{r['app.error']:08x}",audio_fault=r['audio.fault']))
 assert all(x['exit_code']==0 for x in frontend[:-1]);assert frontend[-1]['error']=='0x0b1e0000'
 sizes={name:int(re.search(r'^'+re.escape(name)+r'\s+(\d+)',build['section_sizes'],re.M).group(1)) for name in ['.bss','.iwram']}
 memory=dict(rom_bytes=build['rom_bytes'],ewram_used=sizes['.bss'],ewram_free=262144-sizes['.bss'],iwram_before_stack_used=sizes['.iwram'],iwram_before_reserved_stack_free=0x6700-sizes['.iwram'],main_stack_reserved=4096,irq_stack_reserved=2208)
 ram=(B/'all-b/iwram.bin').read_bytes();sentinel=struct.pack('<I',0xa55a3cc3)
 for key,lo,hi in [('observed_main_stack',0x6700,0x7700),('observed_irq_stack',0x7700,0x7fa0)]:
  first=next((i for i in range(lo,hi,4) if ram[i:i+4]!=sentinel),hi);memory[key]=hi-first
 memory.update(read(B/'struct-sizes.json'))
 memory['limitation']='Sentinel high-water in this route only; not a proof for all scenes or 2P.'
 assert memory['ewram_free']>=0 and memory['iwram_before_reserved_stack_free']>=0
 checks={}
 for name in ['regressions','regressions-b','capture-test-final','capture-asan','render-asan','object-hardware']:
  checks[name]=(B/(name+'.log')).read_text();assert not any(k in checks[name] for k in ['ERROR: AddressSanitizer','runtime error:','Assertion','Traceback'])
 hardware=[]
 for n in [4800,7000,11500,25000]:
  p=B/f'snapshot-{n}';t=(p/'comparison.txt').read_text();assert 'FIX committed OAM/VRAM/palette mismatches0' in t
  h=read(p/'object-hardware.json');assert not(h['obj_tile_mismatched_bytes'] or h['oam_mismatched_bytes'] or h['palette_mismatched_bytes']);hardware.append(dict(lcd=n,comparison=t,hardware=h))
 changes=[
  dict(name='객체 정렬',files=['src/object_video.c'],detail='HUD prefix/suffix를 따로 취급하고 배우만 byte index 안정 정렬.8개 이하는 작은 인덱스 삽입정렬, 그 외 병합정렬. 같은 우선순위의 원래 순서를 유지한다.'),
  dict(name='소유자 조회',files=['src/object_scene.c'],detail='248바이트 간격의 몫/나머지 중복 계산을 제거. 범위·정렬·저장된 index와 실제 슬롯 주소 일치를 검사하며 Slug 별도 소유권 검사는 유지한다.'),
  dict(name='그리기 해석·strip 재사용',files=['src/object_scene.c','src/object_scene.h','src/source_drawing.c','src/source_strip_budget.c'],detail='20바이트 Animation 대신12바이트 불변 그리기 정보. 최대32개 부품의 배치/완전한strip 수를 재사용. 캐시 초과·잘린strip 계산은 기존 경로로 돌아가고 수집/출력 예산은 별도로 적용한다.'),
  dict(name='배경 가림·opacity',files=['src/background_video.c'],detail='FIX 유휴영역에 generation 검증된256-entry8x8 opacity 캐시. 두 배경의 앞층이 같은8px원점 내에서 변경 없이 움직인 경우 정확한 coverage를 이동하고 노출 가장자리만 갱신. 마지막 층의 불필요한 opacity 계산은 생략한다. 시차·파괴·wrap·세대 변경 시 재계산한다.'),
  dict(name='HUD·객체 팔레트',files=['src/object_video.c','src/fixed_object.c'],detail='기존 호환 은행 공유를 우선하며 새 은행이 필요할 때 이전 색상 위치를 재사용. HUD의 이전 nibble mapping을 다른 은행에서도 검증해 재사용. 가끔 실행되는 글자 patch 코드는 ROM으로 옮겨 IWRAM을 확보했다. 색상 단순화는 추가하지 않았다.'),
  dict(name='탄피 생성 축소',files=['src/weapons.c'],detail='권총/로켓 탄피의 수평·위쪽4종만(frame_phase+parent.index)&3==0인 위상에서 생성. 해당4개 원본 animation graph는 WAIT/DRAW/END만 포함함을 확인했다. 추가 난수 호출은 없다. 실제 탄피 감소율은 발사 시점에 따라 다르며 장식 할당/부모 정리 시점까지 동일하다는 주장은 하지 않는다.'),
  dict(name='OBJ 타일 전송',files=['src/object_video_gba.c'],detail='직전 실제 커밋과 ROM주소·길이·VRAM위치가 모두 같은 불변 타일의 DMA를 생략. HUD 등 RAM의 가변 데이터는 항상 전송한다. 실제4개 화면의 OBJ타일/OAM/팔레트 바이트 대조를 통과했다.'),
 ]
 catalog=json.loads(next(l for l in checks['regressions'].splitlines() if l.startswith('{"assets"')))
 report=dict(version='0.5.104',rom_sha256=ROM,elf_sha256=build['elf_sha256'],fingerprint=build['fingerprint'],verified_campaign_scope=old['verified_campaign_scope'],complete_port=False,m1_clear_verified=True,target_fps=30,target_met=False,fixed_frame_bytes=old['fixed_frame_bytes'],input_method=old['input_method'],lcd_clock=old['lcd_clock'],object_palette_policy=old['object_palette_policy']+'; stable compatible actor bank reuse and exact HUD mapping reuse across bank relocations',visual_tradeoffs=old['visual_tradeoffs']+['권총/로켓 수평·위쪽 탄피4종의 생성 위상을4개 중1개로 제한. 실제 총탄·판정·발사 주기는 변경하지 않았다. 장식 할당은 달라진다.'],audio_settings=old['audio_settings'],catalog_decode=catalog,audible_output=dict(captured_this_revision=False,audio_fault=last['audio.fault'],clipped=last['audio.clipped'],limitation='No new listening/waveform capture; unchanged6000Hz/4SFX settings, catalog and1056000-sample host differential rerun.'),pcm_cache=old['pcm_cache'],memory=memory,scene_transitions=transitions,profiles=profiles,performance=perf,implemented_optimizations=changes,comparison=dict(baseline_version='0.5.103',baseline_rom_sha256=old['rom_sha256'],baseline_performance=old['performance'],baseline_profiles=old['profiles'],m1_camera_ranges=ranges,method='Same state-aware controller policy and36000LCD budget. Baseline is frozen0.5.103 recording, not rerun this revision. Both builds input-only.',limitation='Battle dwell, cosmetic allocations and final M2 positions differ. Profiles are at the same LCD times, not identical world states. Seven changes are measured as a bundle, not seven isolated FPS gains.',initial_candidate_fps=read(B/'all-a/summary.json')['active_play']['fps']),frontend_results=frontend,frontend_verified_paths=['logo/title','first demo/ranking before existing second-demo failure','tutorial/game','pause/resume','no-peer link cancel','M1 results/clear to M2 entry'],hardware_snapshots=hardware,regressions=checks,priority='Reach nominal30FPS: occasional HUD full rebuilds/actor palette pressure, M1 logic and M2 background; remaining14whiteframes/late15, second-demo fault, fullcampaign/physical2P',evidence='docs/performance-0.5.104/')
 write(R/'generated/play-validation-0.5.104.json',report);write(D/'measurement.json',report)
 for source,name in [('all-b','play'),('profile-b','profile'),('frontend','frontend')]:archive(B/source,D/(name+'.zip'))
 with zipfile.ZipFile(D/'snapshots.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for n in [4800,7000,11500,25000]:
   for p in (B/f'snapshot-{n}').iterdir():
    if p.is_file() and p.suffix in ['.bin','.json','.txt','.log','.png','.c']:z.write(p,f'{n}/{p.name}')
 for n in ['regressions','regressions-b','capture-test-final','capture-asan','render-asan','object-hardware']:shutil.copy2(B/(n+'.log'),D/(n+'.log'))
 shutil.copy2(B/'casing-audit.json',D/'casing-audit.json');shutil.copy2(B/'all-b/build.json',D/'build.json');shutil.copy2(B/'diagnosis.c',D/'diagnosis.c')
 write(D/'initial-candidate.json',read(B/'all-a/summary.json'))
 shutil.copy2(R/'generated/play-validation-0.5.103.json',D/'baseline-measurement.json')
 diff=[]
 for p in sorted((B/'baseline/src').glob('*')):
  after=R/'src'/p.name
  if p.read_bytes()!=after.read_bytes():diff.extend(difflib.unified_diff(p.read_text().splitlines(True),after.read_text().splitlines(True),fromfile='0.5.103/src/'+p.name,tofile='0.5.104/src/'+p.name))
 (D/'changes.patch').write_text(''.join(diff))
 percent=(perf['overall']['fps']/old['performance']['overall']['fps']-1)*100
 table=''.join(f'<tr><td>{label}</td><td>{old["performance"][key]["fps"]:.2f}</td><td><b>{perf[key]["fps"]:.2f}</b></td><td>{(perf[key]["fps"]/old["performance"][key]["fps"]-1)*100:+.2f}%</td></tr>' for key,label in [('overall','약10분 전체'),('m1_active','M1'),('m2_observed','M2 관측 구간')])
 range_table=''.join(f'<tr><td>{x["camera"][0]}–{x["camera"][1]}</td><td>{x["baseline"]["fps"]:.2f}</td><td>{x["candidate"]["fps"]:.2f}</td><td>{x["percent"]:+.2f}%</td></tr>' for x in ranges)
 cards=''.join('<article><h2>'+html.escape(x['name'])+'</h2><p>'+html.escape(x['detail'])+'</p><p class="code">'+html.escape(' · '.join(x['files']))+'</p></article>' for x in changes)
 pics=''.join(f'<figure><img width="240" height="160" src="data:image/png;base64,{base64.b64encode((B/f"snapshot-{n}/screen.png").read_bytes()).decode()}"><figcaption>실제 재생 화면 · LCD{n}</figcaption></figure>' for n in [7000,25000])
 text=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Metal Slug GBA0.5.104 · 성능 개선 적용 결과</title><style>body{{margin:0;background:#10191e;color:#edf1f3;font:16px/1.8 system-ui,sans-serif}}main{{max-width:1000px;margin:auto;padding:40px 24px}}h1{{font-size:38px;line-height:1.3}}h2{{font-size:23px;color:#a7e7d0}}article{{border:1px solid #344952;background:#192930;padding:22px;margin:18px 0;border-radius:12px}}table{{width:100%;border-collapse:collapse}}th,td{{text-align:left;padding:12px;border-bottom:1px solid #344952}}.muted{{color:#b6c6ce}}.code{{font:12px/1.8 monospace;overflow-wrap:anywhere}}b{{color:#bcebd7}}.lead{{font-size:22px}}.pictures{{display:flex;flex-wrap:wrap;gap:18px}}figure{{margin:0;flex:1;min-width:240px}}img{{width:100%;height:auto;image-rendering:pixelated}}figcaption{{color:#b6c6ce;font-size:13px}}.scroll{{overflow:auto}}@media(max-width:600px){{h1{{font-size:30px}}}}</style><main><p class="muted">METAL SLUG · GBA NATIVE ·0.5.104</p><h1>일곱 후보 적용 후<br>평균22.34 →23.21fps</h1><p class="lead">약10분 자동 플레이에서 <b>{percent:.2f}% 향상</b>. 평균30fps 목표는 아직 달성하지 못했습니다.</p><table><tr><th>범위</th><th>0.5.103</th><th>0.5.104</th><th>변화</th></tr>{table}</table><p class="muted">mGBA0.10.2에서 실제GBA 클록 기준 신규 커밋 수를 측정했습니다. 호스트PC의 실행 배속이나 중복 표시 프레임을 FPS로 세지 않습니다. 동일 자동 조작 정책·36,000LCD프레임 예산이지만 전투 체류·장식 할당·마지막 M2 위치가 달라 완전히 같은 전투 상태의 비교는 아닙니다. 전 캠페인 평균도 아닙니다.</p><h2>적용한 변경</h2>{cards}<h2>M1 같은 카메라 범위</h2><table><tr><th>카메라X</th><th>기존fps</th><th>적용fps</th><th>변화</th></tr>{range_table}</table><p>2400–2648과2649–2999 혼잡 구간은 개선됐습니다. 그러나 초반과1660–1799 등 일부 구간에서는 소폭 하락했습니다. 모든 장면이 빨라졌다고 판단하지 않습니다. 같은 범위라도 적·이펙트 상태는 다를 수 있습니다.</p><h2>남은 병목</h2><p>M2 표본의 배경 평균 처리시간은15.81→{profiles[2]['timing']['native_background_prepare']['mean_non_interrupt_ms']:.2f}ms였습니다. 반면 M1 LCD7000 표본에서는 HUD 캐시가 해제되어 전체 글자 재구성이 발생했고, 비디오 처리가19.84→{profiles[0]['timing']['native_video_prepare']['mean_non_interrupt_ms']:.2f}ms로 커졌습니다. 표본 시점의 전투 상태가 달라 이 값을 순수 구현 속도비로 해석하지 않습니다. 다음 큰 후보는 HUD 전체 재구성을 일으키는 팔레트/용량 압박과 M1 로직, M2 배경입니다.</p><h2>검증 범위</h2><p>M1 결과 화면·클리어 후 M2 진입 및 일부 진행을 확인했습니다. 실제 화면4곳의 BG/HUD RGB·우선순위, OBJ타일·OAM·팔레트 전송 바이트가 모두 일치했습니다. 호스트에서 배경780프레임, FIX90프레임, 팔레트1200프레임, strip488,332경우, 그리기108,000회와 보호 팔레트·OAM 정렬을 검사했습니다. 배경/그리기 AddressSanitizer·UndefinedBehaviorSanitizer 검사도 통과했습니다.</p><div class="pictures">{pics}</div><p>튜토리얼·게임, 일시정지/복귀, 상대 없는 링크 대기 취소는 통과했습니다. 기존 두 번째 데모는scene4/tick112,0x0b1e0000으로 실패합니다. M2 클리어 이후·전체 보스/엔딩·실제GBA2P는 검증하지 않았습니다.</p><p><b>흰 화면14/35,859프레임, 늦은 전송15회는 그대로입니다.</b> OBJ 중복 전송 생략이 이 문제를 해결했다는 증거는 없습니다.</p><h2>메모리·음향</h2><p>EWRAM{memory['ewram_used']:,}바이트, 여유{memory['ewram_free']}바이트. IWRAM{memory['iwram_before_stack_used']:,}바이트, 예약 스택 전 여유{memory['iwram_before_reserved_stack_free']}바이트. 관측 main stack{memory['observed_main_stack']}/4096, IRQ{memory['observed_irq_stack']}/2208바이트입니다. 모든 장면의 최댓값을 보장하지 않습니다.</p><p>75% 그래픽·위4/아래4픽셀 크롭, 음악1개+효과음4개, 내부6,000Hz는 유지했습니다. 음원192개 디코드와1,056,000샘플 차등 검사는 재실행했지만 새 청취/파형 녹음은 하지 않았습니다. 새 시각적 희생은 위에 명시한4종 탄피 생성 축소입니다.</p><h2>비교와 재현</h2><p>첫 묶음22.76fps에서 HUD를 제외한 정렬·객체 팔레트 위치 재사용을 추가해23.21fps를 선택했습니다. 일곱 항목 각각을 분리한 FPS 기여도 측정은 아닙니다.0.5.103 기준은 보관된 측정 기록을 사용했습니다.</p><p class="code">ROM SHA-256: {ROM}<br>ELF SHA-256: {build['elf_sha256']}<br>Build fingerprint: {build['fingerprint']}</p></main></html>'''
 (R/'docs/performance-0.5.104.ko.html').write_text(text)
 readme='''# 0.5.104 측정 재현

최종은 build/perf104/all-b에 보관한ROM/ELF와 연결된 probe/symbols/input이다. baseline은0.5.103의 보관된36000LCD 실행이며 이번에 재실행하지 않았다. 입력만 조작하고 게임RAM/레지스터/속도/진행을 주입하지 않는다. 첫all-a22.756FPS는 중간 실험으로만 기록한다.

```sh
python3 tools/build_native.py --cc /path/to/arm-none-eabi-gcc --incremental
python3 tools/native_probe.py --cc /path/to/arm-none-eabi-gcc --elf build/native/native.elf
python3 tools/prepare_followthrough_probe.py --cc /path/to/arm-none-eabi-gcc --output build/repro104/probe
python3 tools/validate_heli_performance.py --output build/repro104/play --probe build/repro104/probe/probe.txt --mgba-prefix /path/to/mgba/usr --frames36000 --mode8
python3 tools/performance_regressions.py
```

위명령의 --frames36000/--mode8은 각각 `--frames 36000`, `--mode 8`로 띄어쓴다. Profiler diagnosis.c는 libmgba로 host컴파일 후 ROM,probe,symbols,inputs,output,25120 순서로 실행한다. LCD7000/11500/25000의120프레임 창. CPU 함수/그룹은 포함관계가 있어 합산하지 않는다. 같은 LCD라도 세계 상태는 다르다.

실화면은 tools/verify_native_snapshot.py --rom/--inputs/--probe/--frame/--output으로 읽기 전용 캡처하고 tools/verify_object_snapshot.py --cc/--elf/--rom [directories]로 실제 OBJ전송을 검사한다. hud-count.bin을 만들고 native_snapshot_equivalence.c를 재실행하면 캐시 없는HUD도 검증된다. snapshot은 현재헤더와 일치하는 ELF를 사용해야 한다. 물리GBA/2P 검증은 아니다.
'''.replace('--frames36000 --mode8','--frames 36000 --mode 8').replace('위명령의 --frames36000/--mode8은 각각 `--frames 36000`, `--mode 8`로 띄어쓴다. ','')
 (D/'README.ko.md').write_text(readme)
 checkpoint=f'''# 0.5.104 성능 후보 적용

사용자 요청에 따라 정렬·owner조회·그리기/strip 재사용·배경coverage/opacity·HUD팔레트·탄피 생성·OBJ 전송7개 후보를 적용했다. 최종 약10분 평균22.3371→23.2142fps(+{percent:.2f}%). M1{perf['m1_active']['fps']:.4f}, M2관측{perf['m2_observed']['fps']:.4f}. 평균30목표 미달성. 일부 초반/1660구간 소폭 하락을 보고서에 포함했다.

M1결과/클리어→M2일부 진행. 게임/audio오류0, clipped0. 흰14/35859·late15 그대로, 두 번째데모scene4/tick112/0x0b1e0000 그대로. M2클리어·전캠페인·헬기격파·실기2P 완료 주장은 하지 않는다. 동일컨트롤러 정책이나 전투체류·장식할당·최종위치가 다르다.

기존75%/4+4와audio6000Hz/4SFX 유지. 탄피4종만4위상중1위상 생성; 실제탄환/입력/충돌 빈도는 변경하지 않았다. 실제4화면 BG/HUD RGB/priority 및 OBJ/VRAM/OAM/팔레트 불일치0. 호스트 회귀/배경·그리기ASan+UBSan 통과. 새청취녹음은 없음.

ROM{ROM}, {build['rom_bytes']}bytes. EWRAM{memory['ewram_used']}({memory['ewram_free']}free), IWRAM{memory['iwram_before_stack_used']}({memory['iwram_before_reserved_stack_free']}beforestack). 관측 main{memory['observed_main_stack']}/4096, IRQ{memory['observed_irq_stack']}/2208. build fingerprint{build['fingerprint']}.

근거: generated/play-validation-0.5.104.json, docs/performance-0.5.104.ko.html, docs/performance-0.5.104/. 다음은 M1 HUD 전체재구성/팔레트압박·게임로직과 M2배경. 전체30·흰전송·데모오류·전캠페인·2P는 남았다.
'''
 (R/'docs/checkpoint-0.5.104.ko.md').write_text(checkpoint)
 backlog=(R/'docs/remaining-work-0.5.103.ko.md').read_text();cut=backlog.index('이번 완료:');backlog=backlog[:cut]
 backlog=backlog.replace('# 0.5.103','# 0.5.104').replace('이번 10분 자동 입력 평균22.34fps','이번 10분 자동 입력 평균23.21fps')
 backlog=backlog.replace('0.5.102에서 LCD7000 창의 세션25.46ms·비디오17.37ms가 관측됐다. 이 CPU 비용을 축소.','0.5.104 LCD7000 표본에서 HUD full rebuild와 비디오26.41ms가 관측됐다. 팔레트/용량 압박을 줄이고 로직 비용도 축소.').replace('0.5.102 LCD25000 창의 BG20.20ms·전체 사이클42.4%를 기준으로 반복 계산 개선.','0.5.104 LCD25000 창의 BG12.04ms·전체 사이클30.2%를 기준으로 추가 개선.').replace('EWRAM60바이트 여유, IWRAM 예약 스택 전216바이트','EWRAM308바이트 여유, IWRAM 예약 스택 전440바이트')
 backlog+='''이번 완료: 정렬·owner조회·그리기32부품캐시·배경coverage/opacity·HUD/객체팔레트 안정화·탄피4종 생성 제한·불변OBJ타일 전송 생략.0.5.10322.34→23.21fps(+3.93%), M122.26→23.43, M2관측22.45→23.13. 일부 초반구간은 소폭 하락. 일곱 변경의 묶음 비교이며 개별 기여도는 분리하지 않았다.

실화면4곳 BG/HUD 및실제OBJ타일/OAM/팔레트 불일치0, 배경/그리기 ASan+UBSan과 host 회귀 통과. 튜토리얼·pause/resume·no-peer link cancel 통과. 기존두번째demo오류와흰14/35859·late15 유지.75%4+4 및audio6000Hz/4SFX 유지. 탄피는 공격·음향·RNG 명령 없는4종만 생성위상을 제한했으며 장식할당 동일성을 주장하지 않는다.

현재 자료: docs/checkpoint-0.5.104.ko.md, docs/performance-0.5.104.ko.html, generated/play-validation-0.5.104.json, docs/performance-0.5.104/.

구현·ROM 생성·입력-only 플레이·회귀·성능 검증은 승인되었다. 다음 작업은 M1 HUD full rebuild/팔레트압박/게임로직, M2 BG 추가반복축소이다. 핵심 전투·충돌·75% 배율·지형 가독성을 유지하고 전캠페인/실기2P 완료로 표시하지 않는다.
''';(R/'docs/remaining-work-0.5.104.ko.md').write_text(backlog)
 agents=R/'AGENTS.md';a=agents.read_text();entry=f'- Latest checkpoint2026-11-02 v0.5.104: seven authorized optimization candidates applied, samecontroller36000LCD22.337->23.214FPS(+{percent:.2f}%),M1{perf["m1_active"]["fps"]:.3f},M2observed{perf["m2_observed"]["fps"]:.3f};nominal30NOTmet. HUD-aware stable index sort, ownerindex validation, compact captured draw/32part+strip cache, exact256opacity+restricted foregroundcoverage reuse, actorbank/HUDmappingreuse, four emptycasing spawn phases, previouscommit immutableOBJ DMA skip. All changes bundled; someearlycamera regressions. Preserve75percent4+4/approvedplayer/audio6000Hz4SFX. M1results/clear->M2partial,error0audio0clipped0;white14/35859late15andsecond-demotick1120x0b1e0000remain. Four realBG/HUD/OBJVRAM/OAM/palette snapshots0mismatches;hostregressions+BG/captureASanUBSanpass; tutorial/pause/linkcancelpass. No M2clear/fullcampaign/helidefeat/physical2P claim. EWRAM261836(308free),IWRAM25928(440beforestack),observedmain2640IRQ272. ROM{ROM};next HUDfullrebuild/palettepressure/M1logic/M2BG. Current docs/checkpoint-0.5.104.ko.md,docs/remaining-work-0.5.104.ko.md and docs/performance-0.5.104/. Prior notest prohibitions superseded by current user authorization.\n\n'
 if not a.startswith('# Source-based native port\n\n- Latest checkpoint2026-11-02 v0.5.104:'):agents.write_text(a.replace('# Source-based native port\n\n','# Source-based native port\n\n'+entry,1))
 p=R/'README.ko.md';a=p.read_text();note='현재0.5.104: 7개 성능 후보 적용, 10분 평균23.21fps(+3.93%). M1클리어→M2일부 진행. 평균30fps·흰프레임·두번째데모 오류·전캠페인·실기2P는 남습니다. 상세 docs/checkpoint-0.5.104.ko.md와 docs/performance-0.5.104.ko.html.\n\n'
 if note not in a:p.write_text(note+a)
 print(json.dumps(dict(fps=perf['overall']['fps'],gain_percent=percent,memory=memory,rom_sha256=ROM),ensure_ascii=False))
if __name__=='__main__':main()
