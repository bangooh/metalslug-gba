#!/usr/bin/env python3
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from rom_support import elf_to_rom,fix_header
R=Path(__file__).resolve().parents[1]
def main():
 # NativePlayWorld is now the default cartridge target. Keep the historical
 # opening and isolated fixtures explicit so they cannot masquerade as it.
 if not any(x in sys.argv[1:] for x in ('--legacy-opening','--combat-fixture','--movement-fixture')):
  from build_native import main as full_build
  return full_build()
 if '--legacy-opening' in sys.argv:sys.argv.remove('--legacy-opening')
 p=argparse.ArgumentParser();p.add_argument('--cc',default=os.environ.get('ARM_CC','arm-none-eabi-gcc'));p.add_argument('--combat-fixture',action='store_true');p.add_argument('--movement-fixture',action='store_true');a=p.parse_args()
 assert not (a.combat_fixture and a.movement_fixture)
 cc=shutil.which(a.cc) or a.cc
 b=R/'build';b.mkdir(exist_ok=True);(R/'dist').mkdir(exist_ok=True)
 # Tiny diagnostic font is UI only; game artwork is untouched.
 font=ImageFont.load_default(size=8);buf=bytearray(96*32)
 for c in range(33,96):
  im=Image.new('1',(8,8));ImageDraw.Draw(im).text((0,-2),chr(c),font=font,fill=1)
  for y in range(8):
   for x in range(8):buf[c*32+y*4+x//2]|=int(bool(im.getpixel((x,y))))<<((x&1)*4)
 (b/'font.bin').write_bytes(buf)
 assets={'bg_tiles':'background.tiles','bg_map':'background.map','bg_palette':'background.pal','terrain_cells':'terrain.cells','terrain_shapes':'terrain.shapes','player_tiles':'player.tiles','player_palette':'player.pal','title_tiles':'title.tiles','title_maps':'title.maps','title_palette':'title.pal','mission_data':'mission.bin','mission_next':'mission.skip'}
 assets.update(audio_music='audio-music.bin',audio_music_tail='audio-music-tail.bin',audio_effects='audio-effects.bin')
 assets.update(combat_tiles='combat.tiles',combat_palettes='combat.pal')
 assets.update(actor_tiles='actor.tiles',actor_palettes='actor.pal')
 asm=[]
 for name,file in assets.items():asm += [f'.section .rodata.{name},"a",%progbits','.balign 4',f'.global {name}',f'{name}:',f'.incbin "generated/{file}"']
 asm+=['.section .rodata.font,"a",%progbits','.balign 4','.global hud_font','hud_font:','.incbin "build/font.bin"']
 (b/'assets.S').write_text('\n'.join(asm)+'\n')
 version=(R/'VERSION').read_text().strip();stem='metal-slug-gba-native-v'+version
 if not a.combat_fixture and not a.movement_fixture:stem+='-legacy-opening'
 if a.combat_fixture:stem+='-combat-fixture'
 if a.movement_fixture:stem+='-movement-fixture'
 flags=['-mcpu=arm7tdmi','-marm','-O2','-std=c11','-ffreestanding','-fno-builtin','-fno-strict-aliasing','-ffunction-sections','-fdata-sections','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-nostdlib']
 elf=R/'dist'/(stem+'.elf')
 if a.combat_fixture:flags.append('-DCOMBAT_FIXTURE')
 elif not a.movement_fixture:flags.append('-DNATIVE_MISSION')
 suffix='-combat' if a.combat_fixture else '-movement' if a.movement_fixture else ''
 subprocess.run([cc,*flags,'src/startup.S','src/main.c','src/game.c','src/mission.c','src/audio.c','src/hitbox.c','src/combat.c','src/actors.c','src/campaign.c','src/terrain_edits.c','generated/combat-assets.c','generated/actor-assets.c','build/assets.S','-Tsrc/linker.ld',f'-Wl,--gc-sections,-Map=build/game{suffix}.map','-lgcc','-o',str(elf)],cwd=R,check=True)
 rom=elf_to_rom(elf.read_bytes());fix_header(rom)
 rom[0xa0:0xac]=b'MSLUG NATIVE';rom[0xac:0xb0]=b'MSN1';rom[0xbd]=(-sum(rom[0xa0:0xbd])-0x19)&255
 rom.extend(b'\xff'*((-len(rom))%4));out=R/'dist'/(stem+'.gba');out.write_bytes(rom)
 nm=str(Path(cc).with_name('arm-none-eabi-nm'));symbols={}
 for line in subprocess.check_output([nm,'-S',str(elf)],text=True).splitlines():
  v=line.split()
  if len(v)==4 and v[3] in ('game','metrics','g_audio_debug','combat','actors','campaign','terrain_edits'):symbols[v[3]]=int(v[0],16)
 (b/f'symbols{suffix}.json').write_text(json.dumps(symbols,indent=2)+'\n')
 print(json.dumps(dict(rom=str(out),bytes=len(rom),sha256=hashlib.sha256(rom).hexdigest(),symbols=symbols)))
if __name__=='__main__':main()
