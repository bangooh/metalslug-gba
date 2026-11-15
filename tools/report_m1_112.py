#!/usr/bin/env python3
"""Package matched actual Neo Geo / final GBA captures; no synthesized references."""
from pathlib import Path
from PIL import Image,ImageDraw
import csv,hashlib,json,shutil,html,base64
R=Path(__file__).resolve().parents[1];B=R/'build';O=R/'docs/m1-0.5.112';O.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
build=json.loads((B/'m1-release/build.json').read_text())
ref=Path('/workspace/scratch/779e6ec69435/neogeo-reference/mslug.zip')
ng={int(r[1]):r for r in csv.reader((B/'neogeo-m1-matched/reference-captures.csv').open()) if int(r[3])==1}
gba={int(r[1]):r for r in csv.reader((B/'m1-release/reference-captures.csv').open()) if int(r[3])==1}
spec=[('opening','미션 시작',0,128,'mission-start-banner','neogeo-m1-matched/matched-001080.png',300),
('destruction','헬기 구간: 무너진 유적 유지',1536,144,'first-helicopter-arena-opening-ruins-destroyed',None,None),
('scroll','수상 마을 진입',2336,144,'stilt-house-entry',None,None),
('scroll','마을 교전',2648,144,'village-arena',None,None),
('transition','마을 끝 물 배경',2880,144,'last-stilt-house-water-visible',None,None),
('transition','폭포: 배경색 복구',3328,112,'waterfall-train-wreck',None,None),
('transition','보스 앞 절벽',3648,32,'cliff-boss-approach',None,None),
('boss','보스 전투 진입',3848,32,'boss-arena-active',None,None),
('results','포로 집계 화면',3848,32,'boss-defeated-prisoner-results','neogeo-m1-aim/neogeo-013200.png',15180),
('results','MISSION 1 COMPLETE',3848,32,'mission-complete-banner','neogeo-m1-aim/neogeo-013500.png',15480)]
cases=[];sheet=Image.new('RGB',(980,len(spec)*368),'#151a22');draw=ImageDraw.Draw(sheet);sections=[]
trace={r['lcd_frame']:r for r in map(json.loads,(B/'m1-release/trace.jsonl').read_text().splitlines())}
for i,(phase,label,cx,cy,event,source,lcd) in enumerate(spec):
 if source is None:
  row=ng[cx];source='neogeo-m1-matched/matched-'+row[0].zfill(6)+'.png';assert int(row[2])==cy
  row=gba[cx];lcd=int(row[0]);assert int(row[2])==cy
 else:
  rows=list(csv.DictReader((B/source.split('/')[0]/'reference-state.csv').open()));n=int(Path(source).stem.split('-')[-1]); row=next(x for x in rows if int(x['frame'])==n);assert (int(row['camera_x']),int(row['camera_y']))==(cx,cy)
  t=trace[lcd];assert(t['scene.camera.x']>>16,t['scene.camera.y']>>16)==(cx,cy)
 ap=B/source;bp=B/f'm1-release/frame-{lcd:06}.ppm';a=Image.open(ap).convert('RGB');b=Image.open(bp).convert('RGB')
 names=[f'{i+1:02}-neogeo.png',f'{i+1:02}-gba.png'];a.save(O/names[0]);b.save(O/names[1])
 state={'scene':0,'camera_x':cx,'camera_y':cy,'event_state':event}
 case={'mission':0,'phase':phase,'title':label,'reference_kind':'neogeo_rom_capture','review':'passed','review_scope':'Static background coverage, major terrain alignment and categorical event progression. Not frame-exact actor/AI/animation equivalence.','unresolved_differences':[],
 'approved_adaptations':['75% assets; center crop 4+4 GBA pixels from 224-line Neo Geo view','Color reduction and reduced cosmetic effects authorized for GBA performance'],
 'excluded_from_match':['Different input routes, scores, ammunition and animation phase','Existing regional language/free-play and bottom HUD safe-row policy','HUD single OBJ palette and enemy palette pressure color approximation'],
 'neogeo':{'image':names[0],'sha256':sha(O/names[0]),'state':state,'capture_file':source,'bios':'euro-s1','emulator':'MAME 0.264'},
 'gba':{'image':names[1],'sha256':sha(O/names[1]),'state':state,'lcd_frame':lcd,'emulator':'mGBA 0.10.2'}};cases.append(case)
 av=a.resize((240,168),Image.Resampling.NEAREST).crop((0,4,240,164))
 draw.text((8,i*368+5),f'{i+1:02} {phase.upper()} | camera {cx},{cy} | Neo Geo 75% crop / GBA',fill='white')
 sheet.paste(av.resize((480,320),Image.Resampling.NEAREST),(0,i*368+32));sheet.paste(b.resize((480,320),Image.Resampling.NEAREST),(500,i*368+32))
 uri=lambda p:'data:image/png;base64,'+base64.b64encode(p.read_bytes()).decode()
 sections.append(f'<section><h2>{i+1:02}. {html.escape(label)}</h2><p>원본 좌표 {cx}, {cy} · {html.escape(event)}</p><div class="pair"><figure><figcaption>네오지오 원본 320×224</figcaption><img src="{uri(O/names[0])}"></figure><figure><figcaption>GBA 240×160 · LCD {lcd}</figcaption><img src="{uri(O/names[1])}"></figure></div></section>')
sheet.save(O/'comparison.png')
manifest={'version':build['version'],'scope':'M1 visual regression of listed backgrounds and event displays, not complete gameplay fidelity or performance acceptance','gba_rom_sha256':build['rom_sha256'],'neogeo_rom_sha256':sha(ref),'cases':cases}
(O/'reference-comparison.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
metrics=json.loads((O/'metrics.json').read_text());execinfo=json.loads((B/'m1-release/execution.json').read_text())
rows=''.join(f'<tr><td>{k}</td><td>{v["fps"]:.2f}</td></tr>' for k,v in metrics['m1_camera_ranges'].items())
body=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>M1 수정 검증 · v0.5.112</title><style>body{{font:16px/1.65 system-ui,sans-serif;background:#111822;color:#e4eaf1;max-width:1100px;margin:40px auto;padding:0 20px}}h1{{font-size:32px}}h2{{font-size:21px}}section{{border-top:1px solid #34404f;padding:18px 0}}.pair{{display:flex;gap:16px;align-items:start;flex-wrap:wrap}}figure{{margin:0;flex:1;min-width:260px}}img{{width:100%;image-rendering:pixelated}}figcaption,p{{color:#b9c6d5}}strong{{color:#ffe19b}}table{{border-collapse:collapse}}td,th{{text-align:left;padding:6px 24px 6px 0;border-bottom:1px solid #34404f}}code{{word-break:break-all}}</style>
<h1>M1 수정 검증 · v0.5.112-m1</h1><p>실제 네오지오 ROM 실행과 GBA 수정 ROM의 같은 카메라·주요 이벤트를 비교했습니다. 그림 생성이나 원본 화면 추정은 사용하지 않았습니다.</p>
<p><strong>M1 시작 → 헬기 → 마을 → 보스 격파 → 포로 집계 → MISSION 1 COMPLETE → M2 진입</strong>을 입력만으로 실행했습니다. 메모리·체력·카메라 강제 변경은 없습니다. 테스트는 자동 조작 1개 경로이며, 모든 무기·분기·실기·2P 검증을 뜻하지 않습니다.</p>
<h2>실행 결과와 남은 한계</h2><ul><li>M1 평균 {metrics['m1']['fps']:.2f}fps. <strong>30fps 목표 미달</strong>. 배경 누락이 있던 2층 버전의 속도와 동등 조건 비교가 아닙니다.</li><li>M1 오류 0, 흰 화면 0, VBlank 늦은 전송 0. M2 진입 후 흰 화면 2프레임·늦은 전송 2회가 남아 있습니다.</li><li>프레임 간격 중앙값 {metrics['frame_interval_ms']['median']:.2f}ms, p95 {metrics['frame_interval_ms']['p95']:.2f}ms, 최대 {metrics['frame_interval_ms']['max']:.2f}ms. 마을 교전의 끊김은 남아 있습니다.</li><li>배경 4층을 위해 M1 HUD를 캐시된 OBJ·15색으로 전환했습니다. 다른 미션은 기존 BG HUD입니다. 선수·탄환 색은 보호하며, 일반 적의 팔레트 부족 시 유사색을 합칩니다. 이번 경로의 최대 추가 RGB555 채널 차이는 9/31입니다.</li><li>배경 4bpp 양자화 오차는 직전 191색 중간 에셋 대비 평균 최대 채널 0.398/31, p99 3/31, 최대 7/31입니다. 원본 네오지오 전체 대비 오차 수치가 아닙니다.</li><li>원본과 GBA 입력, 점수, 탄약, 글자 움직임의 순간 위상은 다릅니다. GBA의 기존 지역 언어·무료 플레이·하단 HUD 한 줄 이동도 비교에서 구분했습니다.</li></ul>
<h2>적용한 수정</h2><ol><li>미리 완성한 누적 타일·맵으로 교체하여 새 배경 바인딩 때 이전 지형이 사라지던 문제 수정.</li><li>4bpp 배경 4층, VRAM 2개 타일 풀, 팔레트 사전 배치, 화면 가장자리 변경분만 갱신.</li><li>활성 미션에 따라 IWRAM 코드 교체. M2 복귀 후 기존 코드 바이트 일치 확인.</li><li>원본 0x52756 → 0x2C26의 높은 팔레트 뱅크를 사용하도록 수정하여 폭포 뒤 검은 띠 제거.</li><li>매 프레임 다시 만드는 GBA OAM에 종료된 글자 애니메이션도 재제출하여 시작·완료 문구 복구.</li></ol>
<h2>M1 카메라 구간별 평균</h2><p>각 좌표 범위에 들어온 첫 커밋부터 마지막 커밋까지의 경과 시간 기준입니다. 카메라가 되돌아오면 범위 간 측정 시간이 겹칠 수 있습니다. 결과 연출도 M1 전체 평균에 포함됩니다.</p><table><tr><th>원본 카메라 X 범위</th><th>fps</th></tr>{rows}</table>
<h2>비교 기준</h2><p>배경의 빈 영역·지형 연결·주요 배치와 진행 단계만 통과로 판정했습니다. 적 AI·피격 판정·모든 파괴 상태의 프레임 단위 동일성을 인증하지 않습니다. 원본은 MAME 0.264 euro-s1, GBA는 mGBA 0.10.2입니다. 아래 원본은 전체 320×224를 보존했습니다.</p>
{''.join(sections)}<section><h2>배포 식별</h2><p>ROM {build['rom_bytes']:,}바이트 ({build['rom_bytes']/1048576:.2f}MiB), 32MiB 이내.</p><p>SHA-256 <code>{build['rom_sha256']}</code></p><p>오디오 출력 {execinfo['audio_samples']:,} 샘플, 최고 진폭 {execinfo['audio_peak']}; 6kHz 소스 믹서·최대 효과음 4개 설정 유지. 출력 존재를 확인한 것이며 원본 음악 전체의 청취 동일성 검증은 아닙니다.</p></section></html>'''
(O/'M1-comparison-v0.5.112.html').write_text(body)
print(O)
