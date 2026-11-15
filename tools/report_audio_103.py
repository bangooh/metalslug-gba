#!/usr/bin/env python3
"""Package the measured 6000Hz/four-effect candidate and its frozen evidence."""
import base64,csv,difflib,hashlib,html,json,shutil,wave,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[1];B=R/'build/audio103';D=R/'docs/audio-performance-0.5.103'
ROM='1afc760d296f78cde7710f4652e5aae0b310c6aa171a968c60d1eb37e59cdf47'
HZ,LCD=16777216,280896
def read(p):return json.loads(p.read_text())
def write(p,value):p.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def interval(rr):
 a,z=rr[0],rr[-1];seconds=(int(z['lcd'])-int(a['lcd']))*LCD/HZ;commits=int(z['commits'])-int(a['commits'])
 return dict(lcd_start=int(a['lcd']),lcd_end=int(z['lcd']),commits=commits,seconds=seconds,fps=commits/seconds)
def pack(source,target):
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=9)as z:
  for p in sorted(source.rglob('*')):
   if p.is_file() and p.suffix in ('.json','.jsonl','.csv','.txt','.log','.ppm','.bin','.s16le'):
    z.write(p,str(p.relative_to(source)))
def main():
 D.mkdir(parents=True,exist_ok=True)
 build=read(R/'generated/native-build.json');assert build['version']=='0.5.103'and sha(Path(build['rom']))==ROM==build['rom_sha256']
 assert sha(B/'follow/rom.gba')==ROM and sha(B/'follow/native.elf')==build['elf_sha256']
 assert (B/'baseline/commits.csv').read_bytes()==(R/'build/perf102/release-play/commits.csv').read_bytes()
 old=read(R/'generated/play-validation-0.5.102.json');s=read(B/'follow/summary.json');assert s['exit_code']==0
 trace=[json.loads(x)for x in (B/'follow/trace.jsonl').read_text().splitlines()];last=trace[-1]
 active=[r for r in rows(B/'follow/commits.csv')if r['playing']=='1'and r['presentation']=='0'and r['demo']=='0']
 active=active[next(i for i,r in enumerate(active)if int(r['live0'])):];assert not any(int(r['paused'])for r in active)
 assert all(int(y['ticks'])>=int(x['ticks'])for x,y in zip(active,active[1:]))
 scenes={i:interval([r for r in active if int(r['scene'])==i])for i in (0,1)}
 keys=['lcd_frame','world.ticks','world.results.progress','world.weapons.scene_number','session.state']
 transitions=[{k:r[k]for k in keys}for i,r in enumerate(trace)if i and any(r[k]!=trace[i-1][k]for k in keys[2:])]
 assert any(r['session.state']==2 for r in transitions)and any(r['world.results.progress']==1 for r in transitions)
 groups=rows(B/'profile/groups.csv');calls=rows(B/'profile/calls.csv');profiles=[]
 for w,start in enumerate((7000,11500,25000)):
  rr=[r for r in groups if int(r['window'])==w];total=sum(int(r['cycles'])for r in rr);timing={}
  for f in ['native_session_frame','native_video_prepare','native_background_prepare','native_object_scene_append']:
   times=[int(r['non_interrupt_cycles'])*1000/HZ for r in calls if int(r['window'])==w and r['function']==f]
   timing[f]=dict(calls=len(times),mean_non_interrupt_ms=sum(times)/len(times))
  profiles.append(dict(lcd_start=start,lcd_end=start+120,group_percent={r['group']:int(r['cycles'])*100/total for r in rr},timing=timing))
 frontend=[]
 for name in ['tutorial','pause','demo','ranking-extended']:
  p=B/'frontend'/name;execution=read(p/'execution.json');t=[json.loads(x)for x in(p/'trace.jsonl').read_text().splitlines()]
  frontend.append(dict(name=name,exit_code=execution['exit_code'],lcd=t[-1]['lcd_frame'],tick=t[-1]['world.ticks'],error=f"0x{t[-1]['app.error']:08x}",audio_fault=t[-1]['audio.fault']))
 assert frontend[0]['exit_code']==frontend[1]['exit_code']==0
 assert frontend[2]['error']==frontend[3]['error']=='0x0b1e0000'
 pcm=read(R/'generated/native-audio-pcm.json');assert pcm['commands']==read(B/'before/generated/native-audio-pcm.json')['commands']
 regression=read(R/'build/performance-regressions/results.json');catalog=json.loads(regression['audio-catalog'])
 audio_settings=dict(nominal_sample_rate=6000,hardware_sample_rate=HZ/2796,timer_cycles=2796,effect_voices=4,music_voices=1,output='signed8 mono',resampled_from='Original pinned source-renderer WAV, not previously compressed 8192Hz audio',voice_policy='Existing stop/group/priority admission with four slots',initial_fifo_ring_latency_ms=288/(HZ/2796)*1000)
 memory=dict(old['memory']);memory.update(rom_bytes=build['rom_bytes'],iwram_before_stack_used=26152,iwram_before_reserved_stack_free=216,observed_irq_stack=272)
 performance=dict(overall=s['active_play'],m1_active=scenes[0],m2_observed=scenes[1],ranges=s['ranges'],white_frames=s['execution']['all_white_game_frames'],game_lcd_frames=s['execution']['game_lcd_frames'],late_transfers=last['video.late_transfers'],bg_backend=old['performance']['bg_backend'])
 report=dict(version='0.5.103',rom_sha256=ROM,elf_sha256=build['elf_sha256'],verified_campaign_scope=old['verified_campaign_scope'],complete_port=False,m1_clear_verified=True,target_fps=30,target_met=False,fixed_frame_bytes=old['fixed_frame_bytes'],input_method=old['input_method'],lcd_clock=old['lcd_clock'],object_palette_policy=old['object_palette_policy'],visual_tradeoffs=old['visual_tradeoffs'],audio_settings=audio_settings,catalog_decode=catalog,audible_output=dict(samples_stereo=s['execution']['audio_samples'],rate=s['execution']['audio_rate'],seconds=s['execution']['audio_samples']/s['execution']['audio_rate'],peak=s['execution']['audio_peak'],audio_fault=last['audio.fault'],limitation='Captured waveform; full-campaign listening and physical GBA unverified'),pcm_cache={k:v for k,v in pcm.items()if k!='commands'},timing_check=read(B/'timing-check.json'),memory=memory,scene_transitions=transitions,profiles=profiles,performance=performance,baseline_replayed_commit_rows_identical=True,comparison=dict(baseline_version='0.5.102',baseline_performance=old['performance'],baseline_profiles=old['profiles'],fixed_tick_windows=read(B/'matched-ticks.json'),method='Identical state-aware automatic controller; baseline independently replayed and commits exactly reproduced. The literal LCD-key candidate replay paused/restarted and is excluded from FPS claims.',limitation='Time spent in each battle and final M2 position differ; same-camera and simulation-tick comparisons are supplementary, not identical complete game states.'),frontend_results=frontend,frontend_verified_paths=['logo/title','first demo/ranking before existing second-demo failure','tutorial/game','pause/resume','M1 results/clear to M2 entry'],priority=old['priority'],evidence='docs/audio-performance-0.5.103/')
 write(R/'generated/play-validation-0.5.103.json',report);write(D/'measurement.json',report)
 for label in ['baseline','follow','profile','frontend','replay']:pack(B/label,D/(label+'.zip'))
 for name in ['timing-check.json','matched-ticks.json','regressions.log']:shutil.copy2(B/name,D/name)
 for source,name in [(R/'generated/native-build.json','build.json'),(B/'before/generated/native-audio-source.json','baseline-audio-source.json'),(B/'before/generated/native-audio-pcm.json','baseline-audio-pcm.json'),(R/'build/perf102/profile-release.zip','baseline-profile.zip')]:shutil.copy2(source,D/name)
 # Keep the actual profiler source at a location where its relative include resolves.
 driver=(B/'diagnosis.c').read_text().replace('unmodified v0.5.102 ROM','unmodified v0.5.103 ROM');(D/'diagnosis.c').write_text(driver)
 diff=[]
 for before in sorted((B/'before').rglob('*')):
  rel=before.relative_to(B/'before');after=R/rel
  if before.is_file()and after.is_file()and before.suffix in ('.c','.h','.py'):
   diff.extend(difflib.unified_diff(before.read_text().splitlines(True),after.read_text().splitlines(True),fromfile='0.5.102/'+str(rel),tofile='0.5.103/'+str(rel)))
 (D/'changes.diff').write_text(''.join(diff))
 clips={}
 for label,filename in [('baseline','audio-before-8192hz-8sfx.wav'),('follow','audio-after-6000hz-4sfx.wav')]:
  out=D/filename
  with wave.open(str(out),'wb')as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(32768);w.writeframes((B/label/'audio.s16le').read_bytes())
  clips[label]=base64.b64encode(out.read_bytes()).decode()
 delta=(performance['overall']['fps']/old['performance']['overall']['fps']-1)*100
 comparison=[('10분 전체',old['performance']['overall']['fps'],performance['overall']['fps']),('M1 관측 구간',old['performance']['m1_active']['fps'],scenes[0]['fps']),('M2 관측 구간',old['performance']['m2_observed']['fps'],scenes[1]['fps'])]
 table='| 구간 | 0.5.102 · 8,192Hz/8개 | 0.5.103 · 6,000Hz/4개 |\n|---|---:|---:|\n'+''.join(f'| {n} | {a:.2f}fps | {b:.2f}fps |\n'for n,a,b in comparison)
 ranges=[('1552–1659','camera_1552_1660'),('1660–1799','camera_1660_1800'),('1800–1999','camera_1800_2000'),('2000–2399','camera_2000_2400'),('2400–2648','camera_2400_2649'),('2649–2999','camera_2649_3000')]
 ranges_table='| M1 카메라 X | 이전 | 변경 후 |\n|---|---:|---:|\n'+''.join(f"| {n} | {old['performance']['ranges'][k]['fps']:.2f}fps | {performance['ranges'][k]['fps']:.2f}fps |\n"for n,k in ranges)
 profile_table='| LCD 계측 창 | 이전 오디오 IRQ | 변경 후 오디오 IRQ |\n|---|---:|---:|\n'+''.join(f"| {p['lcd_start']}–{p['lcd_end']-1} | {a['group_percent']['audio_irq']:.2f}% | {p['group_percent']['audio_irq']:.2f}% |\n"for a,p in zip(old['profiles'],profiles))
 note='같은 자동 조작 정책으로 각각 36,000 LCD프레임을 실행했다. 입력은 버튼만 사용했고 게임 RAM·HP·진행 상태는 수정하지 않았다. FPS는 호스트 PC 속도가 아닌 에뮬레이트된 GBA 시간당 커밋 수이다. 전투 체류 시간과 마지막 M2 위치는 조금 다르므로 동일한 전체 게임 상태의 대조 실험은 아니다.'
 common=f'''# Metal Slug GBA Native 0.5.103 — 6kHz·효과음 4개 성능 측정

평균 **21.52 → 22.34fps, +{delta:.2f}%**. M1 클리어와 M2 진입을 다시 확인했다. **명목30fps 목표는 아직 미달성**이며 전 캠페인 완료판이 아니다.

{table}
{note}

## 오디오 변경

- 음악 1개는 유지하고 효과음 동시 재생을 8개에서 4개로 제한했다. 기존 stop/group/priority 규칙으로 교체·거절한다. 혼잡할 때 낮은 우선순위 소리 일부가 생략된다.
- 192개 음원을 원본 렌더러 WAV에서 6,000Hz로 재변환했다. 압축된 8,192Hz 파일을 늘이거나 타이머만 느리게 하지 않았다. 출력은 기존 signed8 mono이다. 고음 선명도는 줄어든다.
- 타이머 주기2796, 실제 약6000.435Hz(명목 오차+0.00725%). 재생 길이·루프·응답 위치의 변환 오차는 최대0.146ms다. 음악 음높이·속도가 약27% 낮아지는 변경은 아니다.
- 동일168개 짧은 효과음의 정확 PCM 캐시는4,465,596→3,270,744바이트다. 전체 IMA는9,435,656→6,910,905바이트. ROM은31,663,236→27,943,516바이트다.
- FIFO/링의 초기 무음288샘플을 유지해 이 지연분은35.16→48.00ms(+12.84ms)가 된다. 실제 명령 지연은 IRQ/큐 시점에도 영향을 받는다. 긴 버퍼는 혼잡 구간의 underrun 여유를 유지하기 위한 선택이다.
- 이번에는 그래픽·전투·충돌 코드를 추가로 바꾸지 않았다. 기존75%·위4/아래4픽셀 크롭과 성능용 시각 축소는 유지한다.

## 헬기 이후 같은 카메라 구간

{ranges_table}
각 행의 커밋 증가량은 이전/변경 후가 같았다. 카메라·생존 상태 외 내부 상태까지 완전 동일하다는 뜻은 아니다. M1 tick1–5200 구간은20.98→22.02 simulation fps였다.

## 오디오 CPU 비용

{profile_table}
실제 게임의120 LCD프레임 창에서 읽기 전용 명령 계측을 했다. 같은 LCD 시점이지만 게임 진행 속도가 달라 소리·객체의 조합도 달라질 수 있다. 이를 통제된 순수 믹서 비용 감소율로 해석하지 않는다. 오디오가 줄어도 게임 로직·배경·객체 렌더링 비용이 남아 전체 FPS 증가는 제한적이다.

## 검증과 한계

- 기존0.5.102를 기록된 버튼 입력으로 재실행한 결과, 전체 커밋 CSV가 기존 증거와 바이트 단위로 같았다.
- 변경 ROM에 같은 LCD 시점 버튼을 그대로 재생한 별도 실험은 START가 일시정지를 만들고 라운드가 재시작됐다. 이 실행의 전체 평균은 비교에서 제외했다. 최종 비교는 같은 상태 기반 자동 조작 정책의10분 실행이다.
- 최종 활성600.795초·13,420커밋, world.tick13,419, app.error0, audio.fault0, clipped0. M1 RESULTS는LCD14520/tick5293, M2 PLAY는LCD15420/tick5692에서 관측했다.
- 전체192음원·13,821,728샘플 디코드 오류0,299응답/11루프의 변환·IMA 루프 상태 검사 통과. 1,056,000샘플 PCM 캐시/IMA 참조 차등 일치. 기존 renderer/strip/palette/integration 회귀 검사도 통과했다.
- 튜토리얼→게임·일시정지/복귀 통과. 첫 데모/랭킹 후 기존 두 번째 데모 scene4/tick112/error0x0b1e0000은 계속 발생한다. 실기·전 캠페인 청취·M2 클리어 이후·헬기 격파·실기2P는 미검증이다.
- 흰 LCD프레임14/35,859, 늦은 전송15로 기존과 동일하다.
- EWRAM262,084바이트(60여유), IWRAM26,152바이트(예약 스택 전216여유). 관측 main stack2624/4096, IRQ272/2208. 모든 장면의 최대 사용량 보장은 아니다.

ROM SHA-256: `{ROM}`. 승인 player.tiles SHA-256: `ca7febc19eeda7cf62af36f036cdb91f290bf8be8ac279344e03bf269c74be32`.

자세한 수치: `generated/play-validation-0.5.103.json`. 원자료/재현: `docs/audio-performance-0.5.103/`. 남은 작업: `docs/remaining-work-0.5.103.ko.md`.
'''
 (R/'docs/checkpoint-0.5.103.ko.md').write_text(common)
 backlog=(R/'docs/remaining-work-0.5.102.ko.md').read_text().replace('0.5.102','0.5.103').replace('이번 10분 자동 입력 평균21.52fps','이번 10분 자동 입력 평균22.34fps').replace('IWRAM 예약 스택 전104바이트','IWRAM 예약 스택 전216바이트')
 backlog=backlog[:backlog.index('이번 완료:')]+'''이번 완료: 음악1개+효과음4개, 원본 음원에서6,000Hz 재변환, 루프/응답/페이드 속도 유지. 10분 평균21.52→22.34fps(+3.79%), M1 21.25→22.25, M2관측21.79→22.45. 오디오IRQ는 세 표본창에서12.0–18.0%→8.8–14.2%. 같은 자동 조작 정책이며 전투 체류 시간은 조금 다르다. 30fps 목표는 미달성이다. 고음 선명도·혼잡시 낮은 우선순위 SFX가 줄고 FIFO/링 초기 지연분은약12.84ms 늘어난다.

0.5.102의 시각/배경/팔레트 최적화와75%·위4/아래4픽셀 크롭은 유지한다. 이번에는 음원192개/299응답/11루프, PCM 캐시 차등1056000샘플, 호스트 회귀 검사, 실제10분 M1→M2, 튜토리얼·일시정지/복귀를 확인했다. 실화면 픽셀 대조4곳·ASan은0.5.102의 이전 증거이며 이번에 재실행했다고 표시하지 않는다. 흰 프레임14/35859·late15·두 번째 데모 오류는 남는다.

현재 자료: docs/checkpoint-0.5.103.ko.md, docs/audio-performance-0.5.103.ko.html, generated/play-validation-0.5.103.json, docs/audio-performance-0.5.103/. LCD 시점 버튼을 그대로 재생한 후보 실험은 일시정지/라운드 재시작 때문에 성능 비교에서 제외했다. 최종 기준은같은 자동 조작 정책의follow 실행이다.

구현·ROM 생성·입력-only 플레이·회귀·성능 검증은 승인되어 있으므로 다시 진행 허가를 받지 않는다. 다음 작업은 M1의 그림 수집/strip/팔레트/게임 로직과 M2 BG 반복 계산이다. 핵심 전투·충돌·75% 배율·지형 가독성을 유지한다. 전 캠페인/실기2P 완료로 표시하지 않는다.
'''
 backlog=backlog.replace('LCD7000 창의 세션25.46ms·비디오17.37ms CPU를 축소.','0.5.102에서 LCD7000 창의 세션25.46ms·비디오17.37ms가 관측됐다. 이 CPU 비용을 축소.').replace('LCD25000 창 BG20.20ms·전체 사이클42.4% 개선.','0.5.102 LCD25000 창의 BG20.20ms·전체 사이클42.4%를 기준으로 반복 계산 개선.')
 (R/'docs/remaining-work-0.5.103.ko.md').write_text(backlog)
 body=''.join(f'<tr><td>{n}</td><td>{a:.2f}</td><td>{b:.2f}</td><td>+{(b/a-1)*100:.1f}%</td></tr>'for n,a,b in comparison)
 profile_body=''.join(f"<tr><td>{p['lcd_start']}–{p['lcd_end']-1}</td><td>{a['group_percent']['audio_irq']:.2f}%</td><td>{p['group_percent']['audio_irq']:.2f}%</td></tr>"for a,p in zip(old['profiles'],profiles))
 page=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>6kHz 오디오 성능 측정 · 0.5.103</title>
<style>body{{margin:0;background:#10171c;color:#e8eef0;font:16px/1.75 system-ui,sans-serif}}main{{max-width:940px;margin:auto;padding:48px 24px}}h1{{font-size:40px;line-height:1.3}}h2{{margin-top:40px;color:#9ce7cf}}.lead{{font-size:22px}}.muted{{color:#b1bec4}}table{{border-collapse:collapse;width:100%;margin:22px 0}}td,th{{padding:12px;text-align:left;border-bottom:1px solid #33434b}}.cards{{display:grid;grid-template-columns:1fr 1fr;gap:20px}}article{{padding:20px;background:#1c2930;border-radius:12px}}audio{{width:100%}}code{{overflow-wrap:anywhere;font-size:12px}}@media(max-width:600px){{.cards{{grid-template-columns:1fr}}h1{{font-size:30px}}}}</style>
<main><p class="muted">METAL SLUG · GBA NATIVE · 0.5.103</p><h1>효과음 4개·6kHz로<br>평균 FPS 약3.8% 개선</h1><p class="lead">21.52 → <strong>22.34fps</strong></p><p>음악1개는 유지했습니다. M1 클리어 후 M2 진입을 다시 확인했습니다. <strong>30fps 목표는 아직 미달성</strong>입니다.</p>
<table><thead><tr><th>관측 구간</th><th>이전 FPS</th><th>변경 FPS</th><th>차이</th></tr></thead><tbody>{body}</tbody></table><p class="muted">{note}</p>
<h2>무엇이 달라졌나</h2><p>전체192음원을 원본 렌더러 WAV에서6,000Hz로 다시 변환했습니다. 효과음은 기존8개에서4개로 제한하며 기존 우선순위 규칙을 유지합니다. 고음의 선명도와 혼잡할 때 일부 낮은 우선순위 효과음이 줄어듭니다. 음악 속도와 음높이를 낮추는 방식은 사용하지 않았습니다.</p><p class="muted">타이머 실제6000.435Hz, 오차+0.00725%. 재생 길이·루프·응답 변환 오차 최대0.146ms. 기존288샘플 FIFO/링의 초기 지연분은35.16→48.00ms입니다. ROM31.66→27.94MB. 그래픽과 전투 코드는 이번에 추가 변경하지 않았습니다.</p>
<h2>실제 출력 비교</h2><p>각 자동 플레이의 LCD3000–4199에서 캡처한약20.09초입니다. 서로 진행 속도가 달라 완전히 같은 소리 조합은 아닙니다. 두 파일의32768Hz는 에뮬레이터 캡처 출력 형식이며 게임 내부 음원은 각각8192/6000Hz입니다.</p><div class="cards"><article><h3>이전 · 8,192Hz / 8개</h3><audio controls preload="none" src="data:audio/wav;base64,{clips['baseline']}"></audio></article><article><h3>변경 · 6,000Hz / 4개</h3><audio controls preload="none" src="data:audio/wav;base64,{clips['follow']}"></audio></article></div>
<h2>오디오 처리 비용</h2><table><thead><tr><th>LCD 계측 창</th><th>이전 IRQ 비중</th><th>변경 IRQ 비중</th></tr></thead><tbody>{profile_body}</tbody></table><p class="muted">각120 LCD프레임의 전체CPU 사이클 대비 비중. 진행 속도에 따라 소리·객체 조합이 달라지므로 순수 믹서 비용 감소율의 통제 실험은 아닙니다. 남은 주요 비용은 게임 로직·배경·객체 처리입니다.</p>
<h2>검증</h2><p>10분 경로의 게임/오디오 오류0, 음원192개 전체 디코드 오류0,299응답·11루프 검사 통과, PCM 캐시105만6천샘플 차등 일치. 튜토리얼→게임과 일시정지/복귀를 확인했습니다.</p><p>흰 LCD프레임14/35,859와 늦은 전송15는 기존과 같습니다. 기존 두 번째 데모 scene4/tick112 오류도 남아 있습니다. M2 클리어 이후·전 캠페인 청취·헬기 격파·실기2P는 미검증입니다.</p><p class="muted">기존ROM의 버튼 기록 재실행은 이전 커밋CSV와 완전히 일치했습니다. 변경ROM에 동일 LCD 버튼 기록을 적용한 별도 실험은 일시정지/재시작으로 경로가 달라져 평균 비교에서 제외했습니다. 위 성능은 같은 상태 기반 자동 조작 정책의 최종 실행입니다.</p><p>ROM SHA-256<br><code>{ROM}</code></p></main></html>'''
 (R/'docs/audio-performance-0.5.103.ko.html').write_text(page)
 (D/'README.ko.md').write_text('''# 오디오103 재현

최종 ROM/ELF 해시와 입력/커밋/원시 캡처는 build.json, follow.zip을 따른다. baseline.zip은0.5.102 기록 입력의 재실행이며 기존 커밋CSV와 바이트 단위로 일치한다. profile.zip은후보, baseline-profile.zip은0.5.102의 읽기 전용 명령 계측이다. replay.zip은 일시정지/재시작 때문에 성능 비교에서 제외된 문자 그대로의LCD키 재생 실험이다.

```sh
python3 tools/import_full_audio.py --upstream /path/to/upstream --rom /path/to/mslug.zip --sample-rate 6000
python3 tools/import_audio_pcm_cache.py
python3 tools/audit_native_audio_assets.py
python3 tools/verify_audio_rate.py --baseline docs/audio-performance-0.5.103/baseline-audio-source.json --output build/timing-check.json
python3 tools/build_native.py --cc /path/to/arm-none-eabi-gcc --incremental
python3 tools/native_probe.py --cc /path/to/arm-none-eabi-gcc --elf build/native/native.elf
python3 tools/prepare_followthrough_probe.py --cc /path/to/arm-none-eabi-gcc --output build/repro103/probe
CAPTURE_AUDIO=1 python3 tools/validate_heli_performance.py --output build/repro103/play --probe build/repro103/probe/probe.txt --mgba-prefix /path/to/mgba/usr --frames 36000 --mode 8
python3 tools/performance_regressions.py
```

profiler: diagnosis.c를 host cc -O2, mGBA include/lib와-lmgba로 빌드한다. ROM, probe.txt, symbols.txt, inputs.csv, 출력폴더, 25120순서로 실행한다. LCD7000/11500/25000의120프레임창을 측정한다. DMA전용watch는 비활성이다. emulator library경로를 LD_LIBRARY_PATH에 지정한다. frontend mode2는튜토리얼5000LCD, mode5는pause1500LCD, mode0은자동데모5200LCD이며 기존second-demo오류가 예상된다. mode3은랭킹이다.

성능은실제GBAclock에 대한 커밋fps이며호스트실행속도가 아니다. 오디오원시캡처는 signed16 stereo32768Hz로 게임내부6000Hz와 구분한다. 물리GBA및2P검증아니다. 새snapshot/ASan을재실행하지 않았으며 이전102의근거와구분한다.
''')
 readme=R/'README.ko.md';text=readme.read_text();marker='## 이전 보존본의 작업 기록 (현재 상태는 위 설명 기준)';tail=text[text.index(marker):]
 readme.write_text('''# Metal Slug GBA Native 0.5.103 — 6kHz·효과음4개

10분 자동 플레이 평균 **21.52→22.34fps(+3.79%)**. 음악1개+효과음4개,192음원을 원본에서6,000Hz로 재변환했다. M1클리어·M2진입,오디오오류0. **30fps와전캠페인완료는아직미달성**이다.

고음선명도·혼잡시낮은우선순위효과음이줄며FIFO/링초기지연분은약12.84ms늘어난다. 그래픽·전투코드는이번에추가변경하지않았다. 기존75%·위4/아래4픽셀크롭과시각최적화유지. 흰프레임14/35859·late15·기존두번째데모오류·M2이후전캠페인·실기2P는남는다.

최신문서: docs/checkpoint-0.5.103.ko.md, docs/remaining-work-0.5.103.ko.md, docs/audio-performance-0.5.103.ko.html, generated/play-validation-0.5.103.json. 원자료와재현은 docs/audio-performance-0.5.103/README.ko.md. HTML에이전/변경후실제출력청취를포함했다.

이하이전구현이력은최신상태와구분한다.

'''+tail)
 agents=R/'AGENTS.md';text=agents.read_text();entry='- Latest checkpoint2026-11-02 v0.5.103: user authorized lower audio quality and performance measurement. Music1 plus max4SFX, all192assets resampled from original source WAV to6000Hz (timer6000.435), loop/reply/fade timing retained within quantization. Nominal30FPS NOTmet: 36000LCD input-only automatic follow21.522->22.337FPS(+3.79%); M1clear/results thenM2partial verified, no fullcampaign/heli defeat/physical2P claim. Same state-aware controller; combat dwell/finalM2position differ. Baseline replay byte-identical commitCSV; literal candidate LCD-key replay paused/restarted and is EXCLUDED from FPS comparison. Three audioIRQ samples16.90/18.03/12.03% ->14.18/9.95/8.79%, not identical mix workloads. No new graphics/gameplay changes, preserve75percent4+4/playerSHA. Output6000Hz reduces highs and SFX cap omits lowerpriority sounds; initial288sample FIFO/ring delay35.16->48ms. Audiofault0,clipped0,192catalog/299replies/11loops and1056000sample PCM differential pass, host regressions pass. Tutorial/game and pause pass; second-demo scene4tick1120x0b1e0000 persists. White14/35859late15 unchanged. ROM1afc760d296f78cde7710f4652e5aae0b310c6aa171a968c60d1eb37e59cdf47,27943516bytes;EWRAM262084(60free),IWRAM26152(216beforestack),observedmain2624/4096IRQ272/2208. Next reduce M1 capture/strip/palette/logic and M2 BG repeated work. Current docs/checkpoint-0.5.103.ko.md and docs/remaining-work-0.5.103.ko.md; evidence docs/audio-performance-0.5.103/. Old102 snapshot/ASan not rerun for103; do not misattribute.\n\n'
 if 'Latest checkpoint2026-11-02 v0.5.103:'not in text:agents.write_text(text.replace('# Source-based native port\n\n','# Source-based native port\n\n'+entry,1))
 release=R.parent/'release';release.mkdir(exist_ok=True);evidence=release/'metal-slug-gba-native-v0.5.103-evidence.zip'
 with zipfile.ZipFile(evidence,'w',zipfile.ZIP_DEFLATED,compresslevel=9)as z:
  for p in sorted(D.rglob('*')):
   if p.is_file():z.write(p,str(p.relative_to(R/'docs')))
  for p in [R/'docs/audio-performance-0.5.103.ko.html',R/'docs/checkpoint-0.5.103.ko.md',R/'docs/remaining-work-0.5.103.ko.md']:z.write(p,p.name)
 print(json.dumps(dict(rom=build['rom'],report=str(R/'docs/audio-performance-0.5.103.ko.html'),evidence=str(evidence),overall_fps=performance['overall']['fps'],change_percent=delta),ensure_ascii=False))
if __name__=='__main__':main()
