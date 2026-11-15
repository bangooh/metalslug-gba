#!/usr/bin/env python3
"""Publish matching v109 ROM-budget evidence; preserve rejected experiments explicitly."""
import base64,csv,difflib,hashlib,json,re,shutil,struct,wave,zipfile
from pathlib import Path
from report_reference_106 import interval,copy_files
R=Path(__file__).resolve().parents[1];B=R/'build/rom109';C=B/'audio-only';D=R/'docs/rom-budget-0.5.109'
def read(p):return json.loads(p.read_text())
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 D.mkdir(exist_ok=True);old=read(R/'generated/play-validation-0.5.108.json');build=read(C/'build.json');summary=read(C/'summary.json')
 assert all(build[k]==read(R/'generated/native-build.json')[k]for k in ['rom_sha256','elf_sha256','fingerprint'])
 assert sha(C/'rom.gba')==sha(Path(build['rom']))==build['rom_sha256']
 assert sha(C/'native.elf')==sha(Path(build['elf']))==build['elf_sha256']
 assert summary['exit_code']==0 and not summary['invalid_benchmark_reasons']
 trace=[json.loads(l)for l in (C/'trace.jsonl').read_text().splitlines()];last=trace[-1]
 assert not any(last[k]for k in ['app.error','audio.fault','audio.clipped'])
 def active(path):
  rows=list(csv.DictReader((path/'commits.csv').open()));rows=[r for r in rows if r['playing']=='1' and r['presentation']=='0' and r['demo']=='0'];return rows[next(i for i,r in enumerate(rows)if int(r['live0'])):]
 rows=active(C);baseline_rows=active(R/'build/aggressive108/g2')
 perf=dict(overall=summary['active_play'],m1_active=interval([r for r in rows if r['scene']=='0']),m2_observed=interval([r for r in rows if r['scene']=='1']),white_frames=summary['execution']['all_white_game_frames'],game_lcd_frames=summary['execution']['game_lcd_frames'],late_transfers=last['video.late_transfers'],bg_backend='Conditional exact4bpp/BG HUD,1commit each. Normal8bpp/OBJ HUD. Latched occlusion retired when conservative cell bound leaves64tile headroom; cache retained.')
 matched={name:interval([r for r in rr if 8000<=int(r['camera_x'])<=24000])for name,rr in [('baseline',baseline_rows),('candidate',rows)]}
 transitions=[];previous=None;culling=[];previous_cull=None
 for t in trace:
  state=(t['session.state'],t['scene.scene'],t['playing'],t['presentation'])
  if state!=previous:transitions.append({k:t[k]for k in ['lcd_frame','world.ticks','world.results.progress','world.weapons.camera_x','session.state','scene.scene','playing','presentation']});previous=state
  if t['video.backgrounds.culling']!=previous_cull:culling.append({k:t[k]for k in ['lcd_frame','world.ticks','world.weapons.camera_x','video.backgrounds.culling']});previous_cull=t['video.backgrounds.culling']
 assert any(t['session.state']==2 and t['scene.scene']==0 for t in transitions)
 assert any(t['session.state']==1 and t['scene.scene']==1 and t['world.results.progress']==1 for t in transitions)
 pacing=read(B/'pacing/measurement.json');audio=read(B/'audio/verification.json');frontend=read(B/'frontend/summary.json')
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
 for n in ['audio-host.log','audio-final-asan.log']:
  s=(B/n).read_text();assert 'PASS' in s and 'ERROR:' not in s and 'runtime error:' not in s
 assert sha(R/'generated/player.tiles')=='ca7febc19eeda7cf62af36f036cdb91f290bf8be8ac279344e03bf269c74be32'
 candidates={n:read(B/n/'summary.json')for n in ['a','b']}
 packed=read(R/'generated/native-audio-runtime.json');pcm=read(R/'generated/native-audio-pcm.json')
 assert packed['removed_duplicate_ima_bytes']==817720 and pcm['assets']==168
 saving=old['memory']['rom_bytes']-build['rom_bytes']
 changes=['Remove the IMA copies of168 non-looping SFX already present as exact PCM. Keep original IMA offline for reference; ROM links only compact remaining IMA and offset table.', 'Validate compact IMA/PCM byte ranges on playback admission; no sample rate/voice/volume/quality change. Runtime output exactly matches the original reference.']
 report=dict(old);report.update(version='0.5.109',rom_sha256=build['rom_sha256'],elf_sha256=build['elf_sha256'],fingerprint=build['fingerprint'],memory=memory,performance=perf,frame_pacing=pacing,scene_transitions=transitions,implemented_optimizations=changes,hardware_snapshots=hardware,frontend_results=frontend,catalog_decode=dict(assets=192,decoded_samples=13821728,pcm_fnv32='a2c8099b',faults=0,last_verified_version='0.5.109',rerun_this_revision=True),audible_output=dict(captured_this_revision=True,**summary['execution'],audio_fault=0,clipped=0,ring_verification=audio,limitation='20s emulator output captured; no exhaustive listening or physical GBA claim.'),comparison=dict(baseline_version='0.5.108',baseline_rom_sha256=old['rom_sha256'],baseline_performance=old['performance'],baseline_rerun=False,method=pacing['comparison'],same_camera_8000_24000=matched),regressions=dict(host='audio-host.log: differential1056000samples and whole192assetcatalog; source/runtime retained byte ranges verified by importer',asan_ubsan='audio-final-asan.log;1056000 exact samples. detect_leaks=0 for ptrace.'),rejected_candidates=candidates,experiment_candidates=candidates,occlusion_transitions=culling,target_fps=30,target_met=False,priority='No new FPS win. Keep freed~1.78MiB for a higher-impact background compositor/palette or object optimization. Full8x8 region backend, M1 logic/HUD, white transfers and fullcampaign/2P remain pending.',evidence='docs/rom-budget-0.5.109/',rom_budget_analysis=dict(capacity_bytes=33554432,used_bytes=build['rom_bytes'],free_bytes=memory['rom_free'],removed_duplicate_ima_bytes=817720,net_rom_saving_bytes=saving,implemented=True,extra_precompute_in_final=False))
 write(R/'generated/play-validation-0.5.109.json',report)
 copy_files(C,D/'play',['build.json','summary.json','execution.json','trace.jsonl','commits.csv','inputs.csv','entities.csv','probe.txt','symbols.txt','emulator.log'])
 copy_files(B/'pacing',D/'pacing',['measurement.json','baseline.csv','candidate.csv'])
 copy_files(B/'audio',D/'audio',['verification.json','observer.log'])
 copy_files(B/'frontend',D/'frontend',['summary.json'])
 for n in ['tutorial','pause','link-cancel','ranking','demo']:copy_files(B/'frontend'/n,D/'frontend'/n,['trace.jsonl','commits.csv','inputs.csv','execution.json','emulator.log'])
 from PIL import Image
 for n in [6428,8810,25000]:
  snap=B/f'snapshot-{n}';Image.open(next(snap.glob('frame-*.ppm'))).save(snap/'screen.png')
  copy_files(snap,D/f'snapshot-{n}',['comparison.txt','object-hardware.json','fixture.bin','fixed.bin','hud-active.bin','hud-count.bin','vram.bin','pal.bin','oam.bin','ewram.bin','screen.png','capture.log'])
 for n in ['a','b']:copy_files(B/n,D/f'candidate-{n}',['build.json','summary.json','trace.jsonl','commits.csv','inputs.csv'])
 for folder in ['candidate-a-src','candidate-b-src']:copy_files(B/folder,D/folder,[p.name for p in (B/folder).iterdir()if p.is_file()])
 copy_files(B/'unused-generated',D/'candidate-a-assets',['native-bg-fast-keys.h','native-bg-fast-keys.json','native-bg-fast-keys.u32','native-bg-fast-lookup.u32'])
 copy_files(B,D,['audio-host.log','audio-final-asan.log','renderer-host.log','audio-prefix-host.log','audio-prefix-boundary.log','audio-prefix-asan.log','audio-repack-final.log','fast-keys.log'])
 copy_files(R/'generated',D,['native-audio-runtime.json','native-audio-pcm.json'])
 shutil.copyfile(R/'generated/play-validation-0.5.109.json',D/'validation.json')
 with wave.open(str(D/'audio/output.wav'),'wb')as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(summary['execution']['audio_rate']);w.writeframes((C/'audio.s16le').read_bytes())
 patch=''
 for n in ['native_audio.c','native_audio_assets.S']:
  patch+=''.join(difflib.unified_diff((B/'baseline/src'/n).read_text().splitlines(True),(R/'src'/n).read_text().splitlines(True),fromfile='v108/src/'+n,tofile='v109/src/'+n))
 (D/'runtime-changes.patch').write_text(patch)
 delta=(perf['overall']['fps']/old['performance']['overall']['fps']-1)*100
 rows_html=''.join(f'<tr><td>{label}</td><td>{fps:.3f}</td><td>{rom/1048576:.3f}MiB</td><td>{note}</td></tr>'for label,fps,rom,note in [('108 기준',old['performance']['overall']['fps'],old['memory']['rom_bytes'],'비교 기준'),('109 최종: 중복 제거',perf['overall']['fps'],build['rom_bytes'],'용량 확보; FPS 개선 없음'),('A: 배경 조각 사전 계산',candidates['a']['active_play']['fps'],read(B/'a/build.json')['rom_bytes'],'미채택: 용량 대비 이득 미미'),('B: M1 음악 PCM 구간',candidates['b']['active_play']['fps'],read(B/'b/build.json')['rom_bytes'],'미채택: 전체 평균 회귀')])
 imgs=''.join('<figure><img width="480" src="data:image/png;base64,'+base64.b64encode((D/f'snapshot-{n}/screen.png').read_bytes()).decode()+f'"><figcaption>실제 LCD 요청 {n}: BG/FIX/OBJ 검증</figcaption></figure>'for n in [6428,8810,25000])
 page=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Metal Slug GBA0.5.109 ROM 공간 실험</title><style>body{{font:16px/1.65 system-ui;max-width:980px;margin:36px auto;padding:0 24px;background:#101923;color:#e6edf3}}h1,h2{{color:#8edddd}}table{{border-collapse:collapse;width:100%}}td,th{{padding:9px;border-bottom:1px solid #425163;text-align:left}}img{{max-width:100%;image-rendering:pixelated}}code{{overflow-wrap:anywhere}}</style><h1>0.5.109 · ROM 공간 확보와 사전 계산 비교</h1><p><b>ROM 공간 확보에는 성공했고, 추가 FPS 개선은 얻지 못했습니다.</b> 현재평균{perf['overall']['fps']:.3f}fps,108기준{old['performance']['overall']['fps']:.3f}fps 대비{delta:+.3f}%로 사실상 동일합니다.30fps는 미달입니다.</p><table><tr><th>후보</th><th>평균FPS</th><th>ROM</th><th>판정</th></tr>{rows_html}</table><p>각36,000LCD·약10분·동일한 상태 대응 입력 정책의 별도 주행. 완전히 같은 세계 상태나 반복 통계 실험은 아닙니다. 기준108은 저장된 측정을 재사용했습니다.</p><h2>실제 반영한 변경</h2><p>이미PCM으로 저장된168효과음의 중복IMA817,720바이트를 런타임 묶음에서 제거했습니다. 오프셋표·범위검사 추가를 포함한 ROM 순감소는{saving:,}바이트입니다. 현재{build['rom_bytes']:,}바이트({build['rom_bytes']/1048576:.3f}MiB), 여유{memory['rom_free']:,}바이트({memory['rom_free']/1048576:.3f}MiB). 원본IMA와카탈로그는소스/독립비교용으로보존하며최종ROM에서는링크제거됩니다.6000Hz·4SFX·샘플·볼륨·반복·응답타이밍은유지합니다.</p><h2>A: 배경 사전 계산</h2><p>M1의15개이미지와opening패치에서사용되는5,040종을전부포함해10,416종의위치·반사·빈조각키374,976개를사전생성했습니다. 조회표524,288B+키1,499,904B, 기존조회표제거를포함한순증약1.76MB입니다.전체8×8픽셀합성이나팔레트배치사전계산은아닙니다.실제루프에연결했고원본키·호스트RGB/우선순위검사를통과했지만FPS25.247로108과차이가거의없어최종ROM에서제외했습니다.</p><h2>B: 음악 디코딩 제거</h2><p>M1음악첫850,000샘플(1.7MB)을정확한PCM으로저장하고정상속도·이벤트없는구간은직접읽었습니다.구간끝에서원본IMA predictor/index로복귀합니다.두테스트시나리오5,132,832샘플에걸쳐구간경계·첫반복·템포/페이드·응답이원본과일치했고ASan/UBSan도통과했습니다.그러나M1평균23.637→23.751의작은개선에비해전체평균25.198로회귀하여제외했습니다.음질을줄인실험이아닙니다.</p><h2>최종 검증과 한계</h2><p>M1결과화면→M2일부,게임오류·오디오오류·클리핑0.192개전체음악/효과음13,821,728샘플해시a2c8099b유지.혼합/반복/페이드/응답1,056,000샘플독립비교및ASan/UBSan통과.세GPU캡처의BG/FIX/OBJ색·우선순위·VRAM/OAM/팔레트차이0.오디오링재시작불일치0.튜토리얼·일시정지복귀·랭킹·링크취소통과.두번째데모scene4tick112/error0x0b1e0000은기존실패유지.</p><p>M1{perf['m1_active']['fps']:.3f},M2관측{perf['m2_observed']['fps']:.3f}fps.흰화면{perf['white_frames']}/{perf['game_lcd_frames']}LCD,late{perf['late_transfers']}회. M1ticks10..5200≥100ms32→{pacing['candidate']['at_least_100ms']}회,p99{pacing['candidate']['p99_ms']:.2f}ms,최장{pacing['candidate']['max_ms']:.2f}ms. 성능개선완료나전캠페인평균으로해석하지않습니다.전캠페인·M2클리어·헬기격파·실기2P미검증.</p>{imgs}<h2>다음 작업</h2><p>남은1.78MiB는유지합니다.이번실험은조각키계산과음악일부의디코딩제거만으로주요병목을해결하지못함을보였습니다.실제8×8배경합성·팔레트검색을사전결과로대체하거나M1객체/HUD처리를줄이는연결이필요합니다.107의전체구역계획은여전히오프라인이며ROM미연결입니다.</p><p>EWRAM여유{memory['ewram_free']}B,IWRAM스택앞{memory['iwram_before_reserved_stack_free']}B.관측main{memory['observed_main_stack']}/4096,IRQ{memory['observed_irq_stack']}/2208B.75%·상하4+4·승인플레이어에셋유지.</p><p>ROM SHA256 <code>{build['rom_sha256']}</code></p><p>재현:tools/import_audio_pcm_cache.py→build_native.py→prepare_followthrough_probe.py→validate_heli_performance.py(mode8,36000LCD). 후보별로그·입력·패치·검증자료는evidence.zip에보존했습니다.</p></html>'''
 (R/'docs/rom-budget-0.5.109.ko.html').write_text(page)
 backlog=f'''# 0.5.109 남은 작업

중복IMA168효과음817,720B제거. 최종ROM순감소{saving:,}B, 크기{build['rom_bytes']:,}B, 여유{memory['rom_free']:,}B(약1.78MiB). 음질/샘플/반복/응답 유지.

추가FPS개선없음:108평균25.245→109 {perf['overall']['fps']:.3f}fps. M1{perf['m1_active']['fps']:.3f},M2관측{perf['m2_observed']['fps']:.3f}. 배경조각키사전계산A25.247, M1음악PCM구간B25.198로둘다제외.32MiB내남은공간을무의미한표로채우지않았다.

|우선순위|남은 작업|근거/완료 기준|
|---|---|---|
|1|M1 실제 배경합성·팔레트 사전 배치 또는 객체/HUD 비용 제거|조각키 일부 계산만 제거한A로는 이득 없음. 확보1.78MiB로 중요한실행중처리를대체하고실측|
|2|107 구역계획 ROM 연결|53이미지+659패치 오프라인. 기존에셋표현교체·동적경계·색배치·용량검증필요|
|3|흰 화면·긴 프레임·30fps|흰11/35816,late12. M1≥100ms35회,p9983.71ms,최장234.40ms;30미달|
|4|지속4bpp/BG HUD|주행각1커밋. 강제활성화미채택|
|5|두 번째 데모|scene4tick112/error0x0b1e0000미해결|
|6|캠페인·보스·엔딩|M1results→M2일부검증. M2이후·헬기격파·각보스/엔딩입력전용검증|
|7|실기2P·메모리·음향|EWRAM{memory['ewram_free']}B,IWRAM스택앞{memory['iwram_before_reserved_stack_free']}B. 두기기검증필요|
|8|나머지 원본 의미|48슬롯팔레트·과밀strip·combat overlay·59D1/120D5/318A3A4·YM정밀도·영구랭킹·언어/설정|

ROM `{build['rom_sha256']}`. 최종src/ROM/ELF는build/rom109/audio-only 36000LCD동결실행과정확일치. 보고서docs/rom-budget-0.5.109.ko.html,generated/play-validation-0.5.109.json. 전캠페인/실기2P완료아님.
'''
 (R/'docs/remaining-work-0.5.109.ko.md').write_text(backlog);(R/'docs/checkpoint-0.5.109.ko.md').write_text(backlog)
 entry=f'- Latest checkpoint2026-11-02 v0.5.109: compact runtime IMA removes168PCM-backed SFX duplicates817720B, netROM-{saving}B; ROM{build["rom_bytes"]}B,free{memory["rom_free"]}B(~1.78MiB). Exact192assets/13821728sampleshasha2c8099b; differential/ASan1056000samples,3GPUsnapshots/audio ring/UI pass. NO FPSwin:10825.245->109{perf["overall"]["fps"]:.3f},M1{perf["m1_active"]["fps"]:.3f},M2{perf["m2_observed"]["fps"]:.3f};white11,late12,>=100ms35,p9983.71,max234.40ms. CandidateA~1.76MB BGfragmentkeys25.247(negligible),B1.7MBexactM1PCMprefix25.198(regression),bothREJECTED. FinalnoextraBG/M1PCM. ROM{build["rom_sha256"]}; exactfrozenbuild/rom109/audio-only.107offlineplansstillNOTROMlinked. Preserve75percent4+4/playerSHA/6000Hz4SFX. M1results->M2partialonly;seconddemo0x0b1e0000/fullcampaign/physical2Ppending. Next actual8x8composition/palette or M1objects/HUD. See docs/checkpoint-0.5.109.ko.md,docs/remaining-work-0.5.109.ko.md. Tests/ROMauthorized.\n\n'
 for n in ['AGENTS.md','README.ko.md']:
  p=R/n;s=p.read_text()
  if not s.startswith('- Latest checkpoint2026-11-02 v0.5.109:'):p.write_text(entry+s)
 (D/'README.txt').write_text('Final109 is audio-only compact IMA; exact final ROM and ELF match frozen audio-only execution. CandidateA BGfragment key table and candidateB PCM prefix are NOT in final ROM. CandidateB tests are archived with its source; do not claim their PCM-prefix checks as a final feature. Original IMA remains source/reference data, gc-removed from runtime. Whole8x8 region backend not implemented. Baseline108 reused. No FPS win; no fullcampaign/physical2P proof.\n')
 out=R.parent/'release';out.mkdir(exist_ok=True)
 with zipfile.ZipFile(out/'metal-slug-gba-native-v0.5.109-evidence.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9)as z:
  for p in sorted(D.rglob('*')):
   if p.is_file():z.write(p,p.relative_to(R/'docs'))
 print(json.dumps(dict(version='0.5.109',fps=perf['overall']['fps'],net_rom_saving=saving,memory=memory,rom=build['rom_sha256']),ensure_ascii=False))
if __name__=='__main__':main()
