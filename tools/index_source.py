#!/usr/bin/env python3
"""Index the complete matching source; an indexed routine is NOT a port.

This is an implementation dependency inventory, not a test or progress score.
Only ROM-backed animation tables are collected; labels alone are not trusted.
"""
import argparse,hashlib,json,re,struct
from pathlib import Path
R=Path(__file__).resolve().parents[1]
SHA='1b8eee0891b6f8c837348e60ea2923ae229e0fb603d34ae62ae06131332f0d3d'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,required=True);a=ap.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes()
 if hashlib.sha256(raw).hexdigest()!=SHA:raise ValueError('P-ROM fingerprint mismatch')
 files=[];routines=[];labels={};tables=[]
 for f in sorted((a.source/'asm').rglob('*.s')):
  s=f.read_text();rel=str(f.relative_to(a.source));category=f.relative_to(a.source/'asm').parts[0]
  records=list(re.finditer(r'^\|  ([^\n]+?)\s+@ \$([0-9A-F]+)\s+\((\d+) B\)',s,re.M))
  files.append(dict(path=rel,category=category,sha256=hashlib.sha256(f.read_bytes()).hexdigest(),sections=len(records),status='indexed, not a completion claim'))
  for i,m in enumerate(records):
   name=m[1].strip();at=int(m[2],16);body=s[m.end():records[i+1].start() if i+1<len(records) else len(s)]
   calls=sorted(set(re.findall(r'\b(?:jsr|bsr(?:\.\w)?|jmp)\s+([^\s|]+)',body)))
   row=dict(address=at,name=name,path=rel,bytes=int(m[3]),calls=calls,status='unported_or_unmapped')
   routines.append(row);labels[name]=at
   if category=='player' and 'SpriteTbl' in name:
    offset=at if at<0x100000 else at-0x100000
    ptr=struct.unpack_from('>10I',raw,offset)
    if all(0x279000<=v<0x290000 for v in ptr):
     tables.append(dict(address=at,label=name,source=rel,streams=[list(ptr[:5]),list(ptr[5:])]))
 c_files=[dict(path=str(f.relative_to(a.source)),sha256=hashlib.sha256(f.read_bytes()).hexdigest(),status='source-level m68k wrappers; not native ARM C') for f in sorted((a.source/'src').rglob('*.c'))]
 for row in routines:
  targets=[]
  for target in row.pop('calls'):
   target=re.sub(r'\(pc\)$','',target)
   if target in labels:targets.append(labels[target])
   elif re.fullmatch(r'0x[0-9a-f]+\.[lw]',target):targets.append(int(target[2:-2],16))
  row['direct_calls']=sorted(set(targets))
 result=dict(prom_sha256=SHA,scope='All original source files, including all missions and ending variants',complete=False,
  limitations='Indexed sections include data and thunks. Counts do not measure semantic port completion. Indirect calls require separate typed handler mapping.',files=files,c_files=c_files,routines=routines)
 (R/'generated/source-index.json').write_text(json.dumps(result,indent=2)+'\n')
 # Verified through projectile constructors AND their original drawing data:
 # $9C4D4/$2F865C shotgun, $9C25E/$2F84AE flame, $9BEC2/$2F7AC6 rocket.
 # Decompiled fire-shell labels and the old port's weapon names are incorrect.
 (R/'generated/player-source-catalog.json').write_text(json.dumps(dict(prom_sha256=SHA,weapons=['pistol','shotgun','flame','rocket','hmg'],actors=['marco','tarma'],tables=tables,complete=False,status='Original table inventory; runtime action transitions and effects are separately implemented'),indent=2)+'\n')
 print(json.dumps(dict(assembly_files=len(files),m68k_c_files=len(c_files),sections=len(routines),player_tables=len(tables),unique_player_streams=len({v for t in tables for actor in t['streams'] for v in actor}),complete=False)))
if __name__=='__main__':main()
