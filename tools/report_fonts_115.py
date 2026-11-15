"""Report actual cartridge runs, approved-font provenance and UI comparisons.

This is a UI readability review, not a whole-game reference-equivalence gate.
"""
import base64
import csv
import hashlib
import html
import json
import re
from pathlib import Path
from PIL import Image, ImageDraw

R=Path(__file__).resolve().parents[1]
B=R/'build'; O=R/'docs/m1-0.5.115'; O.mkdir(exist_ok=True)
def read(p): return json.loads(p.read_text())
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def trace(name): return [json.loads(s) for s in (B/name/'trace.jsonl').read_text().splitlines()]
def metrics(name):
    d=B/name; rows=list(csv.DictReader((d/'commits.csv').open()))
    a=[x for x in rows if x['playing']=='1' and x['presentation']=='0' and x['demo']=='0' and x['scene']=='0']
    a=a[next(i for i,x in enumerate(a) if int(x['live0'])):]
    def fps(q): return (int(q[-1]['commits'])-int(q[0]['commits']))/((int(q[-1]['lcd'])-int(q[0]['lcd']))*280896/16777216)
    t=trace(name)
    white=[list(map(int,x)) for x in csv.reader((d/'white-frames.csv').open())] if (d/'white-frames.csv').exists() else []
    return dict(m1_fps=fps(a), village_fps=fps([x for x in a if 2649<=int(x['camera_x'])<3000]),
                m1_white=sum(x[2]==0 for x in white),m2_white=sum(x[2]==1 for x in white),
                m1_late=max(int(x['late']) for x in a), error=max(x['app.error'] for x in t),
                final_scene=t[-1]['world.weapons.scene_number'],final_lcd=t[-1]['lcd_frame'],
                max_cosmetic_retries=max(x['video.objects.budget_retries'] for x in t))

build=read(B/'fonts115-candidate/build.json')
assert sha(B/'fonts115-candidate/rom.gba')==build['rom_sha256']
oldbuild=read(B/'crash114-final/build.json')
summary=dict(version='0.5.115-m1',rom_bytes=build['rom_bytes'],rom_sha256=build['rom_sha256'],
             rom_delta_bytes=build['rom_bytes']-oldbuild['rom_bytes'],
             before=metrics('crash114-final-baseline'),after=metrics('fonts115-m1'),
             burst_before=metrics('village114-final-9'),burst_after=metrics('fonts115-burst9'),
             memory=dict(ewram_bss=261328,ewram_data=4,ewram_free=812,iwram_free_before_reserved_stacks=16),
             fonts=read(O/'asset-validation.json'),
             limitations=['M1 mean remains below30FPS','Known M2 transition white/late frames remain',
              'Known original/GBA hut-state mismatch at camera2880 remains',
              'Whole campaign, physical GBA/link and nonzero live prisoner roster not revalidated',
              'Tutorial retains original arcade A/B/C/D labels; the GBA controls screen remains the control reference'])
for k in ['after','burst_after']:
    assert summary[k]['error']==0 and summary[k]['final_scene']==1 and summary[k]['m1_white']==0
for name in ['fonts115-tutorial','fonts115-go']:
    t=trace(name);assert not any(x['app.error'] for x in t)
assert any(x['presentation']==3 for x in trace('fonts115-tutorial'))
assert any(x['world.results.region']==1 and x['world.enemies.cutscene.playback_mode'] for x in trace('fonts115-go'))
# English roster data can be checked completely, although this M1 controller
# loses its rescued prisoners before the results screen.
s=(R/'generated/mission-result-data.c').read_text()
names=list(map(int,re.search(r'mission_name_bytes\[\]=\{([^}]+)',s)[1].split(',')))
extended={c for c in names[4292:10786] if not 32<=c<127}
# The original English name bank also uses Latin accent glyphs in FIX3xx.
# Preserve their source codes, rather than stripping accents from names.
assert extended=={0xb5,0xc4,0xc5,0xd0,0xd1,0xd2,0xd3,0xd4,0xd5}
summary['english_roster_bytes_checked']=10786-4292
summary['english_roster_preserved_extended_codes']=sorted(extended)
summary['tutorial']=dict(frames=8000,errors=0,completed=True,source_pages=44)
summary['go']=dict(frames=7000,errors=0,source_variants=3,art_frames=40,live_variant_verified=0)

def copy_image(src,name):
    Image.open(src).convert('RGB').save(O/name)
    return dict(image=name,sha256=sha(O/name),capture=str(src.relative_to(R)))

captures={
 'tutorial-ng':B/'neogeo-fonts115-full-tutorial/neogeo-002700.png',
 'tutorial-gba':B/'fonts115-tutorial/frame-000600.ppm',
 'result-ng':R/'docs/m1-0.5.114/09-neogeo.png',
 'result-before':R/'docs/m1-0.5.114/09-gba.png',
 'result-gba':B/'fonts115-m1/frame-014100.ppm',
 'complete-ng':R/'docs/m1-0.5.114/10-neogeo.png',
 'complete-gba':B/'fonts115-m1/frame-014340.ppm',
 'go-ng':B/'neogeo-fonts115-go-attack/neogeo-002700.png',
 'go-before':B/'fonts114-go/frame-001440.ppm',
 'go-gba':B/'fonts115-go/frame-001440.ppm',
 'score-ng':B/'neogeo-village114-stress/matched-002155.png',
 'score-before':B/'crash114-final-baseline/frame-004800.ppm',
 'score-gba':B/'fonts115-m1/frame-004800.ppm'}
review={name:copy_image(path,name+'.png') for name,path in captures.items()}
review['scope']='UI wording, shape and readability only; gameplay/RNG/damage phase differ. GO cameras NG554/GBA549; score popups NG1536/GBA1841. Both show500. No claim of whole-frame pixel equality.'
review['approved_adaptations']=['75% /4+4 crop','Approved FIX v17 and mission letters v22','User-authorized native score/GO redraw','Live source palette/timing retained']
review['review']='passed for shown UI scope only'
review['unverified_live_variants']=['GO slanted arrow','GO up arrow','nonzero prisoner names']
(O/'ui-reference-review.json').write_text(json.dumps(review,ensure_ascii=False,indent=2)+'\n')
(O/'validation.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')

def normalized(name):
    im=Image.open(O/(name+'.png')).convert('RGB')
    if im.size==(320,224): im=im.resize((240,168),Image.Resampling.NEAREST).crop((0,4,240,164))
    return im

# Exact captured pixels, magnified with nearest-neighbour only.
sheet=Image.new('RGB',(1000,710),'#17212b'); draw=ImageDraw.Draw(sheet)
draw.text((24,14),'BEFORE v0.5.114 | ACTUAL ROM CAPTURE',fill='white')
draw.text((524,14),'AFTER v0.5.115 | ACTUAL ROM CAPTURE',fill='white')
rows=[('HUD score / original repaired font','score-before','score-gba',(10,0,110,20),4),
      ('Pickup score / native 5 x 7 digits','score-before','score-gba',(87,72,116,94),7),
      ('GO / native 24 x 24 artwork','go-before','go-gba',(188,28,220,62),6)]
ys=[48,180,390]
for (label,b,a,box,factor),y in zip(rows,ys):
    draw.text((24,y),label,fill='white')
    for x,name in [(24,b),(524,a)]:
        im=normalized(name).crop(box);sheet.paste(im.resize((im.width*factor,im.height*factor),Image.Resampling.NEAREST),(x,y+28))
draw.text((24,660),'Same source palette, positions and animation timing. No runtime font rasterizer added.',fill='#b9c8d6')
sheet.save(O/'font-before-after-v0.5.115.png')

def uri(name):return 'data:image/png;base64,'+base64.b64encode((O/(name+'.png')).read_bytes()).decode()
def figure(name,label):return f'<figure><img src="{uri(name)}" alt="{html.escape(label)}"><figcaption>{html.escape(label)}</figcaption></figure>'
def pair(a,b,la,lb):return '<div class="pair">'+figure(a,la)+figure(b,lb)+'</div>'
before,after=summary['before'],summary['after']
page=f'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>영문 UI·점수·GO 글꼴 개선 — v0.5.115-m1</title><style>
body{{margin:0;background:#f3f5f8;color:#1b2632;font:16px/1.75 system-ui,sans-serif}}main{{max-width:1000px;margin:auto;padding:32px 24px}}h1{{font-size:30px;line-height:1.35}}h2{{margin-top:36px;font-size:23px}}.lead{{background:#e0efe9;padding:20px;border-left:5px solid #2d7964}}.pair{{display:grid;grid-template-columns:1fr 1fr;gap:24px}}img{{width:100%;image-rendering:pixelated}}figure{{margin:12px 0}}figcaption,small{{color:#556575;font-size:13px}}table{{border-collapse:collapse;width:100%;background:white}}th,td{{padding:10px;border-bottom:1px solid #d9e0e7;text-align:left}}code{{overflow-wrap:anywhere}}.note{{background:#fff0d9;padding:18px}}@media(max-width:650px){{.pair{{grid-template-columns:1fr}}}}</style><main>
<small>2026-11-03 · 실제 ARM 롬 + 원본 Neo Geo Euro 실행 화면</small>
<h1>영문 표기 복원과 작은 글자의 선명도 개선</h1>
<p class="lead">튜토리얼·보스 격파 후 결과 화면을 원본 영문으로 전환했습니다. 이전에 만든 HUD·결과용 FIX 글꼴과 MISSION COMPLETE 보정 글자를 복원하고, 게임 중 점수 숫자와 GO 화살표는 GBA 표시 크기에 맞춰 보정했습니다.</p>
{figure('font-before-after-v0.5.115','실제 수정 전·후 GBA 화면의 같은 영역을 정수 배율로 확대. 보간·샤프닝 없음.')}
<h2>변경 범위</h2><table><tr><th>대상</th><th>처리</th></tr>
<tr><td>HUD·작은 글자·결과 화면</td><td>v0.5.17 보정 FIX 640셀 복원. 보관된 이전 바이너리와 바이트 단위 일치.</td></tr>
<tr><td>MISSION COMPLETE</td><td>v0.5.22 원본 형태 보정 17개 문자 맵 복원. 원본 위치·색 전환·이동 유지.</td></tr>
<tr><td>획득 점수</td><td>0~9를 5×7 픽셀 획과 1픽셀 그림자로 정리. 기존 6픽셀 간격과 팝업 움직임 유지.</td></tr>
<tr><td>GO·화살표</td><td>24×24 기준으로 구멍과 윤곽 정리. 세 방향의 40프레임을 기존 FIX 코드에 사전 저장. 실제 M1에서는 오른쪽 방향 확인.</td></tr>
<tr><td>영문 선택</td><td>기본 region=1. 원본 영문 튜토리얼 44페이지, 결과 제목·NO PRISONER·포로 이름 데이터 선택. 원본 영문 엔딩 분기도 따르며 전체 캠페인 검증은 별도.</td></tr></table>
<p>캐릭터·배경의 75% 비율은 그대로입니다. 변경된 OBJ 원본 맵은 글자 27개뿐이며 나머지 9,364개는 변환 픽셀 해시가 같습니다. 배경 중간 데이터와 승인된 Marco 그래픽도 동일합니다.</p>
<h2>원본 영문 화면 대조</h2>
{pair('tutorial-ng','tutorial-gba','Neo Geo: 버튼 A 사격 안내','GBA: 같은 원본 영문 안내 + 기존 작은 글꼴')}
{pair('result-ng','result-gba','Neo Geo: RECAPTURED PRISONER / NO PRISONER','GBA: 원본 영문 제목·집계 문구')}
{pair('complete-ng','complete-gba','Neo Geo: MISSION 1 COMPLETE','GBA: 이전에 승인한 보정 글자 복원')}
{pair('go-ng','go-gba','Neo Geo: 원본 GO / camera554','GBA: 보정한 GO / camera549')}
{pair('score-ng','score-gba','Neo Geo: 500점 팝업 / camera1536','GBA: 500점 팝업 / camera1841')}
<p>GO와 점수 대조는 해당 글자의 형태·내용을 확인한 것입니다. 이동·전투 시점과 카메라가 달라 전체 화면 픽셀 일치나 게임 동작의 동등성을 의미하지 않습니다. 튜토리얼의 버튼 문자는 원본 아케이드 A/B/C/D 표기를 유지합니다. GBA 실제 조작은 별도의 CONTROLS 화면을 기준으로 합니다.</p>
<h2>폰트와 타일의 자원 비용</h2><p>이 롬에서는 폰트도 타일 그래픽입니다. 8×8·4bpp 타일 하나는 32바이트이고, 같은 글자 타일을 재사용하면 글자 수만큼 새 픽셀 데이터를 만들 필요가 없습니다. 실행 중 글꼴을 그리거나 축소하는 경로는 추가하지 않았습니다. 비용을 결정하는 것은 새로운 타일 수·팔레트 수·전송량·화면의 OBJ 수입니다.</p>
<table><tr><th>항목</th><th>v0.5.114</th><th>v0.5.115</th></tr><tr><td>롬 크기</td><td>{oldbuild['rom_bytes']:,} B</td><td>{build['rom_bytes']:,} B (+632 B)</td></tr><tr><td>M1 평균</td><td>{before['m1_fps']:.2f}fps</td><td>{after['m1_fps']:.2f}fps</td></tr><tr><td>마을 구간</td><td>{before['village_fps']:.2f}fps</td><td>{after['village_fps']:.2f}fps</td></tr><tr><td>RAM</td><td colspan="2">정적 사용량 동일. EWRAM 여유812 B, 예약 스택 전 IWRAM 여유16 B.</td></tr></table>
<p>같은 상태 기반 입력 컨트롤러로 측정했습니다. 글꼴 변경에 따른 처리량 차이는 작으며 성능 향상으로 판단하지 않습니다. 30fps 목표는 아직 충족하지 못합니다.</p>
<h2>실행 검증과 남은 제한</h2><ul><li>20,000 LCD프레임: M1 보스 종료·영문 결과·MISSION COMPLETE·M2 진입. 오류0, M1 흰 화면0.</li><li>24,000 LCD프레임: 이전 마을 오류 재현 공격 패턴9로 통과, M2 진입. 오류0.</li><li>8,000 LCD프레임: 영문 튜토리얼 종료 후 게임 진입. 오류0.</li><li>7,000 LCD프레임: 같은 위치에서 GO 반복 표시. 오류0.</li><li>Neo Geo 원본 실행과 글자·문구 대조. 게임 RAM·체력·카메라 주입 없이 버튼 입력만 사용.</li></ul>
<p class="note">기존 M2 전환의 흰 화면2프레임과 M1 마을 일부 오두막 상태 차이는 남아 있습니다. 전체 게임 원본 대조 게이트는 통과 상태로 올리지 않았습니다. 실제 GBA·물리 링크·전체 미션·포로가 남은 결과 이름 화면은 이번 검증 범위에 포함하지 않았습니다.</p>
<p><b>배포 롬:</b> metal-slug-gba-native-v0.5.115-m1.gba<br>{build['rom_bytes']:,}바이트 ({build['rom_bytes']/1e6:.2f}MB / {build['rom_bytes']/1048576:.2f}MiB)<br><small>SHA256 <code>{build['rom_sha256']}</code></small></p>
</main></html>'''
(O/'M1-fonts-English-v0.5.115.html').write_text(page)
print(json.dumps({k:summary[k] for k in ['rom_bytes','rom_delta_bytes','after','burst_after']},indent=2))
