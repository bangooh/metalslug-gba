#!/usr/bin/env python3
"""Freeze measured candidates and distinguish offline plans from ROM support."""
import base64,csv,difflib,hashlib,json,re,shutil,struct,wave,zipfile
from pathlib import Path
from report_reference_106 import interval,copy_files
R=Path(__file__).resolve().parents[1];B=R/'build/region107';C=B/'c2';D=R/'docs/background-regions-0.5.107'
def read(p):return json.loads(p.read_text())
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 D.mkdir(exist_ok=True);old=read(R/'generated/play-validation-0.5.106.json');build=read(C/'build.json');summary=read(C/'summary.json')
 assert build==read(R/'generated/native-build.json')
 assert sha(Path(build['rom']))==sha(C/'rom.gba')==build['rom_sha256']
 assert sha(Path(build['elf']))==sha(C/'native.elf')==build['elf_sha256']
 assert not summary['invalid_benchmark_reasons'] and summary['exit_code']==0
 trace=[json.loads(l)for l in (C/'trace.jsonl').read_text().splitlines()];last=trace[-1]
 assert not any(last[k]for k in ['app.error','audio.fault','audio.clipped'])
 rows=list(csv.DictReader((C/'commits.csv').open()));active=[r for r in rows if r['playing']=='1' and r['presentation']=='0' and r['demo']=='0'];active=active[next(i for i,r in enumerate(active)if int(r['live0'])):]
 perf=dict(overall=summary['active_play'],m1_active=interval([r for r in active if r['scene']=='0']),m2_observed=interval([r for r in active if r['scene']=='1']),white_frames=summary['execution']['all_white_game_frames'],game_lcd_frames=summary['execution']['game_lcd_frames'],late_transfers=last['video.late_transfers'],bg_backend='Conditional exact4bpp/BG HUD; only1 commit each in recorded run. Main gameplay remains8bpp/OBJ HUD.')
 transitions=[];previous=None
 for t in trace:
  state=(t['session.state'],t['scene.scene'],t['playing'],t['presentation'])
  if state!=previous:transitions.append({k:t[k]for k in ['lcd_frame','world.ticks','world.results.progress','world.weapons.camera_x','session.state','scene.scene','playing','presentation']});previous=state
 assert any(t['session.state']==2 and t['scene.scene']==0 for t in transitions)
 assert any(t['session.state']==1 and t['scene.scene']==1 and t['world.results.progress']==1 for t in transitions)
 pacing=read(B/'pacing-c2/measurement.json');audio=read(B/'audio/verification.json');frontend=read(B/'frontend/summary.json')
 assert audio['rom_sha256']==build['rom_sha256'] and audio['restart_mismatches']==0
 assert all(frontend[k]['exit_code']==0 for k in ['tutorial','pause','link-cancel','ranking'])
 assert frontend['demo']['last']['app.error']==0x0b1e0000
 sizes={n:int(re.search(r'^'+re.escape(n)+r'\s+(\d+)',build['section_sizes'],re.M).group(1))for n in ['.bss','.iwram']}
 memory=dict(old['memory']);memory.update(rom_bytes=build['rom_bytes'],rom_free=33554432-build['rom_bytes'],ewram_used=sizes['.bss'],ewram_free=262144-sizes['.bss'],iwram_before_stack_used=sizes['.iwram'],iwram_before_reserved_stack_free=0x6700-sizes['.iwram'])
 ram=(C/'iwram.bin').read_bytes();sentinel=struct.pack('<I',0xa55a3cc3)
 for name,lo,hi in [('observed_main_stack',0x6700,0x7700),('observed_irq_stack',0x7700,0x7fa0)]:memory[name]=hi-next((i for i in range(lo,hi,4)if ram[i:i+4]!=sentinel),hi)
 assert memory['ewram_free']>=0 and memory['iwram_before_reserved_stack_free']>=0 and memory['observed_main_stack']<4096 and memory['observed_irq_stack']<2208
 compiler=read(R/'generated/background-regions/native-bg-regions.json');compiler_summary={k:v for k,v in compiler.items()if k not in ['records','source_hashes','input_hashes']}
 assert compiler['regions']==712 and compiler['original_pixels_verified']==33988672 and not compiler['rom_linked']
 hardware=[]
 for n in [6428,8810,25000]:
  d=B/f'snapshot-{n}';text=(d/'comparison.txt').read_text();assert all(k in text for k in ['BG visible RGB/priority mismatches0','BG committed VRAM/palette mismatches0','FIX mismatches0','FIX committed OAM/VRAM/palette mismatches0'])
  obj=read(d/'object-hardware.json');assert not any(obj[k]for k in ['obj_tile_mismatched_bytes','oam_mismatched_bytes','palette_mismatched_bytes'])
  hardware.append(dict(requested_lcd=n,comparison=text,objects=obj))
 for n in ['regressions-final.log','formats-asan.log']:assert 'PASS' in (B/n).read_text() and 'ERROR:' not in (B/n).read_text() and 'runtime error:' not in (B/n).read_text()
 approved=sha(R/'generated/player.tiles');assert approved=='ca7febc19eeda7cf62af36f036cdb91f290bf8be8ac279344e03bf269c74be32'
 rejected={n:read(B/n/'summary.json')for n in ['a','b']}
 features=dict(old['renderer_features']);features.update(hud='64/128/256 tiles; VRAM base follows actual1..3 BG map blocks; exact layout checks. Automatic disable with8bpp retained because forced HUD candidate regresses.',offline_region_plans=compiler_summary,tile_hash='ARM rotate/add in place of16 per-word multiplies; exact pixel comparison retained. End-to-end gain negligible.',incomplete='All authored image/patch interiors compiled OFFLINE. Source palette plans and packed tiles are NOT linked to ROM. Cross-patch seams, moving-column boundaries, palette scheduling and streaming remain pending.')
 changes=['All53 scene images and659 patches compiled into531073 authored8x8 windows; exact original/fragment comparison and local palette round trip. Offline output only.', 'BG HUD cache capacity now follows actual map count;64tile compact HUD supported. Sustained8bpp/HUD experiment rejected.', 'Exact background cache hash uses rotate/add; no quantization. Full-run FPS effect~0.06%, not a material win.']
 report=dict(old);report.update(version='0.5.107',rom_sha256=build['rom_sha256'],elf_sha256=build['elf_sha256'],fingerprint=build['fingerprint'],memory=memory,performance=perf,frame_pacing=pacing,scene_transitions=transitions,implemented_optimizations=changes,renderer_features=features,hardware_snapshots=hardware,frontend_results=frontend,catalog_decode=dict(assets=192,decoded_samples=13821728,pcm_fnv32='a2c8099b',faults=0,last_verified_version='0.5.107',rerun_this_revision=True),audible_output=dict(captured_this_revision=True,**summary['execution'],audio_fault=0,clipped=0,ring_verification=audio,limitation='20s emulator output captured; no exhaustive listening or physical GBA claim.'),comparison=dict(baseline_version='0.5.106',baseline_rom_sha256=old['rom_sha256'],baseline_performance=old['performance'],baseline_rerun=False,method=pacing['comparison']),regressions=dict(full_host='regressions-final.log',new_formats='3 exact4bpp fixtures and1 exact8bpp fallback;32 HUD edits/fades,1..3map placement checks',asan_ubsan='formats-asan.log, detect_leaks=0 for ptrace environment'),offline_region_compiler=compiler_summary,rejected_candidates=rejected,target_fps=30,target_met=False,priority='Replace the oversized BG asset representation and connect source-region plans, cross-patch fallback and palette scheduling;30FPS, sustained4bpp/HUD and white-transfer fixes remain pending.',evidence='docs/background-regions-0.5.107/')
 write(R/'generated/play-validation-0.5.107.json',report)
 copy_files(C,D/'play',['build.json','summary.json','execution.json','trace.jsonl','commits.csv','inputs.csv','entities.csv','probe.txt','symbols.txt','emulator.log'])
 copy_files(B/'pacing-c2',D/'pacing',['measurement.json','baseline.csv','candidate.csv'])
 copy_files(B/'audio',D/'audio',['verification.json','observer.log'])
 copy_files(B/'frontend',D/'frontend',['summary.json'])
 for n in ['tutorial','pause','link-cancel','ranking','demo']:copy_files(B/'frontend'/n,D/'frontend'/n,['trace.jsonl','commits.csv','inputs.csv','execution.json','emulator.log'])
 for n in [6428,8810,25000]:copy_files(B/f'snapshot-{n}',D/f'snapshot-{n}',['comparison.txt','object-hardware.json','fixture.bin','fixed.bin','hud-active.bin','hud-count.bin','vram.bin','pal.bin','oam.bin','ewram.bin','screen.png','capture.log'])
 for n in ['a','b']:copy_files(B/n,D/f'rejected-{n}',['build.json','summary.json','trace.jsonl','commits.csv','inputs.csv'])
 copy_files(B/'rejected-hud-snapshot',D/'rejected-a/snapshot',['comparison.txt','fixed.bin','hud-active.bin','vram.bin','pal.bin','oam.bin','ewram.bin'])
 for name in ['candidate-a-src','candidate-b-src']:copy_files(B/name,D/name,[p.name for p in (B/name).iterdir()])
 for name in ['regressions-final.log','formats-asan.log','region-compiler.log']:shutil.copyfile(B/name,D/name)
 shutil.copyfile(R/'generated/background-regions/native-bg-regions.json',D/'region-manifest.json')
 shutil.copyfile(R/'generated/play-validation-0.5.107.json',D/'validation.json')
 with wave.open(str(D/'audio/output.wav'),'wb')as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(summary['execution']['audio_rate']);w.writeframes((C/'audio.s16le').read_bytes())
 delta=(perf['overall']['fps']/old['performance']['overall']['fps']-1)*100
 png=''.join('<figure><img alt="실제 자동 플레이 캡처" src="data:image/png;base64,'+base64.b64encode((D/f'snapshot-{n}/screen.png').read_bytes()).decode()+'"><figcaption>LCD '+str(n)+'</figcaption></figure>'for n in [6428,8810,25000])
 page=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>v0.5.107 배경 사전 변환과 조건부 HUD 검증</title><style>body{{font:16px/1.65 system-ui,sans-serif;max-width:1000px;margin:32px auto;padding:0 24px;color:#182532;background:#f8fafc}}h1{{font-size:30px}}h2{{margin-top:36px}}table{{border-collapse:collapse;width:100%;background:white}}th,td{{border:1px solid #ccd6df;text-align:left;padding:10px}}.note{{border-left:5px solid #c68124;background:#fff4df;padding:16px}}img{{max-width:100%;image-rendering:pixelated}}figure{{margin:20px 0}}code{{overflow-wrap:anywhere}}</style>
<h1>배경 사전 변환과 조건부 HUD 검증 · v0.5.107</h1><p>2026-11-02 · 입력 전용 mGBA 0.10.2 검증</p>
<p class="note"><b>상시 BG HUD·4bpp와 평균30fps는 미완료입니다.</b> 최종 후보 평균 {old['performance']['overall']['fps']:.2f} → {perf['overall']['fps']:.2f}fps ({delta:+.3f}%). 긴 끊김 지표도 같아 체감 개선으로 해석하지 않습니다. 새 사전 변환 결과는 오프라인 계획이며 게임 루프에서 읽지 않습니다.</p>
<h2>왜 조건부인가</h2><p>75% 축소된12×12 원본 타일을 GBA8×8에 조합하면 경계에서 다른 원본 팔레트가 섞입니다.4bpp 한 타일은 투명색 외15색까지만 사용할 수 있습니다. 각 타일의 색 수가 맞아도 HUD가4개 팔레트 뱅크를 차지하므로 배경을 나머지12개 뱅크에 동시에 배치해야 합니다.8bpp 배경에 BG HUD를 강제 병용하면 VRAM과 색 예산이 줄어 캐시 재생성과 전송이 증가합니다.</p>
<table><tr><th>36,000 LCD 시험</th><th>평균 FPS</th><th>흰 화면</th><th>BG HUD 커밋</th><th>판정</th></tr><tr><td>기준106</td><td>{old['performance']['overall']['fps']:.3f}</td><td>10</td><td>1</td><td>기준</td></tr><tr><td>A:8bpp에서도 HUD 유지 시도</td><td>{rejected['a']['active_play']['fps']:.3f}</td><td>14</td><td>6,912</td><td>미채택</td></tr><tr><td>B:팔레트 검색 변경</td><td>{rejected['b']['active_play']['fps']:.3f}</td><td>{rejected['b']['execution']['all_white_game_frames']}</td><td>1</td><td>미채택</td></tr><tr><td>최종107 C2</td><td>{perf['overall']['fps']:.3f}</td><td>10</td><td>1</td><td>조건부 유지</td></tr></table>
<p>동일한 상태 대응 입력 정책으로 별도 실행했습니다. 전투 체류 시간과 상태가 완전히 같은 비교는 아닙니다. A도 용량 실패 시 복귀하므로 전 구간 상시 구현이라고 부르지 않습니다. A의 실제 BG HUD 장면에서도 RGB/VRAM 차이0을 확인했습니다.</p>
<h2>이번 사전 변환 결과</h2><p>원본53이미지와659패치의 기본 배치 내부8×8 조합531,073개를 처리했습니다. 독립적인12×12 원본 디코더와33,988,672픽셀을 비교했고 모두 일치했습니다. 중복 제거 후231,858개 픽셀 타일을 각각 로컬 색인과 원본 색 슬롯 대응표로 왕복 검증했습니다. 플레이 캡처에서 생산 에셋을 추출하지 않았고 색을 줄이지 않았습니다.</p>
<table><tr><th>항목</th><th>결과</th></tr><tr><td>원본 색 식별자15개 이하 타일</td><td>230,477개</td></tr><tr><td>15개 초과 / 최대</td><td>1,381개 /26개</td></tr><tr><td>단독 표 기준12뱅크 배치 성공</td><td>465 /712개</td></tr><tr><td>타일·키·맵 계획</td><td>원시18,588,500바이트 / zlib7,635,963바이트</td></tr><tr><td>구역 팔레트 계획</td><td>977,968바이트(zlib)</td></tr><tr><td>현재 ROM 여유</td><td>{memory['rom_free']:,}바이트</td></tr></table>
<p>서로 다른 원본 슬롯이 같은 RGB를 내는 경우도 있으므로 식별자 수는 보수적인 값입니다. 탐욕 배치가12뱅크를 넘었다고 최적 배치도 불가능하다는 뜻은 아닙니다. 단독 표의 성공은 여러 레이어·파괴 경계·전환 전체의 성공을 보장하지 않습니다. zlib는 작업용 저장 형식이며 GBA 스트리밍 구현이 아닙니다. 기존 에셋 대체, 패치 이음새/독립 이동 칼럼 처리와 팔레트 예약을 연결해야 합니다.</p>
<h2>채택한 런타임 변경</h2><p>HUD는64/128/256타일 크기를 사용하고 실제 배경 맵 수에 맞춰 VRAM 위치를 계산합니다.128타일 HUD 기준 배경 캐시는1/2/3레이어에서896/864/832타일을 확보합니다(기존 모두832). 실패한 HUD 시도 후4bpp 재포장에 오래된 예약 값을 넘기지 않도록 정리했습니다. 배경 타일 해시는 ARM 회전·덧셈으로 계산하되 충돌 시 픽셀 전체를 비교합니다. 최종 주행에서 BG HUD와4bpp 적용은 여전히 각각1회뿐입니다.</p>
<h2>프레임·게임 검증</h2><p>M1 결과 화면 → M2 일부까지 진행했습니다. M1 평균 {perf['m1_active']['fps']:.3f}, M2 관측 구간 {perf['m2_observed']['fps']:.3f}fps입니다. M1 ticks10..5200의100ms 이상 지연35→35회, p99 {pacing['candidate']['p99_ms']:.2f}ms, 최장 {pacing['candidate']['max_ms']:.2f}ms입니다. 흰10/{perf['game_lcd_frames']:,}LCD, 늦은 전송11회가 남았습니다.</p><p>호스트 회귀·4bpp/HUD ASan/UBSan 검사 통과. 세 실제 GPU 캡처의 BG/FIX/OBJ 차이0. 오디오 링14,121회 재연결 불일치0. 튜토리얼·일시정지/복귀·랭킹·링크 취소 통과. 두 번째 데모 scene4/tick112/error0x0b1e0000은 그대로 재현됩니다. 전 캠페인·M2 클리어·헬기 격파·실기2P는 미검증입니다.</p>
{png}<h2>메모리와 재현</h2><p>ROM {build['rom_bytes']:,}바이트. EWRAM 여유284바이트, 예약 스택 앞 IWRAM 여유16바이트. 주행 관측 메인 스택3036/4096, IRQ264/2208바이트. 이는 전체 캠페인·2P의 상한 증명이 아닙니다.75%·상하4+4크롭·승인 플레이어 에셋 해시는 유지했습니다.</p><p>ROM SHA256 <code>{build['rom_sha256']}</code></p><p>소스의 tools/prepare_background_regions.py로 계획 생성, tools/build_native.py로 빌드, tools/validate_heli_performance.py와 tools/analyze_frame_pacing.py로 재측정합니다. 상세 입력·해시·GPU·오디오 증거는 동봉된 evidence.zip을 따릅니다.</p></html>'''
 (R/'docs/background-regions-0.5.107.ko.html').write_text(page)
 backlog=f'''# 0.5.107 남은 작업

평균30fps와 안정적인 프레임 간격은 미달이다.106→107 평균23.361→23.375fps(+0.059%, 유의미한 개선 아님). M1>=100ms35회, p9983.71ms, 최장251.14ms 그대로. 흰10,late11.

|우선순위|남은 작업|현재 근거와 완료 기준|
|---|---|---|
|1|구역 사전 변환 결과의 ROM 연결·기존 에셋 대체|53이미지+659패치 기본 배치 내부는 오프라인 변환 완료. 타일계획7.636MB+팔레트0.978MB로 ROM여유1.041MB 초과. 기존4×4/preflip표현 교체, 스트리밍·소스 위치 매핑 필요|
|2|파괴 경계·이동 칼럼·전환·팔레트 배치|단독표465/712개만12뱅크 탐욕 배치 성공. 동시 레이어 적합성 미검증.1381개 타일은 원본 색 식별자15개 초과; 색 공유/선별 양자화/정확한 fallback 검토|
|3|지속적인4bpp/BG HUD|최종 주행 각각1커밋.8bpp+HUD 유지 후보6912커밋이나 평균22.612로 회귀하여 미채택. 활성화 횟수만으로 성능 개선을 주장하지 않음|
|4|전반30fps·끊김·흰 화면|BG/strip/그림 수집/로직 비용 및 전체 캐시 재생성 감소.100ms35회와흰10/late11해결|
|5|두 번째 자동 데모|scene4/tick112/error0x0b1e0000 재현,미해결|
|6|캠페인·보스·엔딩|M1results→M2일부만 검증. M2이후·헬기격파·각보스/Slug/엔딩 입력 전용 검증|
|7|실기2P·메모리·음향|Timer2링크/Timer3음향 실제 두기기 검증. EWRAM284,IWRAM예약스택앞16바이트. 관측main3036/4096,IRQ264/2208|
|8|나머지 원본 의미·기능|48슬롯 팔레트·과밀strip·승인combat overlay·59D1/120D5/318A3A4·YM정밀도·영구랭킹·언어/설정|

채택: 조건부 HUD의64/128/256크기·맵수 기반 VRAM배치, 정확한 회전/덧셈 타일해시. 오프라인 계획은 ROM에 연결되지 않았다. 기본표 내부만 베이크하며 파괴패치 이음새/독립이동 칼럼/추가자동애니메이션상태는 아직 포함하지 않는다.75%·상하4+4·승인플레이어 유지.

ROM `{build['rom_sha256']}`, 최종동결 build/region107/c2. 보고서 docs/background-regions-0.5.107.ko.html, 검사 generated/play-validation-0.5.107.json. 호스트·ASan/UBSan·세GPU스냅샷 통과. 오디오링14121회 불일치0. 튜토리얼·랭킹·pause·링크취소 통과. 전캠페인/실기2P 미검증.
'''
 (R/'docs/remaining-work-0.5.107.ko.md').write_text(backlog)
 checkpoint=backlog+'\n재현: python3 tools/prepare_background_regions.py; python3 tools/build_native.py --incremental --cc ARM_GCC; python3 tools/prepare_followthrough_probe.py --cc ARM_GCC --output PROBE; CAPTURE_AUDIO=1 python3 tools/validate_heli_performance.py --output RUN --probe PROBE/probe.txt --mgba-prefix MGBA --frames 36000 --mode 8.\n'
 (R/'docs/checkpoint-0.5.107.ko.md').write_text(checkpoint)
 entry=f'- Latest checkpoint2026-11-02 v0.5.107: all53images+659patches authored8x8 interiors compiled OFFLINE;531073windows,231858unique tiles,33988672pixels exact. NOT ROM-linked;7.636MB tile plans+0.978MB palette plans exceed1.041MB spare.465/712standalone tables greedy fit12banks;1381tiles>15source identities(max26), not proof about RGB minimum. Patch seams/moving columns/transitions/streaming pending. CandidateA8bpp+HUD6912commits regressed23.361->22.612 andwhite10->14; rejected. FinalC2 conditionalHUD64/128/256+dynamicVRAM, exact rotate/add tile hash:23.375FPS(+0.059%, negligible),>=100ms35,max251ms,p9983.71 unchanged;white10,late11. ActualBG4/HUD1commit each. M1results->M2partial,error/audio/clipped0;host/ASan/3GPUexactchecks pass,14121audio wraps0mismatch. Tutorial/pause/ranking/linkcancel pass; second-demo scene4tick1120x0b1e0000 persists. ROM{build["rom_sha256"]}; EWRAM284,IWRAM16beforestack,main3036/4096 IRQ264/2208. Preserve75percent4+4/playerSHA. Fullcampaign/M2clear/heli defeat/physical2P NOTverified. Next replace asset representation/connect offline region plans and palette scheduling. docs/checkpoint-0.5.107.ko.md and docs/remaining-work-0.5.107.ko.md. Tests/ROM authorized; continue without approval.\n\n'
 for filename in ['AGENTS.md','README.ko.md']:
  path=R/filename;text=path.read_text()
  if not text.startswith(entry):path.write_text(entry+text)
 patches=[]
 for path in sorted((B/'baseline/src').glob('*')):
  current=R/'src'/path.name
  if path.is_file() and current.is_file() and path.suffix in ['.c','.h']:
   patches.extend(difflib.unified_diff(path.read_text().splitlines(True),current.read_text().splitlines(True),fromfile='v106/src/'+path.name,tofile='v107/src/'+path.name))
 (D/'runtime-changes.patch').write_text(''.join(patches))
 (D/'README.txt').write_text('Final candidate C2; no material FPS gain. Offline region plans are in generated/background-regions in source ZIP, not linked into ROM. Main run, GPU, audio and UI evidence match final ROM hashes. Rejected A/B have independent inputs and hashes. Failed C build (IWRAM8byte overflow) is excluded; C2 inlined hash fits16bytes before reserved stacks. Baseline106 results reused, not rerun. See report for commands and limitations.\n')
 with zipfile.ZipFile(R/'dist/metal-slug-gba-native-v0.5.107-evidence.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9)as z:
  for p in sorted(D.rglob('*')):
   if p.is_file():z.write(p,p.relative_to(D))
 print(json.dumps(dict(version='0.5.107',rom=build['rom_sha256'],fps=perf['overall']['fps'],gain_percent=delta,compiler=compiler_summary,memory=memory),ensure_ascii=False))
if __name__=='__main__':main()
