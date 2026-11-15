#!/usr/bin/env python3
"""Package the current, explicitly incomplete native checkpoint."""
import hashlib,json,shutil,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[1];g=R/'generated';version=(R/'VERSION').read_text().strip()
if version!='0.5.43':raise SystemExit('Implementation phase: use checkpoint_source.py; do not reuse 0.5.43 proofs for a new ROM.')
mission=json.loads((g/'mission-source.json').read_text())
integrated={1,4,5,0xb6,0x105,0x106,0xf9,0x12d,0x136,0xc0,0xac,0x125,0x13c,0x147}
status=dict(version=version,requested_scope='Title through M1 boss defeat and mission result',complete=False,
 executable_scope='Title/music plus original M1 opening events and native combat; explicitly stops at unsupported 08E/$0E8AA8',
 native_command_component='Source-tested and connected to gameplay camera/progress/enemy counts',
 implemented=['title start/return controls','GBA Direct Sound title/M1 music and 141 effects',
 '15 M1 layers flattened offline, exact palette over 396 columns, 4 terrain descriptors',
 'M1 events + native actor spawning; 14 template IDs with documented limits',
 'source-tested hit geometry and HP rules, pistol/knife/upward pistol, crate destruction',
 'reduced soldier/charger AI with source visuals, melee callbacks and grenade trajectories',
 'sign collision stamps, score food pickups, POW release/rescue/gift request/exit',
 'game logic outside VBlank; shared resident OBJ animation tiles'],
 adaptations=['reduced NPC AI/world physics','temporary one-way camera','temporary unlimited respawn',
 'F9 grenade variant only','typed crate/POW drops not instantiated as inventory pickups',
 'player action cadence/air lower-body animation incomplete','POW ledge hopping unported'],
 pending=['original title animation/attract flow','complete player actions, grenades, inventory, death/lives',
 'remaining 22 template IDs plus missing variants and child objects',
 'source scene/camera program and encounter gates','complete dynamic scenery/collision',
 '08E flying enemy, Metal Slug vehicle, M1 boss attacks/damage/destruction/clear',
 'POW results and mission end','input-only full title-to-boss ARM playthrough and full combat performance'],
 templates=[dict(id=t['id'],initializer=t['initializer'],status=('native adaptation integrated' if t['id'] in integrated else 'not integrated'),records=t['records']) for t in mission['templates']],
 source_primary='https://github.com/correo415415/mslug-decompiled',commit=mission['commit'])
status['opening_checks']=json.loads((R/'build/campaign-arm-validation.json').read_text())
status['actor_world_checks']=json.loads((R/'build/actor-world-oracle.json').read_text())
status['combat_fixture']={'scope':'Three fixed TEST crates; no M1 encounter or enemy/camera/boss integration',
 'unsupported':['crate body blocking','complete original player cadence/state scheduling','pickups and inventory','complete terrain physics'],
 'source_checks':'17502 pair checks including 5502 actual crate-defense melee cases',
 'arm_checks':'Five input-only fixture routes, 2220 host/ARM ticks; not a full playthrough'}
(g/'port-status.json').write_text(json.dumps(status,indent=2)+'\n')
manifest=dict(version=version,commit=mission['commit'],prom_sha256=mission['prom_sha256'],
 scope=status['executable_scope'],complete=False,palette_exact=True,runtime_pixel_composition=False,
 recorded_route_dependency=False,background_adaptation='Fixed-anchor offline flattening; no original parallax/camera equivalence',
 files={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(g.iterdir()) if f.is_file() and f.name!='provenance.json'})
(g/'provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
out=R.parent/'release';out.mkdir(exist_ok=True);stem='metal-slug-gba-native-v'+version
shutil.copyfile(R/'dist'/(stem+'.gba'),out/(stem+'.gba'))
shutil.copyfile(R/'dist'/(stem+'-combat-fixture.gba'),out/(stem+'-combat-fixture.gba'))
proofs={'source-oracle.json','mission-oracle.json','mission-arm.json','arm-validation.json','symbols.json','game.map','background-reference.png',
        'hitbox-oracle.json','combat-arm-validation.json','symbols-combat.json','game-combat.map','combat-art.png',
 'campaign-arm-validation.json','actor-world-oracle.json','actor-art.png','symbols-movement.json','game-movement.map'}
for row in status['opening_checks']:
 for frame in [400,800,1400,row['rescue_frame'],row['port_limit_frame']]:
  if frame:proofs.add(f"campaign-{row['mode']}-{frame}.png")
proofs.update(f'combat-{n}-{kind}.png' for n in range(5) for kind in ['attack','result'])
for name in ['title','start','scroll','jump']:
 from PIL import Image
 p=R/'build'/(name+'.ppm')
 if p.exists():Image.open(p).save(R/'build'/(name+'.png'));proofs.add(name+'.png')
with zipfile.ZipFile(out/(stem+'-source.zip'),'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for p in sorted(R.rglob('*')):
  if not p.is_file():continue
  rel=p.relative_to(R)
  if '__pycache__' in rel.parts or rel.parts[0]=='dist':continue
  if rel.parts[0]=='build' and rel.name not in proofs:continue
  if any(part.startswith('.') for part in rel.parts):continue
  z.write(p,Path('metal-slug-native')/rel)
print(json.dumps(dict(rom=str(out/(stem+'.gba')),source=str(out/(stem+'-source.zip')),
 fixture=str(out/(stem+'-combat-fixture.gba')),sha256=hashlib.sha256((out/(stem+'.gba')).read_bytes()).hexdigest(),complete=False)))
