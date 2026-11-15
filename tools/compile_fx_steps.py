#!/usr/bin/env python3
"""Collapse only side-effect-free WAIT/JUMP chains ending in DRAW.

All other commands retain the original animation interpreter. The table is
indexed by the existing semantic command number, not a source CPU address.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
def generate():
    source = (ROOT/'generated/boss-programs.c').read_text().split('static const struct AnimationCommand commands[]={',1)[1].split('};',1)[0]
    commands = [x.split(',') for x in re.findall(r'\{([^{}]+)\}',source)]
    plans=[]
    for start in range(len(commands)):
        at=start; delay=0; seen=set(); plan=(0,0,0,0,0)
        while at<len(commands) and at not in seen:
            seen.add(at)
            if len(seen)>512: break
            value,nxt,target,op,width,arg,pad=commands[at]
            nxt,target,arg=int(nxt),int(target),int(arg)
            if op=='AN_WAIT':
                if not delay: delay=arg
                at=nxt
            elif op=='AN_JUMP': at=target
            elif op=='AN_DRAW' and nxt<len(commands) and arg<=1:
                flags=128|arg|(2 if commands[nxt][3]=='AN_END' else 0)
                plan=(target,at,nxt,delay,flags);break
            else: break
        plans.append('{'+','.join(map(str,plan))+'}')
    target=ROOT/'generated/fx-steps.inc'; text=',\n'.join(plans)+'\n'
    if not target.exists() or target.read_text()!=text: target.write_text(text)
    print(f'{len(plans)} FX plans, {sum(not p.endswith(",0}") for p in plans)} eligible')
if __name__=='__main__': generate()
