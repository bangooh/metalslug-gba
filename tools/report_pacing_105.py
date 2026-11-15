#!/usr/bin/env python3
"""Package measured v0.5.105 frame pacing, including remaining regressions."""
import base64, csv, difflib, hashlib, json, re, shutil, struct, zipfile
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from analyze_frame_pacing import samples, metrics, LCD_MS

R=Path(__file__).resolve().parents[1]; B=R/'build/pacing105'; D=R/'docs/frame-pacing-0.5.105'
ROM='f862129810081682ef557e4029edc9f701a58dcbd1c4ab4651bc5a56e98789e1'
def read(p): return json.loads(p.read_text())
def write(p,v): p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def interval(rows):
    a,z=rows[0],rows[-1];seconds=(int(z['lcd'])-int(a['lcd']))*LCD_MS/1000;count=int(z['commits'])-int(a['commits'])
    return dict(lcd_start=int(a['lcd']),lcd_end=int(z['lcd']),commits=count,seconds=seconds,fps=count/seconds)
def archive(source,target):
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(source.rglob('*')):
            if p.is_file() and p.suffix in ['.json','.jsonl','.csv','.txt','.log','.bin','.c','.h','.py','.png','.ppm']:
                z.write(p,str(p.relative_to(source)))

def main():
    D.mkdir(exist_ok=True); build=read(B/'f/build.json'); old=read(R/'generated/play-validation-0.5.104.json')
    assert build==read(R/'generated/native-build.json') and build['version']=='0.5.105'
    assert build['rom_sha256']==ROM==sha(B/'f/rom.gba')==sha(Path(build['rom']))
    assert build['elf_sha256']==sha(B/'f/native.elf')==sha(Path(build['elf']))
    summary=read(B/'f/summary.json');assert summary['exit_code']==0 and not summary['invalid_benchmark_reasons']
    rows=list(csv.DictReader((B/'f/commits.csv').open()))
    active=[r for r in rows if r['playing']=='1' and r['presentation']=='0' and r['demo']=='0']
    active=active[next(i for i,r in enumerate(active) if int(r['live0'])):]
    perf=dict(overall=summary['active_play'],m1_active=interval([r for r in active if r['scene']=='0']),m2_observed=interval([r for r in active if r['scene']=='1']),
              white_frames=summary['execution']['all_white_game_frames'],game_lcd_frames=summary['execution']['game_lcd_frames'],late_transfers=summary['last']['video.late_transfers'],
              bg_backend=old['performance']['bg_backend']+'; stable vertical map origin across source-strip recycling, including scroll-only fast path')
    pacing=read(B/'pacing/measurement.json'); a=pacing['baseline']; z=pacing['candidate']
    trace=[json.loads(x) for x in (B/'f/trace.jsonl').read_text().splitlines()];last=trace[-1]
    assert not last['app.error'] and not last['audio.fault'] and not last['audio.clipped']
    transitions=[];previous=None
    for t in trace:
        state=(t['session.state'],t['scene.scene'],t['playing'],t['presentation'])
        if state!=previous:
            transitions.append({k:t[k] for k in ['lcd_frame','world.ticks','world.results.progress','world.weapons.camera_x','session.state','scene.scene','playing','presentation']});previous=state
    assert any(t['session.state']==2 and t['scene.scene']==0 for t in transitions)
    assert any(t['session.state']==1 and t['scene.scene']==1 and t['world.results.progress']==1 for t in transitions)
    sizes={name:int(re.search(r'^'+re.escape(name)+r'\s+(\d+)',build['section_sizes'],re.M).group(1)) for name in ['.bss','.iwram']}
    memory=dict(old['memory']);memory.update(rom_bytes=build['rom_bytes'],ewram_used=sizes['.bss'],ewram_free=262144-sizes['.bss'],iwram_before_stack_used=sizes['.iwram'],iwram_before_reserved_stack_free=0x6700-sizes['.iwram'])
    ram=(B/'f/iwram.bin').read_bytes();sentinel=struct.pack('<I',0xa55a3cc3)
    for name,lo,hi in [('observed_main_stack',0x6700,0x7700),('observed_irq_stack',0x7700,0x7fa0)]:
        first=next((i for i in range(lo,hi,4) if ram[i:i+4]!=sentinel),hi);memory[name]=hi-first
    assert memory['ewram_free']>=0 and memory['iwram_before_reserved_stack_free']>=0
    assert memory['observed_main_stack']<4096 and memory['observed_irq_stack']<2208
    frontend=read(B/'frontend/summary.json');assert all(frontend[n]['exit_code']==0 for n in ['tutorial','pause','link-cancel'])
    assert frontend['demo']['last']['app.error']==0x0b1e0000
    hardware=[]
    for n in [6428,6930,8810,25000]:
        p=B/f'snapshot-{n}';text=(p/'comparison.txt').read_text()
        assert all(s in text for s in ['BG visible RGB/priority mismatches0','BG committed VRAM/palette mismatches0','FIX mismatches0','FIX committed OAM/VRAM/palette mismatches0'])
        h=read(p/'object-hardware.json');assert all(h[k]==0 for k in ['obj_tile_mismatched_bytes','oam_mismatched_bytes','palette_mismatched_bytes'])
        hardware.append(dict(requested_lcd=n,comparison=text,object_hardware=h))
    changes=['Keep BG Y origin tied to an unchanged strip; preserve that origin in scroll-only fast path.',
             'Replace bounded per-fragment HUD division and repeated rectangular cell scans with exact arithmetic/row bit counts.',
             'Map HUD colors per rectangle under fragmented actor palette pressure, with same-frame mapping reuse.',
             'Preserve source HUD geometry when a temporary packed fallback uses the following staging span; explicitly mark fallback output.']
    report={k:old[k] for k in ['verified_campaign_scope','fixed_frame_bytes','input_method','lcd_clock','object_palette_policy','visual_tradeoffs','audio_settings','pcm_cache']}
    report.update(version='0.5.105',rom_sha256=ROM,elf_sha256=build['elf_sha256'],fingerprint=build['fingerprint'],complete_port=False,m1_clear_verified=True,target_fps=30,target_met=False,
                  catalog_decode=dict(old['catalog_decode'],last_verified_version='0.5.104',rerun_this_revision=False),
                  audible_output=dict(captured_this_revision=False,audio_fault=0,clipped=0,limitation='Audio source/assets/settings unchanged; previous104 catalog/PCM tests retained as historical evidence, not rerun105.'),
                  memory=memory,scene_transitions=transitions,performance=perf,frame_pacing=pacing,implemented_optimizations=changes,
                  comparison=dict(baseline_version='0.5.104',baseline_rom_sha256=old['rom_sha256'],baseline_performance=old['performance'],method=pacing['comparison'],baseline_rerun=False,
                                  limitations='Frozen104 baseline. Same controller and36000LCD budget, not identical combat dwell or final M2 position. Targeted pacing uses4728 vs4730 comparable eligible pairs, not entire-session FPS. Some isolated frames became slower; worst268ms remains.'),
                  frontend_results=frontend,frontend_verified_paths=['logo/title','first demo/ranking before known second-demo error','tutorial/game','pause/resume','no-peer link cancel','M1 clear/results to M2 entry'],
                  hardware_snapshots=hardware,regressions=dict(relevant_host=(B/'final-regressions.log').read_text(),asan_ubsan=(B/'render-asan.log').read_text(),asan_options='detect_leaks=0: LeakSanitizer unsupported under this executor; address/undefined checks enabled.'),
                  priority='Remaining cold BG palette rebuild and isolated HUD rebuild spikes; nominal30FPS, white transfer frames, known second-demo error, full campaign and physical2P.',evidence='docs/frame-pacing-0.5.105/')
    write(R/'generated/play-validation-0.5.105.json',report);write(D/'measurement.json',report);write(D/'baseline-measurement.json',old);write(D/'build.json',build)
    for name in ['final-regressions.log','render-asan.log','object-snapshots.log']:
        shutil.copyfile(B/name,D/name)
    for name in ['baseline.csv','candidate.csv','measurement.json']:
        shutil.copyfile(B/'pacing'/name,D/('pacing-'+name))
    archive(B/'f',D/'play.zip');archive(B/'frontend',D/'frontend.zip')
    with zipfile.ZipFile(D/'snapshots.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as zz:
        for n in [6428,6930,8810,25000]:
            for p in (B/f'snapshot-{n}').iterdir():
                if p.is_file() and p.suffix in ['.bin','.json','.txt','.log','.c','.png','.ppm']:zz.write(p,f'{n}/{p.name}')
    with zipfile.ZipFile(D/'diagnosis104.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as zz:
        for base in [B/'profile104',B/'dump104',R/'build/perf104/profile-b']:
            for p in base.iterdir():
                if p.is_file() and p.suffix in ['.csv','.json','.log','.bin']:zz.write(p,f'{base.name}/{p.name}')
        for name in ['diagnosis.c','dump.c']:zz.write(B/name,name)
    diff=[]
    for name in ['src/background_video.c','src/fixed_object.c','src/fixed_object.h','src/native_video.c','VERSION']:
        diff.extend(difflib.unified_diff((B/'baseline'/name).read_text().splitlines(True),(R/name).read_text().splitlines(True),fromfile='104/'+name,tofile='105/'+name))
    (D/'changes.patch').write_text(''.join(diff))
    # Exact engineering chart: real commit timestamps, no synthesized imagery.
    fig,axes=plt.subplots(2,1,figsize=(10,6.4),layout='constrained')
    colors=['#8a949e','#087f8c']
    for path,label,color in [(R/'build/perf104/all-b','0.5.104',colors[0]),(B/'f','0.5.105',colors[1])]:
        data=samples(path);subset=[r for r in data if 2868<=r['tick']<=2893]
        axes[0].plot([r['tick'] for r in subset],[r['ms'] for r in subset],'-o',markersize=3,label=label,color=color)
    axes[0].set(title='M1 camera 2648: repeated HUD stalls reduced',xlabel='Simulation tick (separate input-only runs)',ylabel='New-frame interval (ms)',ylim=(0,285));axes[0].legend();axes[0].grid(alpha=.18)
    labels=['100 ms or longer','8 LCD frames or longer\n(about 134 ms)'];values=[[a['at_least_100ms'],a['at_least_8_lcd_frames']],[z['at_least_100ms'],z['at_least_8_lcd_frames']]]
    for j,(v,label,color) in enumerate(zip(values,['0.5.104','0.5.105'],colors)):
        bars=axes[1].bar([i+(j-.5)*.30 for i in range(2)],v,width=.30,label=label,color=color);axes[1].bar_label(bars,padding=3)
    axes[1].set(title='M1 ticks 10–5200: long frame intervals',xticks=[0,1],xticklabels=labels,ylabel='Occurrences',ylim=(0,59));axes[1].legend();axes[1].grid(axis='y',alpha=.18)
    fig.savefig(D/'frame-pacing.png',dpi=150);plt.close(fig)
    image=base64.b64encode((D/'frame-pacing.png').read_bytes()).decode()
    pct=(perf['overall']['fps']/old['performance']['overall']['fps']-1)*100
    html=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>M1 끊김 분석 · v0.5.105</title>
<style>body{{margin:0;background:#f4f7f8;color:#19313d;font:16px/1.8 system-ui,sans-serif}}main{{max-width:980px;margin:auto;padding:40px 24px}}h1{{line-height:1.3;font-size:36px}}h2{{margin-top:36px}}table{{width:100%;border-collapse:collapse;background:white}}td,th{{padding:12px;text-align:left;border-bottom:1px solid #d9e2e5}}img{{max-width:100%;height:auto}}.note{{padding:18px;background:#e6f1f2;border-left:4px solid #087f8c}}code{{overflow-wrap:anywhere}}</style><main>
<p>METAL SLUG · GBA NATIVE · 2026-11-02</p><h1>평균보다 긴 프레임이 문제였습니다</h1>
<p class="note">M1의 짧고 반복적인 끊김을 재현하고 v0.5.105에서 완화했습니다. 100ms 이상 지연은 <b>50회 → 33회(34% 감소)</b>, 약134ms 이상은 <b>24회 → 9회(62.5% 감소)</b>입니다. 최악의268ms 지연과 평균30fps 미달은 남아 있습니다.</p>
<table><tr><th>측정</th><th>0.5.104</th><th>0.5.105</th></tr>
<tr><td>동일 자동 입력 정책 · 36,000 LCD 평균</td><td>{old['performance']['overall']['fps']:.2f}fps</td><td>{perf['overall']['fps']:.2f}fps ({pct:+.2f}%)</td></tr>
<tr><td>M1 전투 표본의 99백분위 프레임 시간</td><td>{a['p99_ms']:.1f}ms</td><td>{z['p99_ms']:.1f}ms</td></tr>
<tr><td>100ms 이상 지연</td><td>{a['at_least_100ms']}회 / {a['samples']}프레임</td><td>{z['at_least_100ms']}회 / {z['samples']}프레임</td></tr>
<tr><td>최악의 프레임 시간</td><td>{a['max_ms']:.1f}ms</td><td>{z['max_ms']:.1f}ms</td></tr>
<tr><td>흰 LCD프레임 / 게임 LCD프레임</td><td>14 / 35,859</td><td>{perf['white_frames']} / {perf['game_lcd_frames']:,}</td></tr></table>
<img alt="실제 M1 프레임 간격과 긴 지연 횟수 비교" src="data:image/png;base64,{image}">
<h2>확인된 원인과 수정</h2><ol><li><b>배경 기준점 이동:</b> source 세로 타일 줄이 재사용될 때 첫 줄의 높이가 바뀌어 배경 전체를 다시 계산했습니다. 정상 이동 중인 줄에 기준점을 유지하고, 스크롤 전용 빠른 경로에도 같은 기준을 적용했습니다.</li>
<li><b>HUD 팔레트 압박:</b> HUD 전체 색 묶음을 한 팔레트에 넣으려다 실패하면 객체와 HUD 작업을 반복했습니다. 작은 사각 영역별로 정확한 색을 배치하고, 임시 출력 때도 원본 캐시를 보존하도록 변경했습니다.</li>
<li><b>전체 HUD 재구성 비용:</b> 제한된 좌표에서 소프트웨어 나눗셈을 없애고, 사각형 선택 시 반복하던 셀 계산을 행 비트 집계로 바꿨습니다. 이미 배치한 같은 색 매핑도 재사용합니다.</li></ol>
<p>0.5.104 읽기 전용 명령 추적에서 배경 재구성 한 호출222.13ms, HUD prefix90.82ms, 해당 프레임 전체 video prepare230.38ms가 관측됐습니다. 함수 포함 시간이므로 합산하지 않습니다. 모든 긴 프레임의 원인이 이 두 경로라는 뜻은 아닙니다.</p>
<h2>남은 지연과 비교 한계</h2><p>초반 배경 팔레트 용량 초과 후 전체 재구성(tick2132)은 여전히16 LCD프레임, 약268ms입니다. tick3104는6→14 LCD프레임으로 더 느려졌고, 다른 단발성 HUD 갱신 지연도 남아 있습니다. 이번 개선은 긴 지연의 빈도를 줄였으며, 일정30fps나 끊김 완전 제거를 달성한 것은 아닙니다. 약23fps에서는 정상적인2/3 LCD프레임(약33/50ms) 표시 간격 변화도 체감될 수 있습니다.</p>
<p>간격은 매 LCD프레임에서 읽은 commit 카운터 기준이며, 늦은 전송 후 실제 픽셀이 보이기 시작하는 시점과 다를 수 있습니다. 흰 LCD프레임은 별도로 집계했습니다. 프레임 간격 표본은 M1 tick10–5200 중 연속 게임 tick·commit이 각각1 증가한 비데모·비일시정지 기록입니다. 시작·결과 전환과 중복 표시를 제외했습니다. 평균 FPS는 별도의36,000 LCD 세션 지표입니다. 같은 상태 기반 컨트롤러로 입력만 넣었지만 전투 체류·마지막 위치는 다르며, 사람의 수동 플레이나 동일 세계 상태 비교가 아닙니다. 기준104는 보존된 측정이며 이번에 다시 실행하지 않았습니다.</p>
<h2>검증과 보존한 범위</h2><p>M1 클리어·결과 → M2 진입 및 일부 진행, 게임/오디오 fault0, clipping0. 실제 화면4곳의 BG/HUD 픽셀과 OBJ 타일·OAM·팔레트 전송 불일치0. 배경780프레임·HUD90프레임 비교, 팔레트 분산12프레임/임시 출력 복구4회, 실제 M1 재활용 줄 fixture, 통합 회귀와 ASan/UBSan 통과. LeakSanitizer는 실행 환경의 ptrace 제한으로 비활성화했습니다.</p>
<p>추가 에셋 삭제나 음질 하향은 하지 않았습니다. 기존75% 배율·상하4+4픽셀 자르기·6000Hz/4효과음을 유지합니다. 승인된 플레이어 자산 해시도 같습니다. 튜토리얼·일시정지·링크 취소 통과. 기존 두 번째 데모 scene4/tick112 오류0x0b1e0000은 유지됩니다. M2 클리어·전 캠페인·헬기 격파·실기2P는 검증하지 않았습니다.</p>
<p>ROM {build['rom_bytes']:,}바이트. EWRAM 여유{memory['ewram_free']}바이트, 예약 스택 앞 IWRAM 여유{memory['iwram_before_reserved_stack_free']}바이트. 관측 메인 스택{memory['observed_main_stack']}/4096, IRQ{memory['observed_irq_stack']}/2208바이트. 해당 경로의 관측값이며 모든 미션·2P의 상한 증명은 아닙니다.</p>
<p>ROM SHA256 <code>{ROM}</code></p></main></html>'''
    (R/'docs/frame-pacing-0.5.105.ko.html').write_text(html)
    note=f'''# 0.5.105 M1 프레임 간격 개선

실제 끊김을 재현했다. 104→105 전체 자동 입력 평균23.214→{perf['overall']['fps']:.3f}fps. M1 tick10–5200 비교 표본의100ms 이상50→33회,8 LCD프레임 이상24→9회,99백분위100.46→83.71ms. 최악268ms 유지, tick3104의6→14 LCD프레임 회귀도 존재한다. 평균30fps 및 일정 프레임 목표는 미달이다.

BG 재활용 줄의 Y 기준점/scroll-only 경로, HUD 제한 좌표 나눗셈·사각 탐색, 영역별 팔레트 매핑/같은 프레임 재사용, 임시 packed 출력 후 캐시 보존을 적용했다. 추가 에셋·음향 희생은 없다. 원본 물리 팔레트48슬롯/부분strip 의미와 기존 성능용 변경은 별도다.

M1 clear/results→M2 partial, fault0/audio0/clipped0. 흰10/{perf['game_lcd_frames']} LCD프레임, late{perf['late_transfers']}. 4 실제 BG/HUD/OBJ전송 스냅샷 불일치0, host renderer/통합/실제 BG fixture, ASan/UBSan 통과. LSAN만 ptrace 환경 제한으로 껐다. 튜토리얼/pause/linkcancel 통과; 두번째demo scene4/tick112/error0x0b1e0000 유지. 기존104의 오디오/그리기 회귀를105에서 다시 실행한 것으로 표시하지 않는다.

ROM SHA256 {ROM}; ELF {build['elf_sha256']}; fingerprint {build['fingerprint']}. EWRAM261836/262144, IWRAM26248/26368(예약 스택 전), 관측 main2640/4096 IRQ272/2208. 승인 player.tiles 해시 유지. 물리2P/전캠페인/M2clear/헬기격파는 미검증.

현재 보고서 docs/frame-pacing-0.5.105.ko.html, 증거 docs/frame-pacing-0.5.105/, generated/play-validation-0.5.105.json. 기준104와 별도 입력-only 재주행. 초기후보a..e는 선택되지 않았으며 최종은 build/pacing105/f의 동결ROM이다.
'''
    (R/'docs/checkpoint-0.5.105.ko.md').write_text(note)
    backlog=(R/'docs/remaining-work-0.5.104.ko.md').read_text();backlog=backlog[backlog.index('| 우선순위'):backlog.index('이번 완료:')]
    backlog=backlog.replace('0.5.104 LCD7000 표본에서 HUD full rebuild와 비디오26.41ms가 관측됐다. 팔레트/용량 압박을 줄이고 로직 비용도 축소.','105에서 긴 지연 빈도는 줄었지만 cold BG 전체 재구성268ms와 tick3104/4516의 HUD 지연이 남았다. 긴 프레임과 일반2/3 LCD 간격 편차를 함께 축소.').replace('14/35,859 흰 LCD프레임·late15','10/35,860 흰 LCD프레임·late11').replace('IWRAM 예약 스택 전440바이트','IWRAM 예약 스택 전120바이트')
    (R/'docs/remaining-work-0.5.105.ko.md').write_text('# 0.5.105 남은 작업\n\n목표는 게임 전반 평균 명목30fps와 안정적인 프레임 간격이다. 이번 평균23.32fps, M1 긴 지연100ms 이상50→33회. 최악268ms와 일부 단발 회귀가 남는다.\n\n'+backlog+'\n'+note.split('\n\n',1)[1])
    (D/'README.txt').write_text('Final frozen run: build/pacing105/f. Build with tools/build_native.py then native_probe.py / prepare_followthrough_probe.py; validate_heli_performance.py --frames 36000 --mode 8.\nPacing: tools/analyze_frame_pacing.py --baseline build/perf104/all-b --candidate build/pacing105/f --output build/pacing105/pacing.\nTests: tools/performance_regressions.py --only renderer bg-recycle integration. Actual scene regression fixture is tests/fixtures/m1-bg-before-tick3491-104.bin, captured before104 tick3491 BG prepare; no emulator state injection.\nSnapshot tools require matching current headers and ELF. ASan/UBSan ran with ASAN_OPTIONS=detect_leaks=0 because LSAN is unsupported under executor ptrace.\nHistorical104 profile-b and new read-only104 diagnosis establish causes. No105 CPU timing claim is inferred from those104 measurements. Final105 intervals are new commit timestamps.\n')
    agents=R/'AGENTS.md';s=agents.read_text();entry=f'- Latest checkpoint2026-11-02 v0.5.105: M1 stutter reproduced and mitigated. Samecontroller36000LCD23.214->23.316FPS; M1 ticks10..5200 qualifying pairs4730->4728, >=100ms50->33, >=8LCD24->9, p99100.46->83.71ms. Worst268ms remains; tick3104 worsened6->14LCD. BG stable Y origin/scroll-only, HUD exact bounded arithmetic/row counts, per-rectangle palette mapping/memo, cache-preserving packed fallback. No additional visual/audio sacrifice. M1clear->M2partial, game/audio/clipped0; white10/35860 late11; second-demo scene4tick1120x0b1e0000 persists. Four exactBG/HUD/OBJVRAM/OAM/palette snapshots, renderer/integration/realBGfixture and ASanUBSan passed(LSAN disabled: ptrace). Tutorial/pause/linkcancel pass. EWRAM308free,IWRAM120beforestack, observedmain2640IRQ272. ROM{ROM}. Target30NOTmet; no fullcampaign/M2clear/heli defeat/physical2P claim. Current docs/checkpoint-0.5.105.ko.md and docs/frame-pacing-0.5.105/. Continue coldBG/HUD spikes and30FPS; tests/ROM/input-only validation authorized.\n\n'
    if 'Latest checkpoint2026-11-02 v0.5.105:' not in s:agents.write_text(s.replace('# Source-based native port\n\n','# Source-based native port\n\n'+entry,1))
    readme=R/'README.ko.md';s=readme.read_text()
    if '## v0.5.105 frame pacing' not in s:readme.write_text('## v0.5.105 frame pacing\n\nM1 long-frame frequency reduced; overall23.32FPS, nominal30 unmet. >=100ms50→33; worst268ms remains. Partial M1→M2 input-only validation; see docs/checkpoint-0.5.105.ko.md and docs/frame-pacing-0.5.105.ko.html.\n\n'+s)
    print(json.dumps(dict(performance=perf,pacing=pacing,memory=memory),ensure_ascii=False))

if __name__=='__main__': main()
