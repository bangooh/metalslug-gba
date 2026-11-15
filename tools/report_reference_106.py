#!/usr/bin/env python3
"""Package measured reference-ROM-inspired work without claiming a30FPS win."""
import base64,csv,difflib,hashlib,html,json,re,shutil,struct,wave,zipfile
from pathlib import Path
from analyze_frame_pacing import LCD_MS
R=Path(__file__).resolve().parents[1];B=R/'build/ref106';C=B/'n';D=R/'docs/reference-optimizations-0.5.106'
def read(p):return json.loads(p.read_text())
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def interval(rows):
 a,z=rows[0],rows[-1];seconds=(int(z['lcd'])-int(a['lcd']))*LCD_MS/1000;commits=int(z['commits'])-int(a['commits']);return dict(commits=commits,seconds=seconds,fps=commits/seconds,lcd_start=int(a['lcd']),lcd_end=int(z['lcd']))
def copy_files(source,dest,names):
 dest.mkdir(parents=True,exist_ok=True)
 for n in names:
  if (source/n).exists():shutil.copyfile(source/n,dest/n)
def main():
 D.mkdir(exist_ok=True);build=read(C/'build.json');old=read(R/'generated/play-validation-0.5.105.json');summary=read(C/'summary.json')
 assert build==read(R/'generated/native-build.json');assert sha(C/'rom.gba')==sha(Path(build['rom']))==build['rom_sha256'];assert sha(C/'native.elf')==sha(Path(build['elf']))==build['elf_sha256']
 assert summary['exit_code']==0 and not summary['invalid_benchmark_reasons']
 trace=[json.loads(x) for x in (C/'trace.jsonl').read_text().splitlines()];last=trace[-1]
 assert not(last['app.error'] or last['audio.fault'] or last['audio.clipped'])
 rows=list(csv.DictReader((C/'commits.csv').open()));active=[r for r in rows if r['playing']=='1' and r['presentation']=='0' and r['demo']=='0'];active=active[next(i for i,r in enumerate(active) if int(r['live0'])):]
 perf=dict(overall=summary['active_play'],m1_active=interval([r for r in active if r['scene']=='0']),m2_observed=interval([r for r in active if r['scene']=='1']),white_frames=summary['execution']['all_white_game_frames'],game_lcd_frames=summary['execution']['game_lcd_frames'],late_transfers=last['video.late_transfers'],bg_backend='Exact8bpp baseline plus conditional exact4bpp/BG HUD; full-route counters show only1 combined4bpp/HUD commit, with2 format fallbacks. Normal recorded M1/M2 gameplay uses8bpp and OBJ HUD.')
 transitions=[];previous=None
 for t in trace:
  state=(t['session.state'],t['scene.scene'],t['playing'],t['presentation'])
  if state!=previous:transitions.append({k:t[k] for k in ['lcd_frame','world.ticks','world.results.progress','world.weapons.camera_x','session.state','scene.scene','playing','presentation']});previous=state
 assert any(t['session.state']==2 and t['scene.scene']==0 for t in transitions)
 assert any(t['session.state']==1 and t['scene.scene']==1 and t['world.results.progress']==1 for t in transitions)
 pacing=read(B/'pacing/measurement.json');audio=read(B/'audio-n/verification.json');assert audio['rom_sha256']==build['rom_sha256'] and audio['restart_mismatches']==0
 frontend=read(B/'frontend-n/summary.json');assert all(frontend[n]['exit_code']==0 for n in ['tutorial','pause','link-cancel','ranking']);assert frontend['demo']['last']['app.error']==0x0b1e0000
 sizes={n:int(re.search(r'^'+re.escape(n)+r'\s+(\d+)',build['section_sizes'],re.M).group(1)) for n in ['.bss','.iwram']};memory=dict(old['memory']);memory.update(rom_bytes=build['rom_bytes'],rom_free=33554432-build['rom_bytes'],ewram_used=sizes['.bss'],ewram_free=262144-sizes['.bss'],iwram_before_stack_used=sizes['.iwram'],iwram_before_reserved_stack_free=0x6700-sizes['.iwram'])
 ram=(C/'iwram.bin').read_bytes();sentinel=struct.pack('<I',0xa55a3cc3)
 for name,lo,hi in [('observed_main_stack',0x6700,0x7700),('observed_irq_stack',0x7700,0x7fa0)]:memory[name]=hi-next((i for i in range(lo,hi,4) if ram[i:i+4]!=sentinel),hi)
 assert memory['ewram_free']>=0 and memory['iwram_before_reserved_stack_free']>=0 and memory['observed_main_stack']<4096 and memory['observed_irq_stack']<2208
 approved=sha(R/'generated/player.tiles');assert approved=='ca7febc19eeda7cf62af36f036cdb91f290bf8be8ac279344e03bf269c74be32'
 hardware=[]
 for n in [6428,8810,25000]:
  d=B/f'n-snapshot-{n}';text=(d/'comparison.txt').read_text();assert all(v in text for v in ['BG visible RGB/priority mismatches0','BG committed VRAM/palette mismatches0','FIX mismatches0','FIX committed OAM/VRAM/palette mismatches0'])
  obj=read(d/'object-hardware.json');assert not any(obj[k] for k in ['obj_tile_mismatched_bytes','oam_mismatched_bytes','palette_mismatched_bytes']);hardware.append(dict(requested_lcd=n,comparison=text,objects=obj))
 for n in ['regressions-l.log','formats-final.log','formats-final-asan.log']:assert 'PASS' in (B/n).read_text() and 'ERROR:' not in (B/n).read_text()
 features=dict(palette='Changed32-byte BG/OBJ banks; exact comparison in IWRAM before VBlank, no shadow-state alias.',hud='Exact BG3 HUD implemented with128/256tile spans, dirty cells and source-to-hardware palette bindings; automatically disabled with8bpp to preserve BG cache.',offline=read(R/'generated/native-bg-preflipped.json'),four_bpp='Exact bank packing, largest-set-first retry once per scene, non-mutating fallback. Production assets use the complete imported source catalog, not captured gameplay.',audio='32-sample blocks, Timer3 IRQ about187.5/s; Timer2 preserved for link.16-sample FIFO priming and safe within-burst rebase.',incomplete='The full stage/region8x8 tile+palette precompiler proposed in the reference study is NOT complete. This revision precomputes4x4 orientations and adds a conditional runtime4bpp backend. It does not remove general runtime composition.')
 changes=['Stage and upload only changed BG/OBJ palette banks.','Exact conditional BG HUD with shared scratch, dirty cells and palette-fade bindings.','Offline whole-catalog4x4 orientation preparation; exact runtime4bpp backend and compatibility fallback.','32-sample audio mixing/Timer3 wakeups with validated DMA ring continuity.','Keep ordinary8bpp composition hot path and legacy FIX suffix in IWRAM.']
 report={k:old[k] for k in ['verified_campaign_scope','fixed_frame_bytes','input_method','lcd_clock','object_palette_policy','visual_tradeoffs','audio_settings','pcm_cache']}
 report.update(version='0.5.106',rom_sha256=build['rom_sha256'],elf_sha256=build['elf_sha256'],fingerprint=build['fingerprint'],complete_port=False,m1_clear_verified=True,target_fps=30,target_met=False,memory=memory,performance=perf,frame_pacing=pacing,scene_transitions=transitions,implemented_optimizations=changes,renderer_features=features,hardware_snapshots=hardware,frontend_results=frontend,frontend_verified_paths=['logo/title','tutorial/game','ranking/title','pause/resume','M1 results and M2 entry','no-peer link cancel'],catalog_decode=dict(assets=192,decoded_samples=13821728,pcm_fnv32='a2c8099b',faults=0,last_verified_version='0.5.106',rerun_this_revision=True),audible_output=dict(captured_this_revision=True,**summary['execution'],audio_fault=0,clipped=0,ring_verification=audio,limitation='20s actual emulator output captured; no claim of exhaustive listening or physical hardware timing.'),comparison=dict(baseline_version='0.5.105',baseline_rom_sha256=old['rom_sha256'],baseline_performance=old['performance'],method=pacing['comparison'],baseline_rerun=False),regressions=dict(full_host='regressions-l.log; final runtime rendering algorithms unchanged except automatic fallback policy/audio scheduling',new_formats='formats-final.log;3 exact4bpp fixtures,1 exact8bpp fallback,32 HUD edits/fades; packed hardware maps/palettes decoded against independent oracle',asan_ubsan='formats-final-asan.log; detect_leaks=0 for ptrace environment'),priority='Finish whole-stage/region8x8 tile/palette precompilation, then re-evaluate sustained4bpp/BG HUD. Current measured FPS gain~0.2%, long>=100ms frames33->35;30FPS and smooth pacing are NOT met.',evidence='docs/reference-optimizations-0.5.106/')
 report['audio_settings']=dict(report['audio_settings'],sample_rate=6000,max_sfx_voices=4,mix_block_samples=32,mixer_irq='Timer3',nominal_irq_hz=6000/32,previous_irq_hz=6000/16,initial_silence_samples=272,initial_silence_ms=272/(16777216/2796)*1000,link_timer='Timer2 unchanged')
 write(R/'generated/play-validation-0.5.106.json',report);write(D/'measurement.json',report)
 copy_files(C,D/'selected',['build.json','summary.json','execution.json','probe.txt','symbols.txt','inputs.csv','commits.csv','trace.jsonl','entities.csv','iwram.bin'])
 copy_files(R/'build/pacing105/f',D/'baseline',['build.json','summary.json','commits.csv'])
 copy_files(B/'pacing',D/'pacing',['measurement.json','baseline.csv','candidate.csv'])
 copy_files(B/'audio-n',D/'audio',['verification.json','observer.log']);shutil.copyfile(B/'k/inputs.csv',D/'audio/replay-inputs.csv')
 for n in ['regressions-l.log','regressions-k.log','formats-final.log','formats-final-asan.log']:shutil.copyfile(B/n,D/n)
 copy_files(B/'frontend-n',D/'frontend',['summary.json'])
 for name in frontend:copy_files(B/'frontend-n'/name,D/'frontend'/name,['trace.jsonl','execution.json','inputs.csv','commits.csv','emulator.log'])
 for n in [6428,8810,25000]:copy_files(B/f'n-snapshot-{n}',D/f'snapshot-{n}',['comparison.txt','capture.log','object-hardware.json','fixture.bin','fixed.bin','hud-active.bin','hud-count.bin','vram.bin','pal.bin','oam.bin','screen.png','ewram.bin'])
 copy_files(B/'l',D/'rejected-all-bg-hud',['build.json','summary.json']);copy_files(B/'l-snapshot-400',D/'rejected-all-bg-hud/snapshot-400',['comparison.txt','fixture.bin','fixed.bin','hud-active.bin','vram.bin','pal.bin','oam.bin','ewram.bin'])
 candidates=[]
 for name in ['a','b','c2','e','f','g','h','i','j','k','l','m','n']:
  if (B/name/'summary.json').exists():candidates.append(dict(candidate=name,**read(B/name/'summary.json')))
 write(D/'candidates.json',candidates)
 delta=(perf['overall']['fps']/old['performance']['overall']['fps']-1)*100
 rows_html=''.join(f'<tr><td>{label}</td><td>{a}</td><td>{z}</td></tr>' for label,a,z in [('평균 FPS',f"{old['performance']['overall']['fps']:.3f}",f"{perf['overall']['fps']:.3f}"),('M1 구간 FPS',f"{old['performance']['m1_active']['fps']:.3f}",f"{perf['m1_active']['fps']:.3f}"),('M2 관측 구간 FPS',f"{old['performance']['m2_observed']['fps']:.3f}",f"{perf['m2_observed']['fps']:.3f}"),('100ms 이상 지연',33,35),('8 LCD 이상 지연',9,8),('99백분위', '83.71ms','83.71ms'),('최장 지연','267.88ms','251.14ms'),('흰 LCD 프레임',10,perf['white_frames']),('늦은 전송',11,perf['late_transfers'])])
 pngs=''.join('<figure><img src="data:image/png;base64,'+base64.b64encode((D/f'snapshot-{n}/screen.png').read_bytes()).decode()+'"><figcaption>실제 입력 재생 · LCD '+str(n)+'</figcaption></figure>' for n in [6428,8810,25000])
 wav=D/'audio/output-20s.wav'
 with wave.open(str(wav),'wb') as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(summary['execution']['audio_rate']);w.writeframes((C/'audio.s16le').read_bytes())
 audio64=base64.b64encode(wav.read_bytes()).decode()
 report_html=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>GBA 참조 구조 적용 · v0.5.106</title><style>body{{font:16px/1.75 system-ui,sans-serif;background:#f0f3f5;color:#17212b;margin:0}}main{{max-width:1060px;margin:36px auto;padding:40px;background:white;border-radius:16px}}h1{{font-size:32px;line-height:1.35}}h2{{margin-top:36px;font-size:22px}}.lead{{font-size:20px}}.note{{background:#fff1d7;padding:18px;border-left:4px solid #c78110}}table{{width:100%;border-collapse:collapse;font-size:15px}}th,td{{padding:12px;border-bottom:1px solid #dce2e7;text-align:left}}th{{background:#eaf0f4}}code{{overflow-wrap:anywhere;font-size:13px}}.shots{{display:grid;grid-template-columns:repeat(3,1fr);gap:14px}}figure{{margin:0}}img{{width:100%;image-rendering:pixelated}}figcaption{{font-size:13px;color:#586675}}audio{{width:100%}}@media(max-width:700px){{main{{margin:0;padding:20px}}.shots{{grid-template-columns:1fr}}table{{font-size:13px}}}}</style><main>
<p>METAL SLUG NATIVE · 구현과 검증 · 2026-11-02</p><h1>참고 ROM 구조를 적용했지만,<br>전체 성능 효과는 아직 작습니다.</h1>
<p class="lead">36,000 LCD 프레임 입력 전용 검증. 평균 <b>{old['performance']['overall']['fps']:.2f} → {perf['overall']['fps']:.2f}fps</b> ({delta:+.2f}%). M1 결과 → M2 일부 구간까지 진행했고 게임·오디오 오류는 0건입니다. <b>30fps 목표는 미달</b>입니다.</p>
<p class="note">“전부 구현 완료·상시 활성화” 상태가 아닙니다. 팔레트와 오디오 변경은 채택했습니다. BG HUD·정확한4bpp 경로는 구현·검증했지만, 연속 플레이에서는 대부분8bpp/OBJ HUD로 복귀합니다. 스테이지·구역 전체8×8 타일·팔레트 사전 컴파일러는 남아 있습니다.</p>
<h2>이번 적용 범위</h2><table><tr><th>항목</th><th>반영한 내용</th><th>최종 상태</th></tr>
<tr><td>팔레트 변경분 전송</td><td>실제 팔레트 RAM과 정확히 비교하고 변경32바이트 뱅크만 DMA. 비교는 VBlank 전에 IWRAM에서 수행.</td><td>채택</td></tr>
<tr><td>BG HUD</td><td>기존 FIX 작업 공간을 재사용.128/256타일 배치, 변경 셀 갱신, 페이드 색 바인딩. BG3 사용.</td><td>4bpp와 함께 유지 가능한 경우만 활성.8bpp와 강제 병용하면 캐시가 줄어 성능 회귀.</td></tr>
<tr><td>오프라인 BG·4bpp</td><td>전체53이미지·659패치에서 쓰는189,648블록의3반전 방향 사전 생성.9,103,104픽셀 복원 검증. 정확한4bpp·8bpp 복귀 추가.</td><td>방향 전처리 완료. 구역 전체8×8·팔레트 사전 컴파일은 미완. 현재 주행4bpp/HUD 커밋은 각각1회뿐이며 지속 전투 가속으로 해석하면 안 됨.</td></tr>
<tr><td>오디오 블록·IRQ</td><td>16→32샘플.6kHz·음악1+효과음4 유지. Timer3 약375→187.5회/초. Timer2 링크 유지.</td><td>채택.64샘플 초기안은 전송 타이밍·링 경계 문제가 있어 제외.</td></tr></table>
<h2>성능과 끊김</h2><table><tr><th>측정</th><th>0.5.105</th><th>0.5.106</th></tr>{rows_html}</table>
<p>같은 상태 기반 자동 조작기로 각각 실행했습니다. 관측 전투 시간과 종료 위치가 달라 동일 월드 상태의 A/B 실험은 아닙니다. 약0.2% 차이는 실질적인 속도 개선을 입증하기에 작습니다. M1 tick10–5200의 연속 갱신4,728쌍에서는 평균 간격이 두 버전 모두43.13ms이고,100ms 이상 지연은 오히려2회 늘었습니다. 흰 프레임은 각각10회로 남았습니다.</p>
<p>BG HUD를8bpp와 함께 유지한 후보L은 평균23.337fps·흰12프레임이었습니다. 최종N은 실패 시 기존 HUD로 즉시 돌아갑니다. 스냅샷3곳에서4bpp 변환이 가능하다는 결과가 연속 스크롤·페이드 전체에서 성립하지 않는다는 점을 확인했습니다. 장기 프레임 개선의 다음 대상은 구역별 사전 타일·팔레트 생성과 런타임 합성 비용입니다.</p>
<h2>그래픽 검증</h2><div class="shots">{pngs}</div>
<p>최종 ROM3개 지점의 실제 BG/HUD RGB·우선순위와 VRAM·팔레트·OAM 비교 불일치0.4bpp용 별도 호스트 검사에서는 실제105 원본 상태4개 중3개가 정확히 변환되고,1개는8bpp로 복귀했습니다. 하드웨어용4bpp 맵·팔레트를 다시 읽어 독립 원본 렌더러와 비교했습니다. HUD 변경·페이드32프레임도 RGB·투명도 일치. 후보L의 실제 BG HUD 화면도 비교 불일치0이며, 최종N의 상시 사용 증거와 구분했습니다.</p>
<p>기존75% 크기·상하4+4픽셀 크롭 유지. 승인 player.tiles SHA256 <code>{approved}</code>. 새 색상 양자화·전투 판정·입력·난수 로직 변경은 없습니다.</p>
<h2>오디오와 화면 흐름</h2><p>최종N의 별도36,000 LCD 입력 재생에서 DMA 링 재시작 <b>{audio['dma_ring_wraps']:,}회, 실제 다음 주소와 불일치0</b>. Timer3 확인 {audio['timer3_acknowledgements']:,}회. FIFO16샘플 준비로 초기 지연은288→272샘플, 약48→45.33ms입니다. 링 재시작은 FIFO 내부 워드 위상을 피하는 안전 구간에서 처리합니다.</p>
<p>192음원·13,821,728샘플 카탈로그 검사,1,056,000샘플 PCM 차등 검사 통과. 아래는 최종 ROM의 실제20초 출력이며6kHz 모노를 에뮬레이터32768Hz 스테레오 컨테이너로 담았습니다. 전 구간 청취·실기 음향 검증을 뜻하지 않습니다.</p><audio controls src="data:audio/wav;base64,{audio64}"></audio>
<p>튜토리얼→게임, 랭킹→타이틀, 일시정지·복귀, 상대 없는 링크 취소 통과. 두 번째 자동 데모는 기존 scene4/tick112 오류 <code>0x0b1e0000</code>가 재현되었습니다. 물리2P·전 캠페인·M2 클리어·헬기 격파는 별도 미검증입니다.</p>
<h2>용량·재현</h2><p>ROM {build['rom_bytes']:,}바이트,32MiB까지 {memory['rom_free']:,}바이트. EWRAM 여유 {memory['ewram_free']}바이트, 예약 스택 앞 IWRAM 여유 {memory['iwram_before_reserved_stack_free']}바이트. 관측 메인 스택 {memory['observed_main_stack']}/4096, IRQ {memory['observed_irq_stack']}/2208바이트. 이 주행에서의 관측이며 모든 보스·2P의 상한 증명은 아닙니다.</p>
<p>ROM SHA256 <code>{build['rom_sha256']}</code><br>ELF SHA256 <code>{build['elf_sha256']}</code></p>
<p>소스의 <code>tools/report_reference_106.py</code>, <code>tools/performance_regressions.py</code>, <code>tools/verify_native_audio_ring.py</code>와 증거 묶음의 입력·커밋·텔레메트리·메모리 비교 자료로 재현할 수 있습니다. 자동 입력으로 얻은 캡처는 회귀검사에만 사용하며 제품 BG 에셋의 입력으로 사용하지 않습니다.</p></main></html>'''
 (R/'docs/reference-optimizations-0.5.106.ko.html').write_text(report_html)
 backlog=f'''# 0.5.106 남은 작업

목표 평균30fps와 안정적인 프레임 간격은 미달이다.105→106 평균{old['performance']['overall']['fps']:.3f}→{perf['overall']['fps']:.3f}fps,100ms 이상33→35회, 최장268→251ms, 흰10회 유지. 큰 성능 개선으로 해석하지 않는다.

| 우선순위 | 남은 작업 | 완료 기준 |
|---|---|---|
|1|전체 스테이지·구역별8×8 타일·팔레트 사전 컴파일러|이번 전체 블록 반전 전처리와 구분. 원본53이미지·659패치에서 계획 생성, 스크롤·파괴·페이드·전환 검증. 캡처 경로를 생산 에셋으로 쓰지 않음. ROM 남은{memory['rom_free']:,}바이트 내 비용 측정|
|2|지속적인4bpp/BG HUD 활성 영역 확대|현재 경로는 구현됐지만 실제 주행 커밋 각각1회뿐.8bpp와 BG HUD 강제 병용은 캐시 손실로 회귀. 구역 계획과 정확한 색/VRAM 예산을 함께 해결|
|3|M1/M2 런타임 BG·그림 수집·strip·팔레트·로직|30fps 및100ms 지연 감소. 상시2/3 LCD 간격 변동과 cold-cache 지연을 함께 측정|
|4|흰 화면과 늦은 전송|흰10/35,815, late11 제거|
|5|두 번째 자동 데모|scene4/tick112/error0x0b1e0000 해결|
|6|전 캠페인·보스·엔딩|이번 M1 clear/results→M2 partial까지. M2 이후·헬기 격파·Slug·각 보스·결과/엔딩 입력 전용 검증 확대|
|7|실기2P·메모리·음향|Timer2 링크 유지, Timer3 오디오를 실제 두 기기에서 검증. EWRAM 여유284, IWRAM 예약 스택 앞40. 관측 main3044/4096 IRQ264/2208|
|8|원본 의미·추가 기능|48슬롯 팔레트·과밀strip·승인 combat overlay·59D1/120D5/318A3A4·YM채널/음높이·영구랭킹·언어/설정|

팔레트 변경 뱅크 전송,32샘플/Timer3 오디오, 전체4×4 블록 방향 전처리를 채택했다. BG HUD·정확한4bpp는 구현·검증했고 부적합 시 기존8bpp/OBJ HUD로 복귀한다. 전체 구역 사전 컴파일이 끝났다고 표시하지 않는다.75%·상하4+4·승인 플레이어 해시는 유지했다.

ROM `{build['rom_sha256']}`. 최종 동결 build/ref106/n. 보고서 docs/reference-optimizations-0.5.106.ko.html, 검사 generated/play-validation-0.5.106.json, 증거 docs/reference-optimizations-0.5.106/. 전체 호스트 회귀·4bpp/HUD ASan/UBSan·실제3GPU 스냅샷 통과. 오디오 링 {audio['dma_ring_wraps']}회 불일치0. 튜토리얼·랭킹·pause·링크취소 통과. 기존 데모 오류·전캠페인·실기2P는 남는다.
'''
 (R/'docs/remaining-work-0.5.106.ko.md').write_text(backlog);(R/'docs/checkpoint-0.5.106.ko.md').write_text(backlog)
 (D/'README.ko.md').write_text('''# 재현 및 해석

최종 동결 ROM은 n이며 전체 소스의 generated/native-build.json과 SHA가 일치한다. baseline은105의 기존 동결 측정값이다. 같은 상태 기반 조작기의 별도 실행이며 동일 월드 상태 A/B가 아니다.

실행: tools/build_native.py --cc ARM_GCC; tools/prepare_followthrough_probe.py --cc ARM_GCC --output PROBE; tools/validate_heli_performance.py --output RUN --probe PROBE/probe.txt --mgba-prefix MGBA --frames 36000 --mode 8.

회귀: python tools/performance_regressions.py.4bpp 호스트 검사는 tests/fixtures/reference-105-*.bin의 읽기 전용 원본 상태를 사용한다. 제품 에셋은 전체 원본 목록에서 생성한다. ASan/UBSan은 reference_formats_106.c에 -fsanitize=address,undefined 및 --wrap=native_palette_dirty 사용, LSAN만 ptrace 환경 때문에 비활성화했다.

selected/ 입력은 최종 주행. audio/replay-inputs.csv는 동일 최종ROM에 별도로 재생한 스트레스 입력이며 selected와 다르다. verification.json의 해시로 구분한다. 관찰 후크는 실제DMA 다음 주소를 읽고 원래 메모리 쓰기를 그대로 전달한다.

rejected-all-bg-hud는 후보L 증거다. 최종N의 상시 BG HUD 증거가 아니다. 해당 실제HUD 비교는0차이지만 성능/캐시 비용으로 강제 병용을 채택하지 않았다. 후속 전체 스테이지8×8·팔레트 사전 컴파일은 미완이다.
''')
 entry=f'- Latest checkpoint2026-11-02 v0.5.106: reference-ROM-inspired candidates implemented/tested; NOT full stage/region precompiler completion. Selected n ROM{build["rom_sha256"]}.105->10636000LCD23.316->23.361FPS(~+0.19%, negligible); M1>=100ms33->35, >=8LCD9->8,p9983.71ms unchanged,max268->251ms; white10,late11. Changed palette banks staged beforeVBlank; whole-catalog4x4 preflips; exact4bpp/BG HUD backend with fallback. Actual full route only1 BG4/HUD commit: normal gameplay remains8bpp/OBJ HUD. All-BG-HUD candidate regressed. Audio6000Hz4SFX retained;32sample Timer3 IRQ~187.5/s,Timer2link kept,16sample priming272silence samples; {audio["dma_ring_wraps"]} ring restarts0mismatch. M1results/clear->M2partial,error/audio/clipped0;3 final GPU snapshots exact, host and4bpp/HUD ASanUBSan pass. Tutorial/ranking/pause/linkcancel pass; seconddemo scene4tick1120x0b1e0000 persists. EWRAM284free,IWRAM40beforestack,observed main3044 IRQ264. Preserve75percent4+4/playerSHA. Fullcampaign/M2clear/heli defeat/physical2P NOTverified. Next finish whole-stage/region8x8+palette precompile within ROM budget, then sustained4bpp/HUD. Current docs/checkpoint-0.5.106.ko.md, docs/remaining-work-0.5.106.ko.md, docs/reference-optimizations-0.5.106/. Tests/ROM authorized; continue without approval.\n\n'
 agents=R/'AGENTS.md';s=agents.read_text();s=s.replace('# AGENTS.md\n','# AGENTS.md\n\n'+entry,1) if s.startswith('# AGENTS.md') else entry+s;agents.write_text(s)
 readme=R/'README.ko.md';readme.write_text('# v0.5.106 · 참고 ROM 구조 적용 실험\n\n평균23.316→23.361fps로 큰 개선은 없다.30fps 미달,100ms 이상33→35회. 팔레트·32샘플 오디오·전체 블록 반전 전처리를 채택했고,4bpp/BG HUD는 부적합 시 복귀하는 조건부 경로다. 전체 스테이지·구역8×8/팔레트 사전 컴파일은 남았다.\n\n최신: docs/checkpoint-0.5.106.ko.md, docs/remaining-work-0.5.106.ko.md, docs/reference-optimizations-0.5.106.ko.html. M1결과→M2일부, 오류0; 전캠페인/실기2P 미검증. 아래는 이전 이력이다.\n\n'+readme.read_text())
 # Text-only patch from the frozen105 source; new generated binary asset is
 # documented by its complete-inventory manifest instead of a binary diff.
 patches=[]
 for folder in ['src','tools','tests']:
  oldroot=B/'baseline'/folder
  for p in sorted((R/folder).rglob('*')):
   if p.suffix not in ['.c','.h','.S','.py'] or '__pycache__' in p.parts:continue
   before=oldroot/p.relative_to(R/folder);a=before.read_text().splitlines(True) if before.exists() else [];z=p.read_text().splitlines(True)
   if a!=z:patches.extend(difflib.unified_diff(a,z,fromfile='105/'+str(p.relative_to(R)),tofile='106/'+str(p.relative_to(R))))
 (D/'changes.patch').write_text(''.join(patches))
 with zipfile.ZipFile(R/'dist/metal-slug-gba-native-v0.5.106-evidence.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for p in sorted(D.rglob('*')):
   if p.is_file():z.write(p,p.relative_to(D))
 print(json.dumps(dict(rom=build['rom'],fps=perf['overall']['fps'],delta_percent=delta,memory=memory,report=str(R/'docs/reference-optimizations-0.5.106.ko.html')),ensure_ascii=False))
if __name__=='__main__':main()
