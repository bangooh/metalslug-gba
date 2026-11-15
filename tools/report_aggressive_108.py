#!/usr/bin/env python3
"""Publish matching v108 evidence; preserve rejected experiments explicitly."""
import base64,csv,difflib,hashlib,json,re,shutil,struct,wave,zipfile
from pathlib import Path
from report_reference_106 import interval,copy_files
R=Path(__file__).resolve().parents[1];B=R/'build/aggressive108';C=B/'g2';D=R/'docs/aggressive-performance-0.5.108'
def read(p):return json.loads(p.read_text())
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 D.mkdir(exist_ok=True);old=read(R/'generated/play-validation-0.5.107.json');build=read(C/'build.json');summary=read(C/'summary.json')
 assert build==read(R/'generated/native-build.json')
 assert sha(C/'rom.gba')==sha(Path(build['rom']))==build['rom_sha256']
 assert sha(C/'native.elf')==sha(Path(build['elf']))==build['elf_sha256']
 assert summary['exit_code']==0 and not summary['invalid_benchmark_reasons']
 trace=[json.loads(l)for l in (C/'trace.jsonl').read_text().splitlines()];last=trace[-1]
 assert not any(last[k]for k in ['app.error','audio.fault','audio.clipped'])
 def active(path):
  rows=list(csv.DictReader((path/'commits.csv').open()));rows=[r for r in rows if r['playing']=='1' and r['presentation']=='0' and r['demo']=='0'];return rows[next(i for i,r in enumerate(rows)if int(r['live0'])):]
 rows=active(C);baseline_rows=active(R/'build/region107/c2')
 perf=dict(overall=summary['active_play'],m1_active=interval([r for r in rows if r['scene']=='0']),m2_observed=interval([r for r in rows if r['scene']=='1']),white_frames=summary['execution']['all_white_game_frames'],game_lcd_frames=summary['execution']['game_lcd_frames'],late_transfers=last['video.late_transfers'],bg_backend='Conditional exact4bpp/BG HUD,1commit each. Normal8bpp/OBJ HUD. Latched occlusion retired when conservative cell bound leaves64tile headroom; cache retained.')
 matched={name:interval([r for r in rr if 8000<=int(r['camera_x'])<=24000])for name,rr in [('baseline',baseline_rows),('candidate',rows)]}
 transitions=[];previous=None;culling=[];previous_cull=None
 for t in trace:
  state=(t['session.state'],t['scene.scene'],t['playing'],t['presentation'])
  if state!=previous:transitions.append({k:t[k]for k in ['lcd_frame','world.ticks','world.results.progress','world.weapons.camera_x','session.state','scene.scene','playing','presentation']});previous=state
  if t['video.backgrounds.culling']!=previous_cull:culling.append({k:t[k]for k in ['lcd_frame','world.ticks','world.weapons.camera_x','video.backgrounds.culling']});previous_cull=t['video.backgrounds.culling']
 assert any(t['session.state']==2 and t['scene.scene']==0 for t in transitions)
 assert any(t['session.state']==1 and t['scene.scene']==1 and t['world.results.progress']==1 for t in transitions)
 pacing=read(B/'pacing-g2/measurement.json');audio=read(B/'audio/verification.json');frontend=read(B/'frontend/summary.json')
 assert audio['rom_sha256']==build['rom_sha256'] and audio['restart_mismatches']==0
 assert all(frontend[k]['exit_code']==0 for k in ['tutorial','pause','link-cancel','ranking'])
 assert frontend['demo']['last']['app.error']==0x0b1e0000
 sizes={n:int(re.search(r'^'+re.escape(n)+r'\s+(\d+)',build['section_sizes'],re.M).group(1))for n in ['.bss','.iwram']}
 memory=dict(old['memory']);memory.update(rom_bytes=build['rom_bytes'],rom_free=33554432-build['rom_bytes'],ewram_used=sizes['.bss'],ewram_free=262144-sizes['.bss'],iwram_before_stack_used=sizes['.iwram'],iwram_before_reserved_stack_free=0x6700-sizes['.iwram'])
 ram=(C/'iwram.bin').read_bytes();sentinel=struct.pack('<I',0xa55a3cc3)
 for name,lo,hi in [('observed_main_stack',0x6700,0x7700),('observed_irq_stack',0x7700,0x7fa0)]:memory[name]=hi-next((i for i in range(lo,hi,4)if ram[i:i+4]!=sentinel),hi)
 assert memory['ewram_free']>=0 and memory['iwram_before_reserved_stack_free']>=0 and memory['observed_main_stack']<4096 and memory['observed_irq_stack']<2208
 hardware=[]
 for n in [6428,8810,25000]:
  d=B/f'snapshot-{n}';text=(d/'comparison.txt').read_text();assert all(k in text for k in ['BG visible RGB/priority mismatches0','BG committed VRAM/palette mismatches0','FIX mismatches0','FIX committed OAM/VRAM/palette mismatches0'])
  obj=read(d/'object-hardware.json');assert not any(obj[k]for k in ['obj_tile_mismatched_bytes','oam_mismatched_bytes','palette_mismatched_bytes'])
  hardware.append(dict(requested_lcd=n,comparison=text,objects=obj))
 for n in ['host-final.log','unprune-runner.log','unprune-asan.log']:
  s=(B/n).read_text();assert 'PASS' in s and 'ERROR:' not in s and 'runtime error:' not in s
 assert sha(R/'generated/player.tiles')=='ca7febc19eeda7cf62af36f036cdb91f290bf8be8ac279344e03bf269c74be32'
 candidates={n:read(B/n/'summary.json')for n in ['a','b','c','d','e','f','g2']}
 changes=['Retire latched BG occlusion every128ticks only when all layers remain enabled and nonzero/hidden visible cell count+blank+64headroom fits capacity. Keep cache and palette; recompose previously hidden cells once. Ordinary capacity fallback remains.', 'SFX PCM spans with no event/loop/end boundary use direct pointers; exact sample output and reply timing retained,6000Hz and4SFX unchanged.', 'Exact RGB palette search moved to IWRAM/noinline O2; FIX suffix O2 and prefix ROM free space. No new image/audio sacrifice.']
 features=dict(old['renderer_features']);features.update(occlusion_retirement=changes[0],tile_hash='v107 exact rotate/add hash unchanged; not a new108 optimization.')
 report=dict(old);report.update(version='0.5.108',rom_sha256=build['rom_sha256'],elf_sha256=build['elf_sha256'],fingerprint=build['fingerprint'],memory=memory,performance=perf,frame_pacing=pacing,scene_transitions=transitions,implemented_optimizations=changes,renderer_features=features,hardware_snapshots=hardware,frontend_results=frontend,catalog_decode=dict(assets=192,decoded_samples=13821728,pcm_fnv32='a2c8099b',faults=0,last_verified_version='0.5.108',rerun_this_revision=True),audible_output=dict(captured_this_revision=True,**summary['execution'],audio_fault=0,clipped=0,ring_verification=audio,limitation='20s emulator output captured; no exhaustive listening or physical GBA claim.'),comparison=dict(baseline_version='0.5.107',baseline_rom_sha256=old['rom_sha256'],baseline_performance=old['performance'],baseline_rerun=False,method=pacing['comparison'],same_camera_8000_24000=matched),regressions=dict(host='host-final.log plus unprune-runner.log',asan_ubsan='New cache-preserving unprune fixture and capacity boundaries; detect_leaks=0 for ptrace. unprune-asan.log.'),rejected_candidates={n:candidates[n]for n in ['a','c','d','e','f']},experiment_candidates=candidates,occlusion_transitions=culling,target_fps=30,target_met=False,priority='M1 logic/object/HUD bottlenecks and stable30FPS; retain measured M2 win. Reduce white-transfer frames; source-region precompile/ROM representation replacement and full campaign/2P remain pending.',evidence='docs/aggressive-performance-0.5.108/')
 report['rom_budget_analysis']=dict(capacity_bytes=33554432,used_bytes=build['rom_bytes'],free_bytes=33554432-build['rom_bytes'],pcm_cached_assets=168,duplicate_ima_bytes=817720,possible_free_after_repack=33554432-build['rom_bytes']+817720,implemented=False,next='Repack redundant IMA bytes and adjust catalog/runtime validation without changing decoded sound; spend reclaimed space on measured M1 BG/palette plans. Not a measured FPS claim.')
 write(R/'generated/play-validation-0.5.108.json',report)
 copy_files(C,D/'play',['build.json','summary.json','execution.json','trace.jsonl','commits.csv','inputs.csv','entities.csv','probe.txt','symbols.txt','emulator.log'])
 copy_files(B/'pacing-g2',D/'pacing',['measurement.json','baseline.csv','candidate.csv'])
 copy_files(B/'audio',D/'audio',['verification.json','observer.log'])
 copy_files(B/'frontend',D/'frontend',['summary.json'])
 for n in ['tutorial','pause','link-cancel','ranking','demo']:copy_files(B/'frontend'/n,D/'frontend'/n,['trace.jsonl','commits.csv','inputs.csv','execution.json','emulator.log'])
 for n in [6428,8810,25000]:copy_files(B/f'snapshot-{n}',D/f'snapshot-{n}',['comparison.txt','object-hardware.json','fixture.bin','fixed.bin','hud-active.bin','hud-count.bin','vram.bin','pal.bin','oam.bin','ewram.bin','screen.png','capture.log'])
 for n in ['a','b','c','d','e','f']:copy_files(B/n,D/f'candidate-{n}',['build.json','summary.json','trace.jsonl','commits.csv','inputs.csv'])
 for folder in ['candidate-a-src','candidate-c-src','candidate-e-src','candidate-f-src','profile']:copy_files(B/folder,D/folder,[p.name for p in (B/folder).iterdir()if p.is_file()])
 copy_files(B,D,['host-final.log','unprune-runner.log','unprune-asan.log','diagnosis.c','layers-6428.png','layers-25000.png'])
 shutil.copyfile(R/'generated/play-validation-0.5.108.json',D/'validation.json')
 with wave.open(str(D/'audio/output.wav'),'wb')as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(summary['execution']['audio_rate']);w.writeframes((C/'audio.s16le').read_bytes())
 patch=''
 for n in ['native_audio.c','native_video.c','background_video.c','fixed_object.c','object_video.c']:
  patch+=''.join(difflib.unified_diff((B/'baseline/src'/n).read_text().splitlines(True),(R/'src'/n).read_text().splitlines(True),fromfile='v107/src/'+n,tofile='v108/src/'+n))
 (D/'runtime-changes.patch').write_text(patch)
 delta=(perf['overall']['fps']/old['performance']['overall']['fps']-1)*100
 rows_html=''.join(f'<tr><td>{label}</td><td>{a:.3f}</td><td>{b:.3f}</td><td>{(b/a-1)*100:+.2f}%</td></tr>'for label,a,b in [('전체 주행',old['performance']['overall']['fps'],perf['overall']['fps']),('M1',old['performance']['m1_active']['fps'],perf['m1_active']['fps']),('M2 관측',old['performance']['m2_observed']['fps'],perf['m2_observed']['fps']),('M2 camera8000..24000',matched['baseline']['fps'],matched['candidate']['fps'])])
 imgs=''.join('<figure><img width="480" src="data:image/png;base64,'+base64.b64encode((D/f'snapshot-{n}/screen.png').read_bytes()).decode()+f'"><figcaption>실제 LCD 요청 {n} · BG/FIX/OBJ 출력 검증</figcaption></figure>'for n in [6428,8810,25000])
 page=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Metal Slug GBA0.5.108 성능 검증</title><style>body{{font:16px/1.65 system-ui;max-width:980px;margin:36px auto;padding:0 24px;background:#101923;color:#e6edf3}}h1,h2{{color:#8edddd}}table{{border-collapse:collapse;width:100%}}td,th{{padding:9px;border-bottom:1px solid #425163;text-align:left}}img{{max-width:100%;image-rendering:pixelated}}code{{overflow-wrap:anywhere}}a{{color:#8edddd}}</style><h1>0.5.108 · 배경 가림 계산 해제</h1><p>평균 {old['performance']['overall']['fps']:.3f} → <b>{perf['overall']['fps']:.3f}fps ({delta:+.2f}%)</b>. 목표30fps는 미달입니다. 큰 개선은 M2에서 나왔으며 M1의 로직·객체·HUD 병목은 남았습니다. 추가 그래픽 삭제나 오디오 품질 저하는 없습니다.</p><table><tr><th>측정 범위</th><th>107</th><th>108</th><th>변화</th></tr>{rows_html}</table><p>36,000LCD, 약10분, 같은 상태 대응 자동 입력 정책의 별도 주행입니다. 세계 상태가 완전히 같은 비교는 아닙니다. M2 카메라8000..24000에서는 두 주행 모두4,963커밋을 포함합니다. 전체 캠페인 평균은 아닙니다.</p><h2>왜 빨라졌나</h2><p>타일 부족으로 켜진 정밀 가림 계산이 이후에도 계속 유지됐습니다.128게임틱마다 현재 화면의 비어 있지 않은 타일 셀을 세고, 숨겨진 셀도 모두 별개 타일로 보수적으로 계산합니다. 모든 레이어가 활성이고64타일 여유가 있으면 가림 계산을 해제합니다. 기존 캐시·팔레트를 유지하고 가려져 생략했던 셀만 복원합니다. 새 타일 부족은 기존 fallback으로 처리합니다.</p><p>효과음은 이벤트·반복·끝 경계를 넘지 않는 PCM 구간만 직접 읽습니다. 오디오6000Hz/4SFX와 출력 샘플·응답 타이밍은 유지합니다. 정확한 팔레트 검색은 빠른 메모리로 이동했고 HUD 코드 크기를 줄였습니다. 충돌 순회 IWRAM 이동은 유의미한 이득이 없어 최종본에서 제외했습니다.</p><h2>채택하지 않은 실험</h2><p>HUD 선행 압축 A22.531fps로 회귀. 장면/이미지 전환 캐시 초기화 C23.430fps·흰16회로 제외. D/E는 commit 후 ready가0이어서 해제 조건이 실행되지 않는 구현 오류를 확인했습니다. F는 해제 시 캐시 전체를 비워25.212fps이나 흰13회. 최종G2는 캐시를 유지해25.245fps·흰11회입니다. G는 IWRAM 링크 한도를 넘어 ROM을 만들지 않았으며 측정에 포함하지 않았습니다.</p><h2>끊김과 남은 문제</h2><p>M1 ticks10..5200 기준100ms 이상 지연35→{pacing['candidate']['at_least_100ms']}회,8LCD 이상8→{pacing['candidate']['at_least_8_lcd_frames']}회, p99 {pacing['candidate']['p99_ms']:.2f}ms, 최장251.14→{pacing['candidate']['max_ms']:.2f}ms. 모든 지표가 개선된 것은 아닙니다. 흰 화면10→{perf['white_frames']}/{perf['game_lcd_frames']}LCD, 늦은 전송11→{perf['late_transfers']}회로 각각1회 늘었습니다.</p><h2>검증</h2><p>M1 결과 화면→M2 일부까지 입력만으로 진행. 게임 오류·오디오 오류·클리핑0. 세 실제 GPU 캡처의 BG/FIX RGB·우선순위·VRAM·OBJ/OAM/팔레트 차이0. 호스트 회귀, 새 가림 해제 경계/정확 출력, ASan/UBSan 통과. 오디오 링 불일치0. 튜토리얼·pause복귀·랭킹·링크취소 통과. 두 번째 데모 scene4tick112/error0x0b1e0000은 기존처럼 실패합니다. 전 캠페인·M2클리어·헬기격파·실기2P 미검증.</p>{imgs}<h2>Advance와의 차이</h2><p>Metal Slug Advance는 GBA용 별도 작품입니다(<a href="https://www.nintendo.com/jp/games/feature/nintendo-classics/a-8186_j/index.html">Nintendo 공식 소개</a>). GBA 전용 설계에서는 타일·색·동시 등장량을 함께 조정할 수 있습니다. 현재 포트는 원작의 동작과 배경을 유지하며 실행 중 타일 조합·팔레트 검색을 수행합니다. Advance의 실제 내부 구현이나 고정60fps를 이번 작업에서 확인한 것은 아닙니다. 사전 타일/팔레트 배치 확대는 현재 포트 코드와 프로파일에 근거한 개선 방향입니다.</p><h2>32MiB ROM 공간 활용</h2><p>현재31.001MiB 사용, 여유1,047,788바이트(약1MiB). PCM으로 미리 풀어 놓은168효과음의 원래IMA 데이터817,720바이트도 링크돼 있습니다. 카탈로그/오프셋/검증 경로를 정리해 중복을 제거하면 합계약1.78MiB 확보 가능성이 있습니다. 아직 미적용이며 FPS 이득 미측정. M1의 자주 쓰는 배경 조합·팔레트 계획에 우선 배정하는 것이 다음 후보입니다. 단순 ROM 패딩은 성능을 높이지 않습니다.</p><h2>메모리와 재현</h2><p>ROM {build['rom_bytes']:,}바이트, EWRAM 여유{memory['ewram_free']}, 예약 스택 앞 IWRAM 여유{memory['iwram_before_reserved_stack_free']}바이트. 관측 main {memory['observed_main_stack']}/4096, IRQ {memory['observed_irq_stack']}/2208바이트.75%·상하4+4크롭·승인 플레이어 에셋 SHA 유지.107의 오프라인 구역 계획은 여전히 ROM 미연결입니다.</p><p>ROM SHA256 <code>{build['rom_sha256']}</code></p><p>tools/build_native.py,prepare_followthrough_probe.py,validate_heli_performance.py(36000LCD/mode8),analyze_frame_pacing.py,performance_regressions.py와 동봉된 입력·GPU·오디오 증거로 재현합니다.</p></html>'''
 (R/'docs/aggressive-performance-0.5.108.ko.html').write_text(page)
 backlog=f'''# 0.5.108 남은 작업

107→108 평균23.375→{perf['overall']['fps']:.3f}fps({delta:+.2f}%). M1{perf['m1_active']['fps']:.3f}, M2관측{perf['m2_observed']['fps']:.3f}. M2동일카메라8000..24000 23.368→27.847fps.30fps는 미달.

가림 계산 해제는 기존 캐시를 유지하며64타일 여유 조건과128틱 간격을 사용한다. PCM효과음 직접읽기·팔레트 검색IWRAM 포함. 추가 그림/음질 손실 없음. 흰10→11,late11→12. M1>=100ms35→32,>=8LCD8→9,p9983.71ms,최장251→234ms.

|우선순위|작업|완료 기준|
|---|---|---|
|0|ROM 중복 제거·사전 계산 예산 확보|현재여유1,047,788B. PCM168효과음의중복IMA817,720B제거 후보. 약1.78MiB가능,아직미적용. M1핫구역배경/팔레트에배정후실측|
|1|M1 로직·객체·HUD 비용 감소|M1현재23.637fps. 충돌/적/탄환/입력 보존, 장식 생성·HUD 팔레트 비용부터 측정|
|2|구역 타일·팔레트 사전 배치의 ROM 연결|107 오프라인53이미지+659패치 계획은 여전히 미연결. 기존 에셋 표현 교체와 동적 경계 처리 필요|
|3|흰 화면·긴 프레임|11/35816LCD,late12. 가림 재활성화·팔레트/타일 전체 재구축과 전송 상한 개선|
|4|지속4bpp/BG HUD|주행 각각1커밋. 강제활성화는 여전히 미채택. 정확 색/용량/성능 검증|
|5|두 번째 데모|scene4tick112/error0x0b1e0000 미해결|
|6|캠페인·보스·엔딩|M1results→M2일부만 검증. M2이후/헬기격파/각보스/엔딩 입력 전용 검증|
|7|실기2P·메모리·음향|EWRAM{memory['ewram_free']}바이트,스택앞IWRAM{memory['iwram_before_reserved_stack_free']}바이트. Timer2링크/Timer3음향 두기기 검증|
|8|나머지 원본 의미|48슬롯팔레트·과밀strip·combat overlay·59D1/120D5/318A3A4·YM정밀도·영구랭킹·언어/설정|

ROM `{build['rom_sha256']}`, 동결실행 build/aggressive108/g2. 보고서 docs/aggressive-performance-0.5.108.ko.html, 데이터 generated/play-validation-0.5.108.json. 전캠페인/실기2P 완료 아님.
'''
 (R/'docs/remaining-work-0.5.108.ko.md').write_text(backlog)
 (R/'docs/checkpoint-0.5.108.ko.md').write_text(backlog+'\n최종src는GPU/오디오/주행과동일. 후보A~F와기준프로파일은docs/aggressive-performance-0.5.108에보존. bg-unprune회귀가commit後ready0에서도작동하며hiddenlayer/64tile경계/정확RGB를검사한다.\n')
 entry=f'- Latest checkpoint2026-11-02 v0.5.108: cache-preserving retirement of latched BG occlusion under conservative visible/hidden cell bound+64tile headroom every128ticks; no added visual/audio loss. Exact PCM SFX span loads, palette search IWRAM/FIX size changes.107->10823.375->{perf["overall"]["fps"]:.3f}FPS(+{delta:.2f}%),M1{perf["m1_active"]["fps"]:.3f},M2observed{perf["m2_observed"]["fps"]:.3f}; matchedcamera8000..24000 23.368->27.847.30NOTmet. M1>=100ms35->32,>=8LCD8->9,p9983.71,max251->234ms;white10->11,late11->12.3GPUexact/host/unpruneASan/audio0mismatch/UIpass; second-demo0x0b1e0000remains. M1results->M2partialonly. ROM{build["rom_sha256"]}; EWRAM{memory["ewram_free"]},IWRAM{memory["iwram_before_reserved_stack_free"]}beforestack,observedmain{memory["observed_main_stack"]}IRQ{memory["observed_irq_stack"]}.75percent4+4/playerSHA/audio6000Hz4SFX retained.107offlineplansstillNOTROMlinked. See docs/checkpoint-0.5.108.ko.md and docs/remaining-work-0.5.108.ko.md. Tests/ROM authorized; continue without approval.\n\n'
 for n in ['AGENTS.md','README.ko.md']:
  p=R/n;s=p.read_text();
  if not s.startswith('- Latest checkpoint2026-11-02 v0.5.108:'):p.write_text(entry+s)
 (D/'README.txt').write_text('Final G2 matching ROM/ELF/input hashes; baseline107 not rerun. New M2 improvement does not solve M1 or30FPS. White/late each+1. G failed IWRAM link and has NO gameplay evidence. Source fixture unprune and ASan exact; fullcampaign/physical2P unverified. Replay scripts refer to build paths; evidence files are under this directory.\n')
 out=R.parent/'release';out.mkdir(exist_ok=True)
 with zipfile.ZipFile(out/'metal-slug-gba-native-v0.5.108-evidence.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9)as z:
  for p in sorted(D.rglob('*')):
   if p.is_file():z.write(p,p.relative_to(R/'docs'))
 print(json.dumps(dict(version='0.5.108',fps=perf['overall']['fps'],gain=delta,memory=memory,rom=build['rom_sha256']),ensure_ascii=False))
if __name__=='__main__':main()
